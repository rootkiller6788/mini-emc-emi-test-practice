/**
 * @file insertion_loss.c
 * @brief Insertion Loss Computation — Full Frequency Sweeps, CISPR 17, Uncertainty
 *
 * Implements insertion loss calculations including:
 *  - Theoretical IL from Z/S/ABCD parameters
 *  - Frequency sweeps with logarithmic spacing
 *  - CISPR 17 comprehensive multi-impedance evaluation
 *  - HF degradation from parasitic coupling
 *  - Measurement uncertainty budget (ISO/IEC 17025 GUM)
 *  - System-level IL including connectors and PCB
 *  - IL margin vs EMC limits
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "cm_dm_filter.h"
#include "cm_dm_network.h"
#include "insertion_loss.h"
#include "cm_choke_model.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_2PI
#define M_2PI (2.0 * M_PI)
#endif

/* ========================================================================
 * L1: Insertion Loss from Z-matrix
 *
 * IL = 20·log10( |(Z_S+Z_L)·Z_21 / ((Z_11+Z_S)(Z_22+Z_L) - Z_12·Z_21)| )
 *
 * This is the most general expression for IL of a two-port network
 * with real source and load impedances. It fully captures the
 * interaction between filter input/output impedances and
 * source/load terminations.
 *
 * Special cases:
 *  - If filter is a simple series Z_f in a 50Ω system:
 *    IL ≈ 20·log10(1 + Z_f/100)  [for Z_11 = Z_22 = Z_f/2, Z_12 = Z_21 = Z_f/2]
 *
 *  - If filter is a simple shunt Y_f:
 *    IL ≈ 20·log10(1 + Y_f·25)   [for Z_S = Z_L = 50Ω]
 *
 * Reference: Paul §6.3.2, CISPR 17 Annex A
 * ======================================================================== */

double il_from_z_matrix(const z_matrix_t *z, double z_source, double z_load) {
    if (z == NULL) return 0.0;

    /* Terminated two-port voltage transfer ratio */
    complex_t zs = {z_source, 0.0};
    complex_t zl = {z_load, 0.0};

    /* Compute: (Z_11 + Z_S)(Z_22 + Z_L) - Z_12·Z_21 */
    complex_t t11 = complex_add(z->z11, zs);
    complex_t t22 = complex_add(z->z22, zl);
    complex_t t1_t2 = complex_mul(t11, t22);
    complex_t z12z21 = complex_mul(z->z12, z->z21);
    complex_t denom = complex_sub(t1_t2, z12z21);

    /* Numerator: Z_21 · Z_L */
    complex_t num = {z->z21.real * z_load, z->z21.imag * z_load};

    /* Transfer function: V_L/V_S = num/denom
     * Without filter: V_L/V_S = Z_L/(Z_S+Z_L)
     * IL = 20·log10( |V_wo|/|V_w| ) = 20·log10( |ZL/(ZS+ZL)| / |num/denom| )
     */
    double v_wo_ratio = z_load / (z_source + z_load);
    double v_w_ratio = complex_mag(complex_div(num, denom));

    if (v_w_ratio < 1e-30) return DBL_MAX;

    double il = 20.0 * log10(v_wo_ratio / v_w_ratio);
    return il > 0.0 ? il : 0.0;
}

/* ========================================================================
 * L1: Insertion Loss from S-parameters
 *
 * IL = -20·log10(|S₂₁|)  [dB]
 *
 * This is the insertion loss in a matched Z₀ system where both
 * ports are terminated in Z₀. It represents the forward power
 * transmission loss through the two-port.
 *
 * Note: This differs from the general IL expression because S₂₁
 * is defined with Z₀ terminations, whereas real source/load
 * impedances may differ from Z₀.
 *
 * When Z_S = Z_L = Z₀: IL_s = IL_z (they are equivalent).
 * When Z_S ≠ Z₀ or Z_L ≠ Z₀: use IL_z for accurate results.
 * ======================================================================== */

