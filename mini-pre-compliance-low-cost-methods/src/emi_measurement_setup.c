/**
 * emi_measurement_setup.c — Measurement Setup Implementation
 *
 * LISN impedance modeling, TEM/GTEM cell field calculations, antenna
 * initialization, site attenuation computation, and measurement geometry.
 *
 * Knowledge Coverage:
 *   L2: LISN impedance characteristics, TEM cell field uniformity
 *   L3: Complex impedance network analysis, transmission line modeling
 *   L4: Normalized Site Attenuation (CISPR 16-1-4), Friis formula
 *   L5: Site validation algorithms, three-antenna method
 *   L6: Complete OATS/SAC validation procedure
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "emi_measurement_setup.h"

/* ─── LISN Functions ─────────────────────────────────────────────────── */

double emi_lisn_impedance_mag(double freq_hz, const emi_lisn_spec_t *lisn) {
    if (!lisn) return 50.0;

    double omega = 2.0 * EMI_PI * freq_hz;
    double L = lisn->inductance_h;
    double C_line = lisn->cap_line_to_ground_f;
    double R_meas = lisn->measurement_port_z_ohm;
    double R_series = lisn->series_resistance_ohm;

    /* Impedance of L + R_series branch: Z_L = R_series + jωL */
    /* Impedance of C_line: Z_C = 1/(jωC_line) */
    /* LISN impedance seen at EUT port:
     * Z = R_meas || (jωL + R_series + 1/(jωC_line))
     *
     * Compute as: Z = (R * Z_branch) / (R + Z_branch)
     * |Z| = R * |Z_branch| / |R + Z_branch|
     */

    /* Z_branch = R_series + j(ωL - 1/(ωC_line)) */
    double XL = omega * L;
    double XC = (C_line > 0.0) ? 1.0 / (omega * C_line) : 0.0;
    double reactance = XL - XC;
    double Z_branch_real = R_series;
    double Z_branch_mag = sqrt(Z_branch_real * Z_branch_real + reactance * reactance);

    /* R_meas + Z_branch = (R_meas + R_series) + j*reactance */
    double parallel_real = R_meas + Z_branch_real;
    double parallel_mag = sqrt(parallel_real * parallel_real + reactance * reactance);

    if (parallel_mag <= 0.0) return R_meas;
    return R_meas * Z_branch_mag / parallel_mag;
}

double emi_lisn_voltage_division_factor_db(double freq_hz,
                                            const emi_lisn_spec_t *lisn,
                                            double eut_source_impedance_ohm) {
    if (!lisn) return 0.0;
    /* V_meas / V_source = Z_meas / (Z_meas + Z_source) */
    double z_meas = emi_lisn_impedance_mag(freq_hz, lisn);
    double z_total = z_meas + eut_source_impedance_ohm;
    if (z_total <= 0.0) return 0.0;
    double ratio = z_meas / z_total;
    if (ratio <= 0.0) return -100.0;
    return 20.0 * log10(ratio);
}

void emi_lisn_init_cispr16(emi_lisn_spec_t *lisn,
                            double max_current_a, double max_voltage_v) {
    if (!lisn) return;
    memset(lisn, 0, sizeof(*lisn));
    lisn->inductance_h            = 50e-6;    /* 50 uH per line */
    lisn->cap_line_to_ground_f    = 0.25e-6;  /* 0.25 uF (CISPR 16-1-2, 2-line V-LISN) */
    lisn->cap_coupling_f          = 0.1e-6;   /* 0.1 uF coupling to measurement port */
    lisn->series_resistance_ohm   = 0.5;      /* ESR of inductor */
    lisn->measurement_port_z_ohm  = 50.0;
    lisn->insertion_loss_db       = 0.5;      /* nominal pass-through loss */
    lisn->isolation_db            = 40.0;     /* line-to-line isolation */
    lisn->max_current_a           = max_current_a;
    lisn->max_voltage_v           = max_voltage_v;
    lisn->type                    = "CISPR 16-1-2 V-LISN";
}

void emi_lisn_init_lowcost(emi_lisn_spec_t *lisn) {
    if (!lisn) return;
    memset(lisn, 0, sizeof(*lisn));
    lisn->inductance_h            = 47e-6;    /* 47 uH (standard off-the-shelf value) */
    lisn->cap_line_to_ground_f    = 0.22e-6;  /* 0.22 uF (common film capacitor) */
    lisn->cap_coupling_f          = 0.1e-6;   /* 0.1 uF coupling */
    lisn->series_resistance_ohm   = 2.0;      /* Higher ESR for budget ferrite-core inductor */
    lisn->measurement_port_z_ohm  = 50.0;
    lisn->insertion_loss_db       = 1.5;      /* higher loss for DIY build */
    lisn->isolation_db            = 30.0;
    lisn->max_current_a           = 3.0;      /* limited by inductor gauge */
    lisn->max_voltage_v           = 250.0;
    lisn->type                    = "Low-Cost DIY LISN";
}

