/**
 * surge_coupling.c - Surge Coupling/Decoupling Network Implementation
 *
 * L6 Canonical Problems: Coupling network per IEC 61000-4-5,
 * decoupling network design, multi-stage protection coordination.
 *
 * Reference:
 *   IEC 61000-4-5 Annex A: CDN Design
 *   IEC 61000-4-4 Annex A: EFT Coupling Clamp
 *   Standler, Protection of Electronic Circuits from Overvoltages, 1989
 *   Montrose & Nakauchi, EMC and the Printed Circuit Board, 2004
 */

#include "surge_coupling.h"
#include "surge_waveform.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ==================================================================
 * L6: Coupling Network (IEC 61000-4-5)
 *
 * The coupling network injects the surge into the EUT while
 * protecting the surge generator from line voltage and the
 * auxiliary equipment from the surge.
 *
 * Line-to-line coupling:
 *   18 uF capacitor in series with each line (differential mode)
 *   Equivalent series impedance: ~40 uF + 10 ohm
 *
 * Line-to-ground coupling:
 *   9 uF capacitor + 10 ohm in series per line to ground
 *   Provides 12 ohm effective coupling impedance
 *
 * The coupling capacitor acts as a high-pass filter:
 *   fc = 1/(2*pi*R*C)
 *   For 18 uF + 10 ohm: fc ~ 0.9 Hz (<< surge spectrum, good)
 * ================================================================== */

int coupling_network_init(coupling_network_params_t *cn,
                          surge_coupling_mode_t mode)
{
    if (!cn) return SURGE_ERR_NULL_PTR;

    memset(cn, 0, sizeof(*cn));
    cn->mode = mode;

    switch (mode) {
        case SURGE_COUPLE_LINE_TO_LINE:
            /* 18 uF per line, differential coupling */
            cn->coupling_cap_line_uf = 18.0;
            cn->coupling_cap_ground_uf = 0.0;
            cn->series_resistance_ohm = 10.0;
            cn->decoupling_inductance_uh = 1500.0;  /* 1.5 mH */
            break;

        case SURGE_COUPLE_LINE_TO_GROUND:
            /* 9 uF + 10 ohm per line to ground */
            cn->coupling_cap_line_uf = 0.0;
            cn->coupling_cap_ground_uf = 9.0;
            cn->series_resistance_ohm = 10.0;
            cn->decoupling_inductance_uh = 1500.0;
            break;

        case SURGE_COUPLE_LINE_TO_LINE_PE:
            /* Combined differential and common mode */
            cn->coupling_cap_line_uf = 18.0;
            cn->coupling_cap_ground_uf = 9.0;
            cn->series_resistance_ohm = 10.0;
            cn->decoupling_inductance_uh = 1500.0;
            break;

        case SURGE_COUPLE_TELECOM_LONG:
            /* For telecom: gas tube coupling + 40 ohm */
            cn->coupling_cap_line_uf = 0.0;
            cn->coupling_cap_ground_uf = 0.0;
            cn->series_resistance_ohm = 40.0;
            cn->decoupling_inductance_uh = 5000.0;  /* Higher for telecom */
            break;

        case SURGE_COUPLE_EXTERNAL:
            /* External port: direct injection through 2 ohm */
            cn->coupling_cap_line_uf = 0.0;
            cn->coupling_cap_ground_uf = 0.0;
            cn->series_resistance_ohm = 2.0;
            cn->decoupling_inductance_uh = 1000.0;
            break;

        default:
            return SURGE_ERR_INVALID_WAVEFORM;
    }

    /* Compute residual coupling impedance at 10 kHz (surge corner freq) */
    cn->residual_coupling_impedance = cn->series_resistance_ohm;

    return SURGE_OK;
}