double il_from_s_matrix(const s_matrix_t *s) {
    if (s == NULL) return 0.0;

    double mag_s21 = complex_mag(s->s21);
    if (mag_s21 < 1e-30) return DBL_MAX;

    return -20.0 * log10(mag_s21);
}

/* ========================================================================
 * L1: Insertion Loss from ABCD matrix
 *
 * From ABCD parameters with arbitrary source/load:
 *
 *   V_L/V_S = Z_L / (A·Z_L + B + C·Z_S·Z_L + D·Z_S)
 *
 * Without filter:
 *   V_L/V_S = Z_L / (Z_S + Z_L)
 *
 * IL = 20·log10( |V_wo| / |V_w| )
 *    = 20·log10( |A·Z_L + B + C·Z_S·Z_L + D·Z_S| / |Z_S + Z_L| )
 * ======================================================================== */

double il_from_abcd_matrix(const abcd_matrix_t *abcd,
                            double z_source, double z_load) {
    if (abcd == NULL) return 0.0;

    /* V_w = Z_L / (A·Z_L + B + C·Z_S·Z_L + D·Z_S) */
    complex_t a_zl = {abcd->A.real * z_load, abcd->A.imag * z_load};
    complex_t c_zs_zl = {abcd->C.real * z_source * z_load,
                          abcd->C.imag * z_source * z_load};
    complex_t d_zs = {abcd->D.real * z_source, abcd->D.imag * z_source};

    complex_t denom_with = complex_add(
        complex_add(a_zl, abcd->B),
        complex_add(c_zs_zl, d_zs));

    /* V_wo = Z_L / (Z_S + Z_L) */
    double v_wo_ratio = z_load / (z_source + z_load);

    double mag_vw = z_load / complex_mag(denom_with);
    if (mag_vw < 1e-30) return DBL_MAX;

    double il = 20.0 * log10(v_wo_ratio / mag_vw);
    return il > 0.0 ? il : 0.0;
}

/* ========================================================================
 * L3: Frequency Sweep of Insertion Loss
 *
 * Generates logarithmically-spaced frequency points and computes
 * IL at each point using full network analysis.
 *
 * Logarithmic spacing:
 *   f_i = f_start · 10^(i·log10(f_stop/f_start)/N)  for i=0..N-1
 *
 * This is the standard presentation format for EMI filter
 * performance data (x-axis log frequency, y-axis dB).
 * ======================================================================== */

