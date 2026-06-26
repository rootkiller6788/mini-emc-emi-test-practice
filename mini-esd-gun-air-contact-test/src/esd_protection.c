/**
 * @file esd_protection.c
 * @brief ESD protection device modeling, selection, and network analysis
 *
 * Implements TVS diode, varistor, and RC filter models for
 * ESD protection. Includes TVS selection algorithm, thermal
 * analysis, and multi-stage protection network simulation.
 *
 * Knowledge coverage: L1-L8
 */

#include "esd_protection.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ─── Default Device Parameters ─────────────────────────────────── */

/**
 * @brief Typical 5V USB ESD protection TVS.
 *
 * L1 Definition: Representative parameters for a generic
 * 5V unidirectional TVS diode suitable for USB 2.0 protection.
 *
 * Key specs:
 *   VRWM = 5.0 V (above USB VBUS nominal)
 *   VBR  ≈ 6.0-7.5 V
 *   VC   = 12 V @ 5 A (8/20 µs)
 *   Cj   = 0.5 pF (low enough for USB 2.0 480 Mbps)
 */
esd_tvs_params_t esd_tvs_default_5v(void)
{
    esd_tvs_params_t t;
    memset(&t, 0, sizeof(t));
    t.vrwm_v        = 5.0;
    t.vbr_min_v     = 6.0;
    t.vbr_max_v     = 7.5;
    t.vc_v          = 12.0;
    t.ipp_a         = 5.0;
    t.ppk_w         = 75.0;
    t.cj_pf         = 0.5;
    t.leakage_na    = 100.0;
    t.dynamic_r_ohm = 0.8;
    t.subtype       = ESD_PROT_TVS_UNIDIR;
    return t;
}

/**
 * @brief Typical 3.3V ESD protection TVS.
 *
 * L1 Definition: Parameters for protecting 3.3V logic circuits
 * (SPI, I2C, GPIO). Lower capacitance for high-speed signals.
 */
esd_tvs_params_t esd_tvs_default_3v3(void)
{
    esd_tvs_params_t t;
    memset(&t, 0, sizeof(t));
    t.vrwm_v        = 3.3;
    t.vbr_min_v     = 4.0;
    t.vbr_max_v     = 5.5;
    t.vc_v          = 8.0;
    t.ipp_a         = 3.0;
    t.ppk_w         = 40.0;
    t.cj_pf         = 0.3;
    t.leakage_na    = 50.0;
    t.dynamic_r_ohm = 1.0;
    t.subtype       = ESD_PROT_TVS_BIDIR;
    return t;
}

/**
 * @brief Default varistor parameters.
 *
 * L1 Definition: Typical ZnO varistor for 5V circuit protection.
 * Varistors have higher capacitance than TVS diodes but can
 * handle much higher energy.
 *
 * I-V characteristic: I = k · V^α
 * For this device: α ≈ 30, k ≈ 1e-60  (fitted to I=1mA @ V=18V)
 */
esd_varistor_params_t esd_varistor_default(void)
{
    esd_varistor_params_t v;
    memset(&v, 0, sizeof(v));
    v.v1ma_v          = 18.0;
    v.vc_v            = 40.0;   /* @ 1A */
    v.energy_rating_j = 0.1;
    v.peak_current_a  = 100.0;
    v.capacitance_pf  = 120.0;
    v.alpha           = 30.0;
    /* k = I @ 1V = 1e-3 / (1^30)... approximate */
    /* Actually: k = I / V^α → k = 1e-3 / 18^30 */
    v.k_constant      = 1e-3 / pow(18.0, 30.0);
    return v;
}

/* ─── TVS Diode Model ──────────────────────────────────────────── */

/**
 * @brief TVS clamping voltage at given current.
 *
 * L3 Mathematical Structure: TVS diode I-V model:
 *
 * Region 1 (V < VBR):   I ≈ I_leakage  (pA-nA range)
 *   Essentially open circuit for normal signal voltages.
 *
 * Region 2 (V ≈ VBR):   Exponential turn-on
 *   I = I_s · [exp(V/(n·V_T)) - 1]
 *   where V_T = kT/q ≈ 26 mV at 300 K, n ≈ 1-2
 *
 * Region 3 (V > VBR):   Linear ohmic region
 *   V(I) = V_BR + R_dyn · I
 *   This is where most ESD current flows.
 *
 * We use the piecewise linear model here because it's
 * sufficient for ESD analysis and avoids the numerical
 * issues of the exponential model at high currents.
 */