double coupling_transfer_magnitude(double f,
                                    const coupling_network_params_t *cn,
                                    double z_load)
{
    /* Transfer function magnitude of coupling network:
     *
     * |H(f)| = |Z_load / (Z_load + Z_coupling)|
     *
     * Z_coupling = R_series + 1/(j*w*C_effective)
     *
     * where C_effective is the series/parallel combination of
     * coupling capacitors depending on mode.
     */
    if (!cn || z_load <= 0.0) return 0.0;
    if (f < 0.0) return 0.0;

    /* Determine effective coupling capacitance */
    double c_eff_uf = 0.0;
    if (cn->coupling_cap_line_uf > 0.0 && cn->coupling_cap_ground_uf > 0.0) {
        c_eff_uf = cn->coupling_cap_line_uf + cn->coupling_cap_ground_uf;
    } else if (cn->coupling_cap_line_uf > 0.0) {
        c_eff_uf = cn->coupling_cap_line_uf;
    } else if (cn->coupling_cap_ground_uf > 0.0) {
        c_eff_uf = cn->coupling_cap_ground_uf;
    } else {
        /* DC coupled: H = Z_load / (Z_load + R_series) */
        if (z_load + cn->series_resistance_ohm > 0.0)
            return z_load / (z_load + cn->series_resistance_ohm);
        return 0.0;
    }

    double w = 2.0 * M_PI * f;
    double c_eff_f = c_eff_uf * 1e-6;  /* Convert uF to F */

    /* |Z_coupling| = sqrt(R^2 + 1/(wC)^2) */
    double z_coupling_sq = cn->series_resistance_ohm * cn->series_resistance_ohm;
    if (w > 0.0 && c_eff_f > 0.0) {
        z_coupling_sq += 1.0 / (w * w * c_eff_f * c_eff_f);
    }

    return z_load / sqrt(z_load * z_load + z_coupling_sq);
}

double coupling_bandwidth_hz(const coupling_network_params_t *cn,
                              double z_load)
{
    /* -3dB cutoff frequency of the coupling high-pass filter:
     * fc = 1/(2*pi*R_coupling*C_coupling)
     *
     * The coupling network is essentially a first-order high-pass.
     * For good surge coupling: fc << f_surge_corner.
     */
    if (!cn || z_load <= 0.0) return 0.0;

    double c_eff_uf = 0.0;
    if (cn->coupling_cap_line_uf > 0.0) c_eff_uf += cn->coupling_cap_line_uf;
    if (cn->coupling_cap_ground_uf > 0.0) c_eff_uf += cn->coupling_cap_ground_uf;

    if (c_eff_uf <= 0.0) return 0.0;  /* DC coupled, infinite bandwidth */

    double c_eff_f = c_eff_uf * 1e-6;
    double r_eff = cn->series_resistance_ohm + z_load;

    if (r_eff <= 0.0 || c_eff_f <= 0.0) return 0.0;

    return 1.0 / (2.0 * M_PI * r_eff * c_eff_f);
}

/* ==================================================================
 * L6: Decoupling Network
 * ================================================================== */

double decoupling_inductance(double z_source, double f_corner,
                              double impedance_ratio)
{
    /* L = Z_target / (2*pi*f_corner)
     *
     * where Z_target = Z_source * impedance_ratio
     *
     * For surge decoupling:
     *   Z_source = 2 ohm (IEC combination wave)
     *   f_corner = 10 kHz (1.2/50us surge spectrum)
     *   impedance_ratio = 50 (for 40 dB attenuation)
     *   Z_target = 100 ohm
     *   L = 100 / (2*pi*10e3) = 1.59 mH
     *
     * Standard value: 1.5 mH per IEC 61000-4-5
     */
    if (z_source <= 0.0 || f_corner <= 0.0 || impedance_ratio <= 0.0)
        return 0.0;

    double z_target = z_source * impedance_ratio;
    return z_target / (2.0 * M_PI * f_corner);
}

double decoupling_attenuation_db(double f, double l_decouple_uh,
                                  double z_source)
{
    /* Attenuation = 20 * log10(1 + |Z_L| / Z_source)
     * where Z_L = 2 * pi * f * L is the inductive impedance.
     *
     * For f = f_corner (10 kHz), L = 1.5 mH:
     *   |Z_L| = 2*pi*10e3*1.5e-3 = 94.2 ohm
     *   Attenuation = 20*log10(1 + 94.2/2) = 20*log10(48.1) = 33.6 dB
     *
     * This means the surge voltage at the auxiliary equipment is
     * reduced by a factor of 48, providing effective protection.
     */
    if (f < 0.0 || l_decouple_uh < 0.0 || z_source <= 0.0) return 0.0;

    /* For DC (f=0), no attenuation from inductor */
    if (f == 0.0) return 0.0;

    double z_l = 2.0 * M_PI * f * l_decouple_uh * 1e-6;  /* uH to H */
    double ratio = 1.0 + z_l / z_source;

    if (ratio <= 0.0) return 0.0;

    return 20.0 * log10(ratio);
}