il_sweep_t *il_frequency_sweep(const filter_element_t *filter_elements,
                                size_t n_elem,
                                double f_start, double f_stop,
                                size_t num_points,
                                double z_source_cm, double z_source_dm,
                                double z_load_cm, double z_load_dm) {
    if (n_elem == 0 || f_start <= 0.0 || f_stop <= f_start || num_points < 2) {
        return NULL;
    }

    il_sweep_t *sweep = (il_sweep_t *)malloc(sizeof(il_sweep_t));
    if (sweep == NULL) return NULL;

    memset(sweep, 0, sizeof(il_sweep_t));
    sweep->f_start = f_start;
    sweep->f_stop = f_stop;
    sweep->num_points = num_points;

    sweep->frequencies = (double *)malloc(num_points * sizeof(double));
    sweep->il_cm_db = (double *)malloc(num_points * sizeof(double));
    sweep->il_dm_db = (double *)malloc(num_points * sizeof(double));

    if (!sweep->frequencies || !sweep->il_cm_db || !sweep->il_dm_db) {
        free(sweep->frequencies);
        free(sweep->il_cm_db);
        free(sweep->il_dm_db);
        free(sweep);
        return NULL;
    }

    double log_ratio = log10(f_stop / f_start);

    /* Initialize min values */
    sweep->il_cm_min = DBL_MAX;
    sweep->il_dm_min = DBL_MAX;

    for (size_t i = 0; i < num_points; i++) {
        double frac = (double)i / (double)(num_points - 1);
        sweep->frequencies[i] = f_start * pow(10.0, frac * log_ratio);
        double f = sweep->frequencies[i];

        /* CM insertion loss */
        z_matrix_t z_cm;
        if (filter_to_z_matrix(filter_elements, n_elem, f, &z_cm) == 0) {
            sweep->il_cm_db[i] = il_from_z_matrix(&z_cm, z_source_cm, z_load_cm);
        } else {
            sweep->il_cm_db[i] = 0.0;
        }

        /* DM insertion loss */
        z_matrix_t z_dm;
        if (filter_to_z_matrix(filter_elements, n_elem, f, &z_dm) == 0) {
            sweep->il_dm_db[i] = il_from_z_matrix(&z_dm, z_source_dm, z_load_dm);
        } else {
            sweep->il_dm_db[i] = 0.0;
        }

        if (sweep->il_cm_db[i] < sweep->il_cm_min)
            sweep->il_cm_min = sweep->il_cm_db[i];
        if (sweep->il_dm_db[i] < sweep->il_dm_min)
            sweep->il_dm_min = sweep->il_dm_db[i];

        /* Capture key frequency points */
        if (f >= 140e3 && f <= 160e3) {
            sweep->il_cm_at_150k = sweep->il_cm_db[i];
            sweep->il_dm_at_150k = sweep->il_dm_db[i];
        }
        if (f >= 490e3 && f <= 510e3) {
            sweep->il_cm_at_500k = sweep->il_cm_db[i];
            sweep->il_dm_at_500k = sweep->il_dm_db[i];
        }
        if (f >= 990e3 && f <= 1.01e6) {
            sweep->il_cm_at_1M = sweep->il_cm_db[i];
            sweep->il_dm_at_1M = sweep->il_dm_db[i];
        }
        if (f >= 9.9e6 && f <= 10.1e6) {
            sweep->il_cm_at_10M = sweep->il_cm_db[i];
            sweep->il_dm_at_10M = sweep->il_dm_db[i];
        }
    }

    sweep->is_valid = 1;
    return sweep;
}

void il_sweep_free(il_sweep_t *sweep) {
    if (sweep) {
        free(sweep->frequencies);
        free(sweep->il_cm_db);
        free(sweep->il_dm_db);
        free(sweep);
    }
}

/* ========================================================================
 * L2: CISPR 17 comprehensive insertion loss evaluation
 *
 * Per CISPR 17, the insertion loss of an EMI filter must be
 * characterized under AT LEAST three impedance conditions:
 *   1. 50 Ω source, 50 Ω load (nominal matched)
 *   2. 0.1 Ω source, 100 Ω load (worst-case source)
 *   3. 100 Ω source, 0.1 Ω load (worst-case load)
 *
 * Some standards also require:
 *   4. 0.1 Ω / 0.1 Ω (both low — CM mimicked)
 *   5. 100 Ω / 100 Ω (both high — CM mimicked)
 *
 * The filter's rated IL is the WORST CASE across all conditions.
 * This prevents manufacturers from cherry-picking favorable numbers.
 * ======================================================================== */

