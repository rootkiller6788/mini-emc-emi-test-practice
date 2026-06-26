/**
 * emi_lowcost_methods.c — Low-Cost Pre-Compliance Methods Implementation
 *
 * DIY probe construction, current probe models, three-antenna calibration,
 * shielding effectiveness measurements, pre-compliance/full-compliance
 * correlation, and recommended margins.
 *
 * Knowledge Coverage:
 *   L5: DIY calibration methods, three-antenna method
 *   L6: Shielding effectiveness measurement workflow
 *   L7: Real applications — Arduino EMC, SMPS testing, IoT devices
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "emi_lowcost_methods.h"

/* ─── DIY Probe Initialization ────────────────────────────────────────── */

void emi_diy_hprobe_init(emi_diy_probe_spec_t *probe, double loop_diameter_mm,
                          int num_turns) {
    if (!probe) return;
    memset(probe, 0, sizeof(*probe));
    probe->probe_type = "h-field_loop";
    probe->loop_diameter_mm = loop_diameter_mm;
    probe->num_turns = num_turns;
    probe->wire_diameter_mm = 0.5; /* 24 AWG semi-rigid coax center conductor */
    probe->connector_type = "SMA";
    probe->has_shielding = 1;
    probe->material_cost_usd = 3.50;
    /* Estimated -3dB bandwidth from self-resonance of
     * a shielded loop of diameter d: f_res ≈ c/(pi*d) * safety factor */
    double d_m = loop_diameter_mm / 1000.0;
    if (d_m > 0.0) {
        probe->estimated_bandwidth_hz = EMI_C0 / (EMI_PI * d_m * 1.5);
    } else {
        probe->estimated_bandwidth_hz = 1e9;
    }
    /* Commercial probes have ~0-3 dB better sensitivity due to
     * optimized construction and calibration. A well-built DIY
     * probe is within 3-6 dB of commercial performance. */
    probe->estimated_sensitivity_db = -5.0;
}

void emi_diy_eprobe_init(emi_diy_probe_spec_t *probe, double probe_length_mm) {
    if (!probe) return;
    memset(probe, 0, sizeof(*probe));
    probe->probe_type = "e-field_monopole";
    probe->loop_diameter_mm = 0.0; /* not a loop */
    probe->num_turns = 0;
    probe->wire_diameter_mm = 1.0; /* rigid wire, ~18 AWG */
    probe->connector_type = "BNC";
    probe->has_shielding = 0; /* monopole is inherently unshielded */
    probe->material_cost_usd = 2.00;
    /* Monopole bandwidth limited by capacitive loading.
     * f_c ≈ 1/(2*pi*50*C) where C scales with probe length.
     * Longer probe → higher capacitance → lower bandwidth. */
    double length_m = probe_length_mm / 1000.0;
    double cap_est = 2.0 * EMI_PI * 8.854e-12 * length_m
                     / (log(2.0 * length_m / 0.0005) - 1.0);
    if (cap_est > 1e-15) {
        probe->estimated_bandwidth_hz = 1.0 / (2.0 * EMI_PI * 50.0 * cap_est);
        if (probe->estimated_bandwidth_hz > 1e9) probe->estimated_bandwidth_hz = 1e9;
    } else {
        probe->estimated_bandwidth_hz = 300e6;
    }
    /* Shorter probe = higher bandwidth but lower sensitivity */
    probe->estimated_sensitivity_db = -3.0 - 20.0 * log10(0.05 / length_m);
}

/* ─── Current Probe Functions ─────────────────────────────────────────── */

double emi_current_probe_transfer_impedance(double freq_hz,
                                             const emi_current_probe_spec_t *cp) {
    if (!cp) return 0.0;
    double mu0 = 4.0 * EMI_PI * 1.0e-7;
    double omega = 2.0 * EMI_PI * freq_hz;
    double n = (double)cp->num_secondary_turns;

    /* Magnetizing inductance:
     * L_m = mu0 * mu_r * N^2 * A_core / l_eff */
    double L_m = mu0 * cp->core_mu_r * n * n
                 * cp->core_cross_section_m2 / cp->core_effective_length_m;

    /* Z_T = (jωL_m) || R_load / N
     * At high frequencies where ωL_m >> R_load:
     *   Z_T ≈ R_load / N
     * At low frequencies:
     *   Z_T ≈ jωL_m / N  (rolls off at 20 dB/decade)
     */
    double r_load = 50.0; /* standard measurement port */
    double x_lm = omega * L_m;

    /* Parallel combination: (jX * R_load) / (jX + R_load)
     * Magnitude: |Z| = X * R_load / sqrt(X^2 + R_load^2)
     * Then divide by N for primary-to-secondary ratio. */
    double z_parallel_mag = x_lm * r_load / sqrt(x_lm * x_lm + r_load * r_load);

    return z_parallel_mag / n;
}