int coupling_verify_waveform(const coupling_network_params_t *cn,
                             const surge_waveform_params_t *wf,
                             double *peak_error_pct,
                             double *rise_error_pct,
                             double *width_error_pct)
{
    if (!cn || !wf || !peak_error_pct || !rise_error_pct || !width_error_pct)
        return SURGE_ERR_NULL_PTR;

    /* Verify that coupling network does not distort surge beyond IEC tolerance.
     *
     * IEC 61000-4-5 tolerances:
     *   - Front time: +/- 30%
     *   - Duration:   +/- 20%
     *   - Peak:       +/- 10%
     *
     * The coupling network introduces a high-pass characteristic that
     * can affect low-frequency content (waveform tilt/sag).
     *
     * For the standard IEC coupling network (18 uF + 10 ohm):
     *   fc = 0.9 Hz << surge spectrum (10 kHz+)
     *   Distortion is negligible (< 1%)
     */

    /* Effective time constant of coupling network */
    double c_eff_uf = 0.0;
    if (cn->coupling_cap_line_uf > 0.0) c_eff_uf += cn->coupling_cap_line_uf;
    if (cn->coupling_cap_ground_uf > 0.0) c_eff_uf += cn->coupling_cap_ground_uf;

    if (c_eff_uf <= 0.0) {
        /* DC coupling: no distortion from capacitor */
        *peak_error_pct = 0.0;
        *rise_error_pct = 0.0;
        *width_error_pct = 0.0;
        return SURGE_OK;
    }

    /* Time constant of coupling network */
    double tau_coupling = cn->series_resistance_ohm * c_eff_uf * 1e-6; /* seconds */

    /* Surge duration in seconds */
    double tau_surge = wf->t_duration * 1e-6;  /* us to seconds */

    /* If tau_coupling >> tau_surge, coupling is essentially flat */
    double ratio = tau_coupling / tau_surge;

    if (ratio > 100.0) {
        /* Waveform passes with negligible distortion */
        *peak_error_pct = 0.0;
        *rise_error_pct = 0.0;
        *width_error_pct = 0.0;
    } else if (ratio > 10.0) {
        /* Small droop on waveform tail */
        *peak_error_pct = 1.0 / ratio * 100.0;
        *rise_error_pct = 0.5 / ratio * 100.0;
        *width_error_pct = 2.0 / ratio * 100.0;
    } else {
        /* Significant distortion - coupling network inadequate */
        *peak_error_pct = 10.0 / ratio * 100.0;
        *rise_error_pct = 5.0 / ratio * 100.0;
        *width_error_pct = 20.0 / ratio * 100.0;
        return SURGE_ERR_INVALID_IMPEDANCE;
    }

    return SURGE_OK;
}

/* ==================================================================
 * L6: Multi-Stage Protection Coordination
 * ================================================================== */

static double estimate_coarse_clamp(protection_device_type_t type,
                                     double v_nominal, double i_surge) {
    switch (type) {
        case PROT_GDT:
            return 15.0 + 0.01 * i_surge;  /* Arc voltage */
        case PROT_SGAP:
            return 25.0;  /* Spark gap arc voltage */
        case PROT_MOV:
            return v_nominal * pow(i_surge / 0.001, 1.0/30.0);
        default:
            return v_nominal;
    }
}

int multistage_protection_design(multistage_protection_t *mp,
                                  const surge_waveform_params_t *wf,
                                  double v_line_max, double v_withstand)
{
    if (!mp || !wf) return SURGE_ERR_NULL_PTR;
    if (v_line_max <= 0.0 || v_withstand <= 0.0) return SURGE_ERR_INVALID_WAVEFORM;

    memset(mp, 0, sizeof(*mp));

    double i_surge = wf->i_peak > 0.0 ? wf->i_peak :
                     wf->v_peak / wf->source_impedance;

    /* Stage 1: Coarse protection (GDT for high energy, MOV for medium) */
    if (i_surge > 2000.0) {
        /* Use GDT for very high currents */
        mp->num_stages = 1;
        surge_select_gdt(&mp->devices[0], v_line_max, i_surge, v_withstand);
        mp->stage_types[0] = PROT_STAGE_COARSE;
    } else if (i_surge > 100.0) {
        /* Two-stage: MOV + TVS */
        mp->num_stages = 2;

        /* Stage 1: MOV */
        surge_select_mov(&mp->devices[0], v_line_max, i_surge * 0.8, v_withstand);
        mp->stage_types[0] = PROT_STAGE_COARSE;

        /* Stage 2: TVS for precision clamping */
        double i_residual = i_surge * 0.2;  /* 80% absorbed by MOV */
        surge_select_tvs(&mp->devices[1], v_line_max * 0.9, i_residual,
                        v_withstand, 1);
        mp->stage_types[1] = PROT_STAGE_FINE;

        /* Decoupling between stages: typical 5-10 ohm + 10-50 uH */
        mp->decoupling_R[0] = 5.0;   /* 5 ohm */
        mp->decoupling_L[0] = 20.0;  /* 20 uH */

    } else {
        /* Single-stage TVS for low-energy surges */
        mp->num_stages = 1;
        surge_select_tvs(&mp->devices[0], v_line_max, i_surge,
                        v_withstand, 1);
        mp->stage_types[0] = PROT_STAGE_FINE;
    }

    /* Compute residual voltage and energy distribution */
    multistage_energy_distribution(mp, wf);

    return SURGE_OK;
}