il_cispr17_results_t *il_cispr17_comprehensive(
    const filter_element_t *elements, size_t n_elem,
    double f_start, double f_stop, size_t num_points) {

    il_cispr17_results_t *results = (il_cispr17_results_t *)
        malloc(sizeof(il_cispr17_results_t));
    if (results == NULL) return NULL;

    memset(results, 0, sizeof(il_cispr17_results_t));
    results->num_conditions = 3;

    /* Condition 1: 50Ω/50Ω */
    results->sweep_50_50 = *il_frequency_sweep(
        elements, n_elem, f_start, f_stop, num_points, 50.0, 50.0, 50.0, 50.0);

    /* Condition 2: 0.1Ω/100Ω */
    results->sweep_01_100 = *il_frequency_sweep(
        elements, n_elem, f_start, f_stop, num_points, 0.1, 0.1, 100.0, 100.0);

    /* Condition 3: 100Ω/0.1Ω */
    results->sweep_100_01 = *il_frequency_sweep(
        elements, n_elem, f_start, f_stop, num_points, 100.0, 100.0, 0.1, 0.1);

    /* Compute worst-case values */
    results->worst_case_cm_150k = DBL_MAX;
    results->worst_case_dm_150k = DBL_MAX;
    results->worst_case_cm_1M = DBL_MAX;
    results->worst_case_dm_1M = DBL_MAX;

    il_sweep_t *sweeps[3] = {&results->sweep_50_50,
                              &results->sweep_01_100,
                              &results->sweep_100_01};

    for (int s = 0; s < 3; s++) {
        if (sweeps[s]->is_valid) {
            if (sweeps[s]->il_cm_at_150k < results->worst_case_cm_150k)
                results->worst_case_cm_150k = sweeps[s]->il_cm_at_150k;
            if (sweeps[s]->il_dm_at_150k < results->worst_case_dm_150k)
                results->worst_case_dm_150k = sweeps[s]->il_dm_at_150k;
            if (sweeps[s]->il_cm_at_1M < results->worst_case_cm_1M)
                results->worst_case_cm_1M = sweeps[s]->il_cm_at_1M;
            if (sweeps[s]->il_dm_at_1M < results->worst_case_dm_1M)
                results->worst_case_dm_1M = sweeps[s]->il_dm_at_1M;
        }
    }

    return results;
}

void il_cispr17_results_free(il_cispr17_results_t *r) {
    if (r) {
        /* Free arrays inside each embedded sweep (not the sweep struct itself) */
        free(r->sweep_50_50.frequencies);
        free(r->sweep_50_50.il_cm_db);
        free(r->sweep_50_50.il_dm_db);
        free(r->sweep_01_100.frequencies);
        free(r->sweep_01_100.il_cm_db);
        free(r->sweep_01_100.il_dm_db);
        free(r->sweep_100_01.frequencies);
        free(r->sweep_100_01.il_cm_db);
        free(r->sweep_100_01.il_dm_db);
        free(r);
    }
}

/* ========================================================================
 * L6: High-Frequency Insertion Loss Degradation
 *
 * At frequencies above ~10 MHz, the designed filter response breaks
 * down due to parasitic effects:
 *
 * 1. CM choke inter-winding capacitance provides a bypass path
 *    for CM current → IL plateau.
 *
 * 2. Y-capacitor ESL limits high-frequency shunting effectiveness.
 *
 * 3. PCB trace inductance adds series impedance to the ground
 *    connection, limiting Y-cap effectiveness.
 *
 * 4. Magnetic coupling between input and output conductors bypasses
 *    the filter entirely (crosstalk).
 *
 * The resulting "IL shelf" typically occurs at 30–100 MHz for
 * discrete-component filters, limited by layout parasitics.
 *
 * Shelf IL estimation:
 *   CM IL_shelf ≈ 20·log10( Z_cm_choke / Z_coupling )
 *   where Z_coupling is the parasitic coupling impedance.
 *
 * Reference: Paul §6.6 "Parasitic Effects in Filters"
 *            Ott §14 "PCB Layout for Filters"
 * ======================================================================== */