double emi_current_probe_measure_current(double v_measured_v, double freq_hz,
                                          const emi_current_probe_spec_t *cp) {
    double z_t = emi_current_probe_transfer_impedance(freq_hz, cp);
    if (z_t <= 0.0) return 0.0;
    return v_measured_v / z_t;
}

void emi_diy_current_probe_init(emi_current_probe_spec_t *cp) {
    if (!cp) return;
    memset(cp, 0, sizeof(*cp));
    /* FT-240-43 ferrite toroid parameters */
    cp->core_mu_r = 850.0;             /* Fair-Rite type 43 material */
    cp->core_cross_section_m2 = 1.53e-4;   /* ~15.3 mm^2 (240-size toroid) */
    cp->core_effective_length_m = 0.130;   /* ~130 mm magnetic path */
    cp->num_secondary_turns = 8;
    cp->transfer_impedance_ohm = 6.25;     /* R_load/N = 50/8 */
    cp->freq_min_hz = 10e3;
    cp->freq_max_hz = 100e6;
    cp->insertion_impedance_ohm = 2.0;     /* negligible loading */
    cp->saturation_current_a = 2.0;
}

/* ─── Three-Antenna Method ────────────────────────────────────────────── */

void emi_three_antenna_method(double s21_12_db, double s21_13_db,
                               double s21_23_db, double freq_hz,
                               double distance_m,
                               double *af1_out, double *af2_out,
                               double *af3_out) {
    if (!af1_out || !af2_out || !af3_out) return;

    /* CISPR 16-1-4, Annex A, three-antenna method equations:
     *
     * Let:
     *   S_ij = measured S21 between antenna i and j, dB
     *   AF_i = antenna factor of antenna i, dB(1/m)
     *   P(f,d) = 20*log10(4*pi*d/lambda) = free-space path loss, dB
     *
     * For identical measurement geometry:
     *   S_12 = P + AF1 + AF2
     *   S_13 = P + AF1 + AF3
     *   S_23 = P + AF2 + AF3
     *
     * Solving:
     *   AF1 = (S_12 + S_13 - S_23 - P) / 2
     *   AF2 = (S_12 + S_23 - S_13 - P) / 2
     *   AF3 = (S_13 + S_23 - S_12 - P) / 2
     */

    double lambda = EMI_C0 / freq_hz;
    double path_loss_db = 20.0 * log10(4.0 * EMI_PI * distance_m / lambda);

    *af1_out = (s21_12_db + s21_13_db - s21_23_db - path_loss_db) / 2.0;
    *af2_out = (s21_12_db + s21_23_db - s21_13_db - path_loss_db) / 2.0;
    *af3_out = (s21_13_db + s21_23_db - s21_12_db - path_loss_db) / 2.0;
}

/* ─── GTEM Validation ────────────────────────────────────────────────── */

double emi_gtem_validation_factor(double measured_field_vm,
                                   double input_power_w,
                                   const emi_tem_cell_spec_t *tem) {
    if (!tem || input_power_w <= 0.0) return 1.0;
    /* Theoretical field: E_theo = sqrt(P * Z0) / d */
    double e_theo = sqrt(input_power_w * tem->characteristic_impedance_ohm)
                    / tem->septum_height_m;
    if (e_theo <= 0.0) return 1.0;
    return measured_field_vm / e_theo;
}

/* ─── Shielding Effectiveness ─────────────────────────────────────────── */

double emi_shielding_effectiveness(double ref_level_dbm, double shielded_level_dbm,
                                    const emi_antenna_spec_t *tx_ant,
                                    const emi_antenna_spec_t *rx_ant,
                                    double distance_m, double freq_hz) {
    (void)tx_ant; (void)rx_ant;
    (void)distance_m; (void)freq_hz;

    /* SE = Reference - Shielded (dB)
     * Basic point-by-point measurement.
     * For the full method including antenna factor corrections:
     * SE = (P_ref - P_shield) + (AF_tx(f) + AF_rx(f))_corrected
     *
     * For pre-compliance with identical antennas at fixed positions,
     * the antenna factor terms cancel exactly in the difference,
     * so SE = ref_level - shielded_level is valid. */
    return ref_level_dbm - shielded_level_dbm;
}