int multistage_energy_distribution(multistage_protection_t *mp,
                                    const surge_waveform_params_t *wf)
{
    if (!mp || !wf) return SURGE_ERR_NULL_PTR;
    if (mp->num_stages < 1 || mp->num_stages > 5)
        return SURGE_ERR_INVALID_WAVEFORM;

    double total_energy = wf->energy_per_pulse;
    double i_surge = wf->i_peak > 0.0 ? wf->i_peak :
                     wf->v_peak / wf->source_impedance;

    /* Simplified energy distribution:
     * Stage 1 absorbs the bulk, subsequent stages handle residual.
     * The energy division depends on relative clamping voltages and
     * decoupling impedances.
     */
    if (mp->num_stages == 1) {
        mp->stage_energy_fraction[0] = 1.0;
        mp->total_energy_absorbed = total_energy;
        mp->residual_v_at_load = mp->devices[0].v_clamp_rated;
    } else {
        /* Multi-stage: energy divides per impedance ratio */
        double remaining_energy = total_energy;
        double remaining_i = i_surge;
        int s;

        for (s = 0; s < mp->num_stages; s++) {
            /* Each stage absorbs energy proportional to its V_clamp * I */
            double v_clamp = estimate_coarse_clamp(mp->devices[s].type,
                                                    mp->devices[s].v_breakdown_max,
                                                    remaining_i);
            double energy_this_stage = v_clamp * remaining_i *
                                        (wf->t_duration * 1e-6);
            /* Ensure energy doesn't exceed remaining */
            if (energy_this_stage > remaining_energy)
                energy_this_stage = remaining_energy;

            mp->stage_energy_fraction[s] = energy_this_stage / total_energy;
            remaining_energy -= energy_this_stage;

            /* Current reduction through decoupling */
            if (s < mp->num_stages - 1) {
                double z_decouple = mp->decoupling_R[s] +
                    2.0 * M_PI * 10000.0 * mp->decoupling_L[s] * 1e-6;
                remaining_i *= mp->devices[s].v_clamp_rated /
                               (mp->devices[s].v_clamp_rated + z_decouple);
            }
        }

        mp->total_energy_absorbed = total_energy;
    }

    return SURGE_OK;
}

int multistage_verify_coordination(const multistage_protection_t *mp,
                                    double v_withstand)
{
    if (!mp) return SURGE_ERR_NULL_PTR;
    if (v_withstand <= 0.0) return SURGE_ERR_INVALID_WAVEFORM;

    if (mp->num_stages < 1 || mp->num_stages > 5)
        return SURGE_ERR_COORDINATION_FAIL;

    /* Check voltage handoff between stages */
    int s;
    for (s = 0; s < mp->num_stages - 1; s++) {
        double v_clamp_this = mp->devices[s].v_clamp_rated;
        double v_breakdown_next = mp->devices[s + 1].v_breakdown_min;

        /* Stage N+1 must trigger before stage N's clamp voltage reaches it */
        if (v_breakdown_next > v_clamp_this * 1.2) {
            return SURGE_ERR_COORDINATION_FAIL;
        }
    }

    /* Check each stage is not overstressed */
    for (s = 0; s < mp->num_stages; s++) {
        double energy_rating = mp->devices[s].energy_rating;
        double energy_absorbed = mp->stage_energy_fraction[s] *
                                  mp->total_energy_absorbed;
        if (energy_absorbed > energy_rating) {
            return SURGE_ERR_OVER_STRESS;
        }
    }

    /* Check final residual voltage */
    if (mp->residual_v_at_load > v_withstand) {
        return SURGE_ERR_PROTECTION_FAIL;
    }

    return SURGE_OK;
}

double multistage_residual_voltage(const multistage_protection_t *mp,
                                    double i_residual)
{
    if (!mp || mp->num_stages < 1) return 0.0;

    /* Residual = clamping voltage of last stage + I*R drop from wiring */
    double v_last = mp->devices[mp->num_stages - 1].v_clamp_rated;

    /* Add resistive drop in decoupling elements */
    double r_total = 0.0;
    int s;
    for (s = 0; s < mp->num_stages - 1; s++) {
        r_total += mp->decoupling_R[s];
    }

    return v_last + i_residual * r_total;
}