il_hf_degradation_t il_hf_degradation_estimate(
    const cm_choke_full_t *cm_choke,
    const filter_element_t *y_caps, size_t n_y_caps,
    double trace_len_mm) {

    il_hf_degradation_t hfd;
    memset(&hfd, 0, sizeof(hfd));

    /* Parasitic coupling capacitance
     * Between filter input and output: ~0.1 pF/mm for parallel traces */
    hfd.parasitic_c_f = trace_len_mm * 0.1e-12;

    /* Ground trace inductance: ~1 nH/mm */
    hfd.parasitic_l_h = trace_len_mm * 1e-9;

    /* CM choke parasitic capacitance */
    if (cm_choke != NULL) {
        hfd.parasitic_c_f = cm_choke->total_parallel_cap;
    }

    /* Y-cap parallel ESL */
    double total_esl = 0.0;
    double total_c_y = 0.0;
    for (size_t i = 0; i < n_y_caps; i++) {
        total_esl += y_caps[i].esl;
        total_c_y += y_caps[i].nominal_value;
    }

    /* IL shelf level:
     * For CM: limited by inter-winding capacitance
     * For DM: limited by X-cap ESL */
    if (hfd.parasitic_c_f > 1e-15) {
        /* Shelf impedance is the parasitic capacitive reactance */
        hfd.shelf_frequency = 30e6;
        double z_parasitic = 1.0 / (M_2PI * hfd.shelf_frequency * hfd.parasitic_c_f);
        hfd.shelf_il_cm_db = 20.0 * log10(50.0 / z_parasitic);
    }

    /* DM shelf from ESL */
    if (total_esl > 0.0 && total_c_y > 0.0) {
        hfd.shelf_il_dm_db = 20.0 * log10(1.0 / (M_2PI * 30e6 * total_esl * total_c_y));
    }

    hfd.max_usable_freq = 30e6;
    if (hfd.parasitic_c_f > 0.0) {
        hfd.max_usable_freq = 1.0 / (M_2PI * hfd.parasitic_c_f * 50.0);
    }

    return hfd;
}

/* ========================================================================
 * L6: IL Measurement Uncertainty per ISO/IEC 17025 (GUM)
 *
 * Uncertainty contributors for insertion loss measurement
 * per CISPR 17-4:
 *
 * 1. Impedance mismatch (Type B):
 *    Γ_source = (Z_S - Z₀)/(Z_S + Z₀) → mismatch uncertainty
 *    u_mm = 20·log10(1 ± |Γ_S|·|Γ_L|)  [dB]
 *
 * 2. Receiver/analyzer (Type B):
 *    From manufacturer calibration certificate
 *    Typically ±0.5 dB for conducted emissions receiver
 *
 * 3. Cable and connector effects (Type B):
 *    Cable loss stability, connector repeatability
 *    Typically ±0.2 dB
 *
 * 4. Repeatability (Type A):
 *    Standard deviation of n repeated measurements
 *    u_A = s / √n
 *
 * Combined standard uncertainty:
 *    u_c = √(u_A² + u_mm² + u_rx² + u_cable²)
 *
 * Expanded uncertainty (k=2, 95% confidence):
 *    U = k · u_c
 *
 * Reference: ISO/IEC Guide 98-3 (GUM), CISPR 16-4-2
 * ======================================================================== */

il_uncertainty_t il_measurement_uncertainty(double il_nominal,
                                             double vswr_source,
                                             double vswr_load,
                                             double receiver_snr,
                                             double repeat_stddev) {
    (void)il_nominal; /* Reserved for IL-dependent uncertainty scaling */
    il_uncertainty_t u;
    memset(&u, 0, sizeof(u));

    /* Impedance mismatch uncertainty
     * Γ = (VSWR-1)/(VSWR+1)
     * u_mm = 20·log10(1 ± |Γ_source|·|Γ_load|) */
    double gamma_s = (vswr_source - 1.0) / (vswr_source + 1.0);
    double gamma_l = (vswr_load - 1.0) / (vswr_load + 1.0);
    u.u_impedance_mismatch = 20.0 * log10(1.0 + gamma_s * gamma_l);

    /* Calibration uncertainty (typical spectrum analyzer) */
    u.u_calibration = 0.5;  /* ±0.5 dB typical */

    /* Receiver noise floor contribution
     * When signal is near noise floor, SNR adds uncertainty */
    u.u_receiver_noise = (receiver_snr > 0.0) ? 0.1 / receiver_snr : 0.5;

    /* Cable/connector effects */
    u.u_cable_effects = 0.2;  /* ±0.2 dB typical for good cables */

    /* Repeatability (Type A) */
    u.u_repeatability = repeat_stddev;

    /* Combined standard uncertainty (RSS) */
    double sum_sq = u.u_repeatability * u.u_repeatability
                   + u.u_impedance_mismatch * u.u_impedance_mismatch
                   + u.u_calibration * u.u_calibration
                   + u.u_receiver_noise * u.u_receiver_noise
                   + u.u_cable_effects * u.u_cable_effects;

    u.u_combined = sqrt(sum_sq);
    u.u_expanded_k2 = 2.0 * u.u_combined;  /* k=2 → ~95% confidence */

    return u;
}