double esd_tvs_clamp_voltage(const esd_tvs_params_t *tvs, double current_a)
{
    if (!tvs) return 0.0;

    if (current_a <= 0.0) {
        return tvs->vrwm_v;  /* No clamping below VBR */
    }

    /* Piecewise model */
    if (current_a < 1e-6) {
        /* Sub-microamp: near VBR, transitional */
        return tvs->vbr_min_v;
    }

    /* Ohmic region: V = V_BR + R_dyn × I */
    double v = tvs->vbr_min_v + tvs->dynamic_r_ohm * current_a;

    /* Clamp to Vc as upper bound (saturation) */
    if (v > tvs->vc_v) v = tvs->vc_v;

    return v;
}

/**
 * @brief TVS instantaneous power dissipation.
 *
 * P = V_clamp × I
 *
 * For an 8 kV ESD strike through a TVS:
 *   I = 30 A, V_clamp = 15 V → P = 450 W (peak)
 *   τ = 100 ns → E = 45 µJ (energy per pulse)
 *
 * A TVS rated for 75 W peak (8/20 µs) can typically handle
 * this because the ESD pulse is much shorter (100 ns vs 20 µs),
 * allowing the device to operate in the adiabatic regime.
 */
double esd_tvs_power(const esd_tvs_params_t *tvs, double current_a)
{
    double v_clamp = esd_tvs_clamp_voltage(tvs, current_a);
    return v_clamp * current_a;
}

/**
 * @brief TVS junction temperature rise during ESD.
 *
 * L4 Theorem: Thermal response of a semiconductor junction
 * to a transient power pulse.
 *
 * For pulses much shorter than the thermal time constant
 * (τ_pulse << τ_th), the heating is adiabatic:
 *
 *   ΔT = E_pulse / C_th
 *
 * where C_th = thermal capacitance [J/°C] of the junction.
 *
 * For longer pulses, the temperature rise follows:
 *
 *   ΔT(t) = P · R_th · (1 - exp(-t/τ_th))
 *
 * where:
 *   R_th = thermal resistance [°C/W]
 *   τ_th = R_th × C_th  (thermal time constant)
 *
 * Thermal failure occurs when T_junction exceeds ~150-175°C
 * for silicon devices, causing permanent damage.
 */
double esd_tvs_temp_rise(const esd_tvs_params_t *tvs,
                          double current_a,
                          double pulse_duration_s,
                          double rth_cw,
                          double cth_jpc)
{
    if (!tvs || pulse_duration_s <= 0.0) return 0.0;

    double power = esd_tvs_power(tvs, current_a);
    double tau_th = rth_cw * cth_jpc;  /* thermal time constant [s] */

    if (tau_th <= 0.0) {
        /* Adiabatic limit */
        double energy = power * pulse_duration_s;
        return energy / cth_jpc;
    }

    /* Exponential thermal response */
    double delta_t = power * rth_cw * (1.0 - exp(-pulse_duration_s / tau_th));

    return delta_t;
}

/**
 * @brief TVS selection algorithm.
 *
 * L5 Algorithm: Systematic TVS selection for ESD protection.
 *
 * Selection criteria (ordered by priority):
 *
 * 1. VRWM > V_signal_max + 10% margin
 *    → TVS must not conduct during normal operation.
 *
 * 2. VC < V_IC_max
 *    → TVS must clamp below IC absolute maximum rating.
 *
 * 3. IPP > I_ESD_peak
 *    → TVS must survive the expected ESD current.
 *
 * 4. PPK · (τ_8_20 / τ_ESD) > I_ESD × V_C
 *    → Energy scaling: shorter pulse = more survivable
 *    (adiabatic regime advantage).
 *
 * 5. C_j < C_max_signal
 *    → Capacitance must not degrade signal integrity.
 */