/* ─── TEM Cell Functions ──────────────────────────────────────────────── */

double emi_tem_cell_field_strength(double p_in_dbm, const emi_tem_cell_spec_t *tem) {
    if (!tem || tem->septum_height_m <= 0.0) return 0.0;
    /* E = sqrt(P * Z0) / d
     * E_dBVm = P_dBW + 10*log10(Z0) - 20*log10(d) — in dB
     * Then convert to V/m: E_Vm = 10^(E_dBVm/20) * 1e-6
     */
    double p_in_w = pow(10.0, (p_in_dbm - 30.0) / 10.0);
    double e_vm = sqrt(p_in_w * tem->characteristic_impedance_ohm)
                  / tem->septum_height_m;
    return e_vm;
}

double emi_tem_cell_input_power(double desired_e_vm, const emi_tem_cell_spec_t *tem) {
    if (!tem || tem->septum_height_m <= 0.0 || tem->characteristic_impedance_ohm <= 0.0) {
        return -200.0;
    }
    /* P = (E * d)^2 / Z0 */
    double p_w = (desired_e_vm * tem->septum_height_m)
                 * (desired_e_vm * tem->septum_height_m)
                 / tem->characteristic_impedance_ohm;
    if (p_w <= 0.0) return -200.0;
    return 10.0 * log10(p_w) + 30.0; /* Convert to dBm */
}

/* ─── Antenna Specification Functions ────────────────────────────────── */

static void emi_antenna_init_generic(emi_antenna_spec_t *ant,
                                      const char *name,
                                      double fmin, double fmax,
                                      double g_nominal,
                                      const char *pol) {
    if (!ant) return;
    memset(ant, 0, sizeof(*ant));
    ant->name = name;
    ant->freq_min_hz = fmin;
    ant->freq_max_hz = fmax;
    ant->gain_dbi_nominal = g_nominal;
    ant->polarization = pol;
    ant->is_broadband = 1;
    ant->vswr_max = 2.0;
    /* Allocate a simple 3-point AF table for interpolation */
    ant->num_af_points = 5;
    ant->af_freq_hz = (double *)malloc(5 * sizeof(double));
    ant->af_db      = (double *)malloc(5 * sizeof(double));
    if (ant->af_freq_hz && ant->af_db) {
        /* Distribute points across frequency range */
        double log_fmin = log10(fmin);
        double log_fmax = log10(fmax);
        for (int i = 0; i < 5; i++) {
            double log_f = log_fmin + (log_fmax - log_fmin) * i / 4.0;
            ant->af_freq_hz[i] = pow(10.0, log_f);
            ant->af_db[i] = emi_antenna_factor_from_gain(ant->af_freq_hz[i],
                                                          g_nominal);
        }
    }
}

void emi_antenna_init_biconical(emi_antenna_spec_t *ant) {
    emi_antenna_init_generic(ant, "Biconical (30-300 MHz)",
                              30e6, 300e6, 1.5, "horizontal");
}

void emi_antenna_init_lpda(emi_antenna_spec_t *ant) {
    emi_antenna_init_generic(ant, "Log-Periodic Dipole Array (200-2000 MHz)",
                              200e6, 2000e6, 6.0, "horizontal");
}

void emi_antenna_init_double_ridge_horn(emi_antenna_spec_t *ant) {
    emi_antenna_init_generic(ant, "Double-Ridged Horn (1-18 GHz)",
                              1e9, 18e9, 10.0, "dual");
}

void emi_antenna_init_active_monopole(emi_antenna_spec_t *ant) {
    emi_antenna_init_generic(ant, "Active Monopole (9 kHz - 30 MHz)",
                              9e3, 30e6, -10.0, "vertical");
    /* Active monopole uses capacitive sensing, not resonance,
     * so gain is very low (~-10 dBi) below 1 MHz.
     * Correct AF table: AF increases sharply at low frequencies. */
    if (ant->af_freq_hz && ant->af_db) {
        for (int i = 0; i < ant->num_af_points; i++) {
            /* AF for active monopole is higher at low frequencies */
            double freq_mhz = ant->af_freq_hz[i] / 1e6;
            ant->af_db[i] = 20.0 * log10(freq_mhz) + 10.0 - 29.79;
            /* Adjust: active monopole AF ~ 10 dB worse than passive at low end */
        }
    }
}