/* ========================================================================
 * L6: System-level insertion loss
 *
 * In a real installation, the filter is part of a chain:
 * Source → Connector → PCB trace → Filter → PCB trace → Connector → Load
 *
 * Each element has its own ABCD matrix, and the system-level IL is
 * computed by cascading all ABCD matrices.
 *
 * Additionally, the ground connection impedance plays a crucial role:
 * a high ground impedance reduces Y-cap effectiveness and can create
 * ground loops that increase noise.
 *
 * The shield effectiveness of the enclosure can add additional
 * attenuation at high frequencies.
 * ======================================================================== */

double il_system_level(const filter_system_abcd_t *sys,
                        double z_source, double z_load) {
    if (sys == NULL) return 0.0;

    /* Cascade all ABCD matrices */
    abcd_matrix_t chain[4] = {
        sys->input_connector_abcd,
        sys->pcb_trace_abcd,
        sys->filter_abcd,
        sys->output_connector_abcd
    };

    /* Note: PCB trace after filter is cascaded separately */
    abcd_matrix_t pre_filter, total_pre;
    abcd_cascade_n(chain, 2, &pre_filter);     /* input_connector + pcb_trace */

    /* Filter itself */
    /* Combine pre-filter with filter */
    total_pre = abcd_cascade_two(&pre_filter, &sys->filter_abcd);

    /* Add output connector */
    abcd_matrix_t total_abcd = abcd_cascade_two(&total_pre,
                                                  &sys->output_connector_abcd);

    /* Compute IL from total ABCD */
    double il = il_from_abcd_matrix(&total_abcd, z_source, z_load);

    /* Add shield effectiveness contribution
     * (shielding provides additional attenuation at high frequencies) */
    il += sys->shield_effectiveness_db * 0.5;  /* Rough estimate of contribution */

    return il;
}

/* ========================================================================
 * L3: Ideal theoretical maximum insertion loss
 *
 * For an ideal N-stage LC filter:
 *   |H(f)| = 1 / (1 + (f/f_c)^(2N))   (Butterworth approximation)
 *
 * For f >> f_c:
 *   IL(f) ≈ 20·N · log10(f/f_c)  [dB]
 *         = N · 20·log10(f/f_c)
 *
 * Each LC pair = 2nd order = 40 dB/decade.
 * N pairs = 40·N dB/decade.
 *
 * A CM choke with two Y-caps is 3rd order → 60 dB/decade.
 *
 * This is the theoretical maximum. In practice:
 *  - Component parasitics limit high-frequency performance
 *  - Source/load impedance mismatch reduces actual IL
 *  - Layout coupling can bypass the filter
 *
 * Reference: Ozenbaugh §2.3 "Asymptotic IL Estimation"
 * ======================================================================== */

double il_ideal_theoretical_max(int n_stages, double f_cutoff, double freq) {
    if (n_stages <= 0 || f_cutoff <= 0.0 || freq <= 0.0) return 0.0;
    if (freq <= f_cutoff) return 0.0;

    double ratio = freq / f_cutoff;
    return n_stages * 40.0 * log10(ratio);
}