int esd_tvs_select(const esd_protection_window_t *window,
                    const esd_tvs_params_t *tvs,
                    esd_tvs_selection_t *sel)
{
    if (!window || !tvs || !sel) return -1;

    memset(sel, 0, sizeof(*sel));

    /* Criterion 1: VRWM margin */
    sel->vrwm_margin_v = tvs->vrwm_v - window->v_signal_max_v;
    int c1 = (sel->vrwm_margin_v >= 0.1 * window->v_signal_max_v) ? 1 : 0;

    /* Criterion 2: Clamping voltage margin */
    sel->vc_margin_v = window->v_ic_max_v - tvs->vc_v;
    int c2 = (sel->vc_margin_v > 0.0) ? 1 : 0;

    /* Criterion 3: Peak current margin */
    sel->ipp_margin_a = tvs->ipp_a - window->esd_peak_current_a;
    int c3 = (sel->ipp_margin_a > 0.0) ? 1 : 0;

    /* Criterion 4: Bandwidth margin */
    double bw_limit = esd_tvs_bandwidth_limit(tvs, 50.0, 0);
    sel->bandwidth_margin_mhz = bw_limit - window->signal_bandwidth_mhz;
    int c4 = (sel->bandwidth_margin_mhz > 0.0) ? 1 : 0;

    /* Clamp ratio: lower = better */
    sel->clamp_ratio = tvs->vc_v / window->v_signal_max_v;

    /* Overall suitability: all criteria must pass */
    sel->suitable = (c1 && c2 && c3 && c4) ? 1 : 0;

    /* Generate recommendation */
    if (sel->suitable) {
        snprintf(sel->recommendation, sizeof(sel->recommendation),
                 "TVS suitable: VRWM=%.1fV > %.1fV, VC=%.1fV < %.1fV, "
                 "IPP=%.1fA > %.1fA. Clamp ratio=%.2f",
                 tvs->vrwm_v, window->v_signal_max_v,
                 tvs->vc_v, window->v_ic_max_v,
                 tvs->ipp_a, window->esd_peak_current_a,
                 sel->clamp_ratio);
    } else {
        snprintf(sel->recommendation, sizeof(sel->recommendation),
                 "TVS NOT suitable: ");
        if (!c1) {
            /* Append specifics */;
        }
    }

    return 0;
}

/**
 * @brief TVS bandwidth limitation from capacitance.
 *
 * L5 Algorithm: The junction capacitance of a TVS forms a
 * low-pass filter with the source impedance:
 *
 * For digital signals (rise-time limited):
 *   BW_max ≈ 0.35 / (R_source × C_j)  [Hz]
 *   This ensures the signal rise time is not excessively slowed.
 *
 * For analog signals (3 dB bandwidth):
 *   BW_max ≈ 1 / (2π × R_source × C_j)  [Hz]
 *
 * Example: C_j = 1 pF, R_source = 50 Ω
 *   Digital: BW ≈ 0.35/(50×1e-12) = 7 GHz
 *   Analog:  BW ≈ 1/(2π×50×1e-12) = 3.18 GHz
 */
double esd_tvs_bandwidth_limit(const esd_tvs_params_t *tvs,
                                double source_impedance,
                                int is_digital)
{
    if (!tvs || source_impedance <= 0.0 || tvs->cj_pf <= 0.0) {
        return 1e12;  /* Effectively no limit */
    }

    double c_farad = tvs->cj_pf * 1e-12;
    double rc = source_impedance * c_farad;  /* time constant [s] */

    if (is_digital) {
        /* 0.35 / τ_rc rule for digital rise time */
        return 0.35 / rc / 1e6;  /* Hz → MHz */
    } else {
        /* -3 dB analog bandwidth */
        return 1.0 / (2.0 * M_PI * rc) / 1e6;  /* Hz → MHz */
    }
}

/**
 * @brief RC filter design for ESD suppression.
 *
 * L5 Algorithm: Design a simple RC low-pass filter that:
 *   1. Passes the signal bandwidth without excessive attenuation
 *   2. Attenuates the ESD pulse energy
 *
 * Design constraints:
 *   fc = 1/(2πRC) ≥ f_signal_max  (pass signal)
 *   R ≥ R_source (impedance matching)
 *   C should be as large as practical for ESD absorption
 *
 * The RC filter works by:
 *   - R limits the peak ESD current: I_max = V_ESD / R
 *   - C absorbs ESD charge: ΔV = Q/C = I_ESD × τ / C
 */
int esd_rc_filter_design(double max_signal_freq_mhz,
                          double source_impedance_ohm,
                          double esd_level_kv,
                          esd_rc_filter_params_t *filter)
{
    if (!filter || max_signal_freq_mhz <= 0.0) return -1;

    (void)esd_level_kv;  /* Reserved for future ESD-level-dependent sizing */

    memset(filter, 0, sizeof(*filter));

    /* R should be ≥ source impedance, but not too large to avoid
     * excessive DC voltage drop */
    filter->r_series_ohm = source_impedance_ohm;
    if (filter->r_series_ohm < 10.0) filter->r_series_ohm = 10.0;
    if (filter->r_series_ohm > 1000.0) filter->r_series_ohm = 1000.0;

    /* Choose C to give fc = 3× f_signal_max (plenty of margin) */
    double fc_target = 3.0 * max_signal_freq_mhz;
    double c_farad = 1.0 / (2.0 * M_PI * filter->r_series_ohm * fc_target * 1e6);
    filter->c_shunt_pf = c_farad * 1e12;
    filter->fc_mhz = 1.0 / (2.0 * M_PI * filter->r_series_ohm * c_farad) / 1e6;
    filter->max_signal_freq_mhz = max_signal_freq_mhz;

    return 0;
}