double emi_cable_shielding_effectiveness(double injected_current_ma,
                                          double coupled_voltage_uv,
                                          double freq_hz,
                                          double cable_length_m) {
    (void)freq_hz;
    if (injected_current_ma <= 0.0 || cable_length_m <= 0.0) return 0.0;

    /* Transfer impedance: Z_T = V_coupled / (I_injected * length)
     * SE = -20*log10(|Z_T| / R_0) where R_0 = 1 Ohm reference
     *
     * For cable shield effectiveness:
     * SE = 20*log10(I_injected / I_coupled)
     * where I_coupled = V_coupled / Z_term
     */
    double i_injected_a = injected_current_ma / 1000.0;
    double v_coupled_v = coupled_voltage_uv / 1.0e6;
    double z_term = 50.0; /* termination impedance */
    double i_coupled_a = v_coupled_v / z_term;

    if (i_coupled_a <= 0.0) return 100.0;
    return 20.0 * log10(i_injected_a / i_coupled_a);
}

/* ─── Correlation Analysis ───────────────────────────────────────────── */

int emi_compute_correlation(const double *pre_dbuvm, const double *full_dbuvm,
                             int num_points,
                             emi_correlation_result_t *result) {
    if (!pre_dbuvm || !full_dbuvm || !result || num_points < 2) return -1;

    memset(result, 0, sizeof(*result));
    result->num_points = num_points;

    /* Allocate arrays */
    result->freq_hz = (double *)calloc(num_points, sizeof(double));
    result->precompliance_dbuvm = (double *)calloc(num_points, sizeof(double));
    result->fullcompliance_dbuvm = (double *)calloc(num_points, sizeof(double));
    result->correlation_factor_db = (double *)calloc(num_points, sizeof(double));

    if (!result->freq_hz || !result->precompliance_dbuvm
        || !result->fullcompliance_dbuvm || !result->correlation_factor_db) {
        free(result->freq_hz);
        free(result->precompliance_dbuvm);
        free(result->fullcompliance_dbuvm);
        free(result->correlation_factor_db);
        return -1;
    }

    /* Compute statistics */
    double sum_diff = 0.0;
    double sum_diff_sq = 0.0;
    double sum_pre = 0.0, sum_full = 0.0;
    double sum_pre_sq = 0.0, sum_full_sq = 0.0;
    double sum_pre_full = 0.0;
    int valid = 0;
    double max_dev = 0.0;

    for (int i = 0; i < num_points; i++) {
        /* Skip points where either value is below reasonable threshold */
        if (full_dbuvm[i] < -100.0 && pre_dbuvm[i] < -100.0) continue;

        double diff = full_dbuvm[i] - pre_dbuvm[i];
        result->precompliance_dbuvm[valid] = pre_dbuvm[i];
        result->fullcompliance_dbuvm[valid] = full_dbuvm[i];
        result->correlation_factor_db[valid] = diff;

        sum_diff += diff;
        sum_diff_sq += diff * diff;
        sum_pre += pre_dbuvm[i];
        sum_full += full_dbuvm[i];
        sum_pre_sq += pre_dbuvm[i] * pre_dbuvm[i];
        sum_full_sq += full_dbuvm[i] * full_dbuvm[i];
        sum_pre_full += pre_dbuvm[i] * full_dbuvm[i];

        double adiff = fabs(diff);
        if (adiff > max_dev) max_dev = adiff;
        valid++;
    }

    result->num_comparable_points = valid;
    result->max_deviation_db = max_dev;

    if (valid < 2) return valid;

    /* Mean offset */
    double mean_diff = sum_diff / valid;
    result->mean_offset_db = mean_diff;

    /* Standard deviation */
    double var = (sum_diff_sq / valid) - (mean_diff * mean_diff);
    if (var < 0.0) var = 0.0;
    result->std_deviation_db = sqrt(var);

    /* Pearson correlation coefficient:
     * r = (N*sum_pre_full - sum_pre*sum_full)
     *     / sqrt((N*sum_pre_sq - sum_pre^2) * (N*sum_full_sq - sum_full^2)) */
    double n = (double)valid;
    double num_r = n * sum_pre_full - sum_pre * sum_full;
    double den_r = sqrt((n * sum_pre_sq - sum_pre * sum_pre)
                        * (n * sum_full_sq - sum_full * sum_full));
    result->pearson_r = (den_r > 0.0) ? (num_r / den_r) : 0.0;

    return valid;
}

double emi_recommend_precompliance_margin(const emi_correlation_result_t *corr,
                                           double k_factor) {
    if (!corr) return 10.0;
    /* Recommended margin = |mean_offset| + k * std_deviation
     * k = 1: ~68% confidence
     * k = 2: ~95% confidence
     * k = 3: ~99.7% confidence
     */
    return fabs(corr->mean_offset_db) + k_factor * corr->std_deviation_db;
}