/* ========================================================================
 * L7: IL margin vs EMC limit
 *
 * Computes the margin between achieved IL and required IL at
 * each frequency point:
 *
 *   Margin(f_i) = IL_achieved(f_i) - IL_required(f_i)  [dB]
 *
 * IL_required(f_i) = Noise(f_i) - Limit(f_i) + Design_Margin
 *
 * Positive margin → filter meets requirements.
 * Negative margin → filter FAILS at that frequency.
 *
 * This is the final verification step in filter design.
 * ======================================================================== */

double *il_margin_vs_limit(const il_sweep_t *sweep,
                            const emc_limit_t *limit,
                            const noise_spectrum_t *noise,
                            size_t *n_out) {
    if (sweep == NULL || limit == NULL || noise == NULL || n_out == NULL) {
        return NULL;
    }

    *n_out = sweep->num_points;
    double *margin = (double *)malloc(*n_out * sizeof(double));
    if (margin == NULL) return NULL;

    emc_limit_summary_t ls = emc_limit_summary_get(limit->standard,
                                                    limit->is_class_a);

    for (size_t i = 0; i < sweep->num_points; i++) {
        double f = sweep->frequencies[i];

        /* Find noise level at this frequency (closest bin) */
        double noise_level = 0.0;
        for (size_t j = 0; j < noise->num_bins; j++) {
            if (fabs(f - noise->frequencies[j]) < f * 0.01) {
                noise_level = noise->amplitude_dbuV[j];
                break;
            }
        }

        /* Find limit at this frequency */
        double limit_level;
        if (f < 500e3) {
            limit_level = ls.limit_150k_qp_dbuV;
            if (limit_level != ls.limit_500k_qp_dbuV) {
                double t = (f - 150e3) / 350e3;
                limit_level = ls.limit_150k_qp_dbuV
                    + t * (ls.limit_500k_qp_dbuV - ls.limit_150k_qp_dbuV);
            }
        } else if (f < 5e6) {
            limit_level = ls.limit_500k_qp_dbuV;
        } else {
            limit_level = ls.limit_5M_qp_dbuV;
        }

        /* IL_achieved: use worst of CM and DM at this frequency */
        double il_achieved = (sweep->il_cm_db[i] < sweep->il_dm_db[i]) ?
                             sweep->il_cm_db[i] : sweep->il_dm_db[i];

        /* IL_required */
        double il_req = noise_level - limit_level + 6.0;  /* 6 dB margin */

        margin[i] = il_achieved - il_req;
    }

    return margin;
}

/* ========================================================================
 * L3: Theoretical IL computation
 *
 * IL_theoretical computes IL using the s-domain transfer function
 * method rather than numerical frequency sweep. This is the
 * analytical approach used in filter textbooks.
 *
 * Given the DM LC parameters and source/load impedances, we
 * evaluate the exact transfer function at the given complex
 * frequency (typically s = jω for steady-state analysis).
 * ======================================================================== */

double il_theoretical(const dm_lc_params_t *params,
                       il_theory_config_t config, double freq) {
    if (params == NULL) return 0.0;

    /* Build ABCD matrix for the topology */
    abcd_matrix_t abcd = build_filter_abcd(params->topology,
        params->l_series,
        params->c_shunt_input,
        params->l_series2,
        params->c_shunt_output,
        freq);

    /* Use real or complex source/load */
    double zs = config.z_source;
    double zl = config.z_load;

    if (config.use_complex_source) {
        /* Complex source/load → compute IL from ABCD */
        return il_from_abcd_matrix(&abcd, complex_mag(config.z_source_cmplx),
                                    complex_mag(config.z_load_cmplx));
    }

    return il_from_abcd_matrix(&abcd, zs, zl);
}