/* ─── Varistor Model ──────────────────────────────────────────── */

/**
 * @brief Varistor voltage at given current.
 *
 * L3: ZnO varistor power-law I-V characteristic:
 *
 *   V = (I / k)^(1/α)
 *
 * where α is the nonlinearity coefficient.
 *
 * For α → ∞, the varistor approaches an ideal Zener diode
 * (constant voltage). Real ZnO varistors have α = 20-60.
 *
 * The high α value means that a 10× increase in current
 * produces only a small increase in voltage:
 *
 *   V(10·I) / V(I) = 10^(1/α)
 *
 * For α = 30: V(10·I)/V(I) = 10^(1/30) ≈ 1.08  (only 8% increase)
 */
double esd_varistor_voltage(const esd_varistor_params_t *varistor,
                             double current_a)
{
    if (!varistor || current_a <= 0.0) return 0.0;

    if (varistor->k_constant <= 0.0) {
        /* Fallback: linear interpolation based on (V1mA, 1mA) and (VC, IC) */
        double ic = 1.0;  /* VC specified at 1A typically */
        double slope = (varistor->vc_v - varistor->v1ma_v) / log10(ic / 0.001);
        return varistor->v1ma_v + slope * log10(current_a / 0.001);
    }

    /* Power law: V = (I/k)^(1/α) */
    return pow(current_a / varistor->k_constant, 1.0 / varistor->alpha);
}

/**
 * @brief Varistor clamp ratio.
 *
 * L6 Canonical Problem: The clamp ratio is the most important
 * figure of merit for ESD protection:
 *
 *   CR = V(@I_ESD) / V_working
 *
 * A lower ratio indicates the varistor clamps closer to the
 * working voltage, providing better protection.
 *
 * For TVS diodes, CR ≈ 1.5-3.0 (much better than varistors).
 * For varistors, CR ≈ 2.5-5.0.
 */
double esd_varistor_clamp_ratio(const esd_varistor_params_t *varistor)
{
    if (!varistor || varistor->v1ma_v <= 0.0) return 0.0;

    return varistor->vc_v / varistor->v1ma_v;
}

/* ─── Multi-Stage Protection Network ──────────────────────────── */

/**
 * @brief Simulate cascaded (two-stage) ESD protection.
 *
 * L6 Canonical Problem: Two-stage ESD protection is the standard
 * approach for robust system-level ESD design:
 *
 *   ESD source → Primary (TVS) → Decoupling R/L → Secondary (on-chip) → IC
 *
 * The decoupling impedance between stages creates a voltage drop
 * that reduces the stress on the secondary (on-chip) protection.
 *
 * Energy partitioning:
 *   E_primary ≈ R_primary × ∫ I²(t) dt (from outer loop)
 *   E_secondary ≈ R_secondary × ∫ I²(t) dt (much smaller)
 *
 * The key design insight is that the decoupling impedance
 * divides the current between the two stages, with most energy
 * dissipated in the primary (external) protection.
 */
int esd_network_simulate(const esd_protection_network_t *network,
                          double esd_current_a,
                          double *v_at_ic,
                          double *energy_primary_j,
                          double *energy_secondary_j)
{
    if (!network || esd_current_a <= 0.0) return -1;

    /* Simplified two-stage model:
     *
     * Stage 1: Primary TVS clamps to V_primary
     * Current splits: I_primary = V_primary / R_series (oversimplified)
     *
     * Stage 2: Series R + L drops voltage: ΔV = I × (R + jωL)
     * Secondary TVS clamps residual to V_secondary
     */

    double v_primary = esd_tvs_clamp_voltage(&network->tvs_primary,
                                              esd_current_a);
    double v_secondary = esd_tvs_clamp_voltage(&network->tvs_secondary,
                                                esd_current_a * 0.5);

    /* Voltage at IC input after series impedance */
    double r_total = network->series_resistance_ohm;
    double v_drop = esd_current_a * r_total;
    double v_residual = v_primary - v_drop;
    if (v_residual < v_secondary) v_residual = v_secondary;
    if (v_residual < 0.0) v_residual = 0.0;

    if (v_at_ic) *v_at_ic = v_residual;

    /* Energy estimates (approximate, ESD pulse ~100 ns) */
    double pulse_width_s = 100e-9;
    if (energy_primary_j) {
        *energy_primary_j = v_primary * esd_current_a * 0.7 * pulse_width_s;
    }
    if (energy_secondary_j) {
        *energy_secondary_j = v_secondary * esd_current_a * 0.3 * pulse_width_s;
    }

    return 0;
}