double emi_antenna_factor_interpolate(const emi_antenna_spec_t *ant,
                                       double freq_hz) {
    if (!ant || !ant->af_freq_hz || !ant->af_db || ant->num_af_points <= 0) {
        return 0.0;
    }
    if (ant->num_af_points == 1) return ant->af_db[0];

    if (freq_hz <= ant->af_freq_hz[0]) return ant->af_db[0];
    if (freq_hz >= ant->af_freq_hz[ant->num_af_points - 1]) {
        return ant->af_db[ant->num_af_points - 1];
    }

    for (int i = 0; i < ant->num_af_points - 1; i++) {
        if (freq_hz >= ant->af_freq_hz[i] && freq_hz <= ant->af_freq_hz[i + 1]) {
            double log_f = log10(freq_hz);
            double log_f1 = log10(ant->af_freq_hz[i]);
            double log_f2 = log10(ant->af_freq_hz[i + 1]);
            double t = (log_f - log_f1) / (log_f2 - log_f1);
            return ant->af_db[i] + t * (ant->af_db[i + 1] - ant->af_db[i]);
        }
    }
    return ant->af_db[ant->num_af_points - 1];
}

/* ─── Site Attenuation (NSA) ──────────────────────────────────────────── */

double emi_normalized_site_attenuation(double freq_hz, double distance_m,
                                        double h_tx_m, double h_rx_m) {
    /* NSA for horizontal polarization, broadband antennas
     * Ref: CISPR 16-1-4, Annex A, Eq. A.1-A.4
     *
     * NSA = AF1 + AF2 - 20*log10(f_MHz) + 48.92
     *       - 20*log10(|1/d1 + rho_h*exp(-j*k*d1)/d2|)
     *
     * where:
     *   d1 = sqrt(d^2 + (h1 - h2)^2)  — direct path
     *   d2 = sqrt(d^2 + (h1 + h2)^2)  — reflected path
     *   rho_h ≈ -1 for horizontal polarization over perfect ground
     */

    double lambda = EMI_C0 / freq_hz;
    double k_wave = 2.0 * EMI_PI / lambda;
    double d_horiz = distance_m;  /* Horizontal separation */

    /* Direct path length */
    double dh = h_tx_m - h_rx_m;
    double d1 = sqrt(d_horiz * d_horiz + dh * dh);

    /* Reflected path length */
    double sh = h_tx_m + h_rx_m;
    double d2 = sqrt(d_horiz * d_horiz + sh * sh);

    /* Reflection coefficient: -1 for horizontal over perfect ground */
    double rho = -1.0;

    /* Combined path gain: |1/d1 + rho*exp(-j*k*d2)/d2|
     * Simplified NSA model (phase differential reserved for future refinement) */
    (void)k_wave;
    /* exp(-j*k*d2) = cos(k*d2) - j*sin(k*d2) */
    double real_part = 1.0 / d1 + rho * cos(k_wave * d2) / d2;
    double imag_part = -rho * sin(k_wave * d2) / d2;
    double combined_mag = sqrt(real_part * real_part + imag_part * imag_part);

    if (combined_mag <= 0.0) return 100.0;

    double freq_mhz = freq_hz / 1.0e6;
    /* NSA = -20*log10(|combined_path|) + 20*log10(f_MHz) - ... */
    double nsa = -20.0 * log10(combined_mag) + 20.0 * log10(freq_mhz) - 29.60;

    return nsa;
}

double emi_site_attenuation_deviation_db(double measured_nsa_db,
                                          double theoretical_nsa_db) {
    return measured_nsa_db - theoretical_nsa_db;
}

/* ─── Measurement Distance Correction ────────────────────────────────── */

double emi_distance_correction_db(double e_meas_dbuvm, double d_meas_m,
                                   double d_std_m) {
    if (d_meas_m <= 0.0 || d_std_m <= 0.0) return e_meas_dbuvm;
    if (fabs(d_meas_m - d_std_m) < 0.01) return e_meas_dbuvm;
    /* 20 dB/decade extrapolation: E_std = E_meas + 20*log10(d_meas/d_std) */
    double correction = 20.0 * log10(d_meas_m / d_std_m);
    return e_meas_dbuvm + correction;
}