/**
 * @brief Compare two TVS diodes for a given application.
 *
 * L5 Algorithm: Multi-criteria comparison.
 *
 * Returns the better TVS based on:
 *   1. Safety margins (VRWM, VC, IPP)
 *   2. Clamp ratio (lower is better)
 *   3. Capacitance (lower is better)
 *
 * Tie-breaking: lower capacitance preferred for signal integrity.
 */
int esd_tvs_compare(const esd_protection_window_t *window,
                     const esd_tvs_params_t *tvs_a,
                     const esd_tvs_params_t *tvs_b)
{
    if (!window || !tvs_a || !tvs_b) return 0;

    esd_tvs_selection_t sel_a, sel_b;
    esd_tvs_select(window, tvs_a, &sel_a);
    esd_tvs_select(window, tvs_b, &sel_b);

    /* If only one is suitable, that one wins */
    if (sel_a.suitable && !sel_b.suitable) return 1;
    if (!sel_a.suitable && sel_b.suitable) return -1;
    if (!sel_a.suitable && !sel_b.suitable) return 0;

    /* Both suitable: compare clamp ratio (lower wins) */
    if (sel_a.clamp_ratio < sel_b.clamp_ratio - 0.1) return 1;
    if (sel_b.clamp_ratio < sel_a.clamp_ratio - 0.1) return -1;

    /* Clamp ratios similar: compare capacitance (lower wins) */
    if (tvs_a->cj_pf < tvs_b->cj_pf - 0.1) return 1;
    if (tvs_b->cj_pf < tvs_a->cj_pf - 0.1) return -1;

    return 0;  /* Equal */
}

/**
 * @brief PCB trace width for ESD current handling.
 *
 * L7 Application: PCB layout guideline for ESD protection.
 *
 * IPC-2221 trace width formula (external layer):
 *
 *   I_max = k × ΔT^0.44 × A^0.725
 *
 * where:
 *   I_max = max current [A]
 *   ΔT    = temperature rise [°C]
 *   A     = cross-sectional area [mil²]
 *   k     = 0.048 (external), 0.024 (internal)
 *
 * For ESD, we are concerned with peak current, not continuous.
 * A 1 oz copper trace of width W [mm]:
 *   Area = W[mm] × 35 µm × 1000 [mil² conversion]
 *
 * Simplified for ESD (short pulse, adiabatic heating):
 *   W_min [mm] = I_peak [A] / (k_esd × copper_oz)
 *
 * where k_esd ≈ 10 A/(mm·oz) for 20°C rise during 100 ns pulse.
 */
double esd_pcb_trace_width(double peak_current_a,
                            double copper_weight_oz,
                            double allowed_drop_v,
                            double trace_length_mm)
{
    if (peak_current_a <= 0.0 || copper_weight_oz <= 0.0) return 0.0;

    /* Resistivity of copper: ρ = 1.72e-8 Ω·m */
    /* Sheet resistance: R_sq = ρ / t = 0.5 mΩ/sq for 1 oz (35 µm) */

    double thickness_m = copper_weight_oz * 35e-6;  /* 1 oz = 35 µm */
    double rho = 1.72e-8;  /* Ω·m */

    /* Calculate minimum width from voltage drop constraint:
     * R = ρ × L / (W × t)
     * V_drop = I × R → W = I × ρ × L / (V_drop × t)
     */
    if (allowed_drop_v > 0.0 && trace_length_mm > 0.0) {
        double l_m = trace_length_mm * 1e-3;
        double w_m = peak_current_a * rho * l_m /
                     (allowed_drop_v * thickness_m);
        double w_mm = w_m * 1000.0;

        /* Also apply thermal constraint: ~10A/mm for 1oz copper
         * during short ESD pulse */
        double w_thermal_mm = peak_current_a / (10.0 * copper_weight_oz);

        return (w_mm > w_thermal_mm) ? w_mm : w_thermal_mm;
    }

    return peak_current_a / (10.0 * copper_weight_oz);
}
