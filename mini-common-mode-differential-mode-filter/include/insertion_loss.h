/**
 * @file insertion_loss.h
 * @brief Insertion Loss — The Primary EMI Filter Figure of Merit
 *
 * Insertion loss (IL) is the fundamental metric for EMI filter performance.
 * It measures the reduction in noise power delivered to the load when the
 * filter is inserted between source and load.
 *
 * Definition (L1):
 *   IL = 10·log₁₀(P_without / P_with)  = 20·log₁₀(|V_without| / |V_with|)  [dB]
 *
 * Measurement setup per CISPR 17:
 *   Source (Z_S) → Filter → Load (Z_L)
 *   V_without: measured with filter replaced by a through connection
 *   V_with:    measured with filter in place
 *
 * This file covers:
 *  - Theoretical IL from network parameters (L2-L3)
 *  - Frequency-swept IL computation (L3)
 *  - CM vs DM insertion loss separation (L2)
 *  - IL measurement uncertainty analysis (L6)
 *  - IL in cascaded multi-stage filters (L5)
 *  - High-frequency IL degradation from parasitics (L6)
 *
 * Reference:
 *  - CISPR 17: Methods of measurement of the suppression characteristics
 *    of passive EMC filtering devices
 *  - Paul, C.R. "Introduction to EMC" — Chapter 6.3
 *  - IEC 55017 / EN 55017
 */

#ifndef INSERTION_LOSS_H
#define INSERTION_LOSS_H

#include "cm_dm_filter.h"
#include "cm_dm_network.h"
#include "dm_filter_model.h"
#include "cm_choke_model.h"
#include "filter_design_params.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * L1: Insertion loss types
 * ======================================================================== */

/** @brief Insertion loss mode */
typedef enum {
    IL_MODE_CM = 0,             /**< Common-mode insertion loss */
    IL_MODE_DM = 1,             /**< Differential-mode insertion loss */
    IL_MODE_TOTAL = 2,          /**< Combined CM+DM (worst-case per frequency) */
    IL_MODE_CM_DM_SEPARATED = 3 /**< Both CM and DM reported separately */
} il_mode_t;

/** @brief Complete frequency-swept insertion loss measurement/simulation */
typedef struct {
    double *frequencies;        /**< Frequency sweep points [Hz] */
    double *il_cm_db;           /**< CM insertion loss at each frequency [dB] */
    double *il_dm_db;           /**< DM insertion loss at each frequency [dB] */
    size_t num_points;          /**< Number of frequency points */
    double f_start;             /**< Start frequency [Hz] */
    double f_stop;              /**< Stop frequency [Hz] */
    double il_cm_min;           /**< Minimum CM IL in sweep [dB] */
    double il_dm_min;           /**< Minimum DM IL in sweep [dB] */
    double il_cm_at_150k;       /**< CM IL at 150 kHz (typical reference) [dB] */
    double il_dm_at_150k;       /**< DM IL at 150 kHz [dB] */
    double il_cm_at_500k;       /**< CM IL at 500 kHz [dB] */
    double il_dm_at_500k;       /**< DM IL at 500 kHz [dB] */
    double il_cm_at_1M;         /**< CM IL at 1 MHz [dB] */
    double il_dm_at_1M;         /**< DM IL at 1 MHz [dB] */
    double il_cm_at_10M;        /**< CM IL at 10 MHz [dB] */
    double il_dm_at_10M;        /**< DM IL at 10 MHz [dB] */
    int is_valid;               /**< Nonzero if sweep computed successfully */
} il_sweep_t;

/* ========================================================================
 * L2: IL measurement conditions (CISPR 17 standard)
 * ======================================================================== */

/**
 * @brief CISPR 17 measurement setup.
 *
 * Standard specifies four source/load impedance combinations:
 *  1. 50 Ω / 50 Ω    — nominal
 *  2. 0.1 Ω / 100 Ω — worst-case source for DM
 *  3. 100 Ω / 0.1 Ω — worst-case load for DM
 *  4. 0.1 Ω / 0.1 Ω — worst-case for CM  (optional)
 *  5. 100 Ω / 100 Ω — worst-case for CM (optional)
 */
typedef struct {
    double z_source;            /**< Source impedance magnitude [Ω] */
    double z_load;              /**< Load impedance magnitude [Ω] */
    char label[64];             /**< Human-readable condition label */
    int is_standard;            /**< Nonzero if this is a CISPR 17 condition */
} il_meas_condition_t;

/** @brief IL results across all standard conditions */
typedef struct {
    il_sweep_t sweep_50_50;     /**< 50Ω/50Ω sweep */
    il_sweep_t sweep_01_100;    /**< 0.1Ω/100Ω sweep */
    il_sweep_t sweep_100_01;    /**< 100Ω/0.1Ω sweep */
    double worst_case_cm_150k;  /**< Worst CM IL at 150 kHz across conditions */
    double worst_case_dm_150k;  /**< Worst DM IL at 150 kHz across conditions */
    double worst_case_cm_1M;    /**< Worst CM IL at 1 MHz */
    double worst_case_dm_1M;    /**< Worst DM IL at 1 MHz */
    int num_conditions;         /**< Number of conditions evaluated */
} il_cispr17_results_t;

/* ========================================================================
 * L3: Theoretical IL from s-domain transfer functions
 * ======================================================================== */

/**
 * @brief Parameters for theoretical IL computation.
 *
 * IL depends on ALL four impedances in the system:
 *   Z_S — source impedance
 *   Z_L — load impedance
 *   Z_11, Z_12, Z_21, Z_22 — filter Z-parameters
 *
 * The voltage transfer ratio is:
 *   |V_L(with)|   |  Z_L · Z_21  |
 *   |─────────| = |───────────────|
 *   |V_S       |   |(Z_11+Z_S)(Z_22+Z_L) - Z_12·Z_21|
 *
 * Without filter (through connection):
 *   |V_L(without)|   |  Z_L  |
 *   |────────────| = |───────|
 *   |V_S         |   |Z_S+Z_L|
 *
 * Therefore:
 *   IL = 20·log10( |(Z_S+Z_L)·Z_21 / ((Z_11+Z_S)(Z_22+Z_L) - Z_12·Z_21)| )
 */
typedef struct {
    double z_source;            /**< Source impedance [Ω] */
    double z_load;              /**< Load impedance [Ω] */
    int use_complex_source;     /**< Nonzero if source is complex impedance */
    complex_t z_source_cmplx;   /**< Complex source impedance [Ω] */
    complex_t z_load_cmplx;     /**< Complex load impedance [Ω] */
} il_theory_config_t;

/* ========================================================================
 * L6: High-frequency IL degradation
 * ======================================================================== */

/**
 * @brief HF degradation mechanism.
 *
 * At high frequencies, filter IL degrades (decreases) due to:
 *  1. Parasitic coupling (capacitive and inductive)
 *  2. Component self-resonance
 *  3. Layout parasitics (trace inductance, ground plane impedance)
 *  4. CM choke inter-winding capacitance
 *  5. Y-cap ESL to ground connection inductance
 *
 * The "IL shelf" is the asymptotic high-frequency IL determined
 * by the parasitic coupling, not the designed filter response.
 */
typedef struct {
    double shelf_frequency;     /**< Frequency where IL flattens [Hz] */
    double shelf_il_cm_db;     /**< CM IL shelf level [dB] */
    double shelf_il_dm_db;     /**< DM IL shelf level [dB] */
    double parasitic_c_f;      /**< Dominant parasitic capacitance [F] */
    double parasitic_l_h;      /**< Dominant parasitic inductance [H] */
    double max_usable_freq;    /**< Maximum frequency with useful IL [Hz] */
} il_hf_degradation_t;

/* ========================================================================
 * L6: IL measurement uncertainty budget
 * ======================================================================== */

/**
 * @brief Uncertainty contributions per ISO/IEC 17025 (GUM).
 */
typedef struct {
    double u_impedance_mismatch;  /**< Uncertainty from impedance mismatch [dB] */
    double u_calibration;         /**< Calibration uncertainty [dB] */
    double u_receiver_noise;      /**< Receiver noise floor contribution [dB] */
    double u_cable_effects;       /**< Cable and connector effects [dB] */
    double u_repeatability;       /**< Repeatability (Type A) [dB] */
    double u_combined;            /**< Combined standard uncertainty [dB] */
    double u_expanded_k2;         /**< Expanded uncertainty (k=2) [dB] */
} il_uncertainty_t;

/* ========================================================================
 * L7: Filter-connector-system IL
 * ======================================================================== */

/**
 * @brief Complete system insertion loss including connectors and PCB.
 *
 * In practice, the filter is not an isolated component — it's embedded
 * in a system with connectors, PCB traces, and shielding structures.
 * This models the system-level IL including these effects.
 */
typedef struct {
    abcd_matrix_t filter_abcd;          /**< Filter ABCD matrix */
    abcd_matrix_t input_connector_abcd; /**< Input connector ABCD */
    abcd_matrix_t output_connector_abcd;/**< Output connector ABCD */
    abcd_matrix_t pcb_trace_abcd;       /**< PCB trace ABCD */
    complex_t z_ground_connection;      /**< Ground connection impedance [Ω] */
    double shield_effectiveness_db;     /**< Shielding contribution [dB] */
} filter_system_abcd_t;

/* ========================================================================
 * API FUNCTIONS
 * ======================================================================== */

/**
 * @brief Compute insertion loss from Z-parameters and source/load impedances.
 *
 * L1-L3: Complete IL expression from two-port network theory.
 *
 * IL = 20·log10(| (Z_S+Z_L)·Z_21 / [(Z_11+Z_S)(Z_22+Z_L) - Z_12·Z_21] |)
 *
 * @param z        Filter Z-matrix (2×2)
 * @param z_source Source impedance [Ω] (real)
 * @param z_load   Load impedance [Ω] (real)
 * @return Insertion loss [dB]. Returns 0.0 if Z_21≈0.
 */
double il_from_z_matrix(const z_matrix_t *z, double z_source, double z_load);

/**
 * @brief Compute insertion loss from S-parameters.
 *
 * IL = -20·log10(|S₂₁|) * √(Z₀_S / Z₀_L)
 *
 * When both ports reference to Z₀: IL = -20·log10(|S₂₁|)
 */
double il_from_s_matrix(const s_matrix_t *s);

/**
 * @brief Compute insertion loss from ABCD matrix.
 *
 * IL = 20·log10( |A·Z_L + B + C·Z_S·Z_L + D·Z_S| / |Z_S + Z_L| )
 */
double il_from_abcd_matrix(const abcd_matrix_t *abcd,
                            double z_source, double z_load);

/**
 * @brief Perform a frequency sweep of insertion loss.
 *
 * L3: Sweeps from f_start to f_stop with logarithmically-spaced points.
 * Computes IL at each frequency using full network analysis.
 *
 * @param filter_elements Array of filter elements
 * @param n_elem          Number of elements
 * @param f_start         Start frequency [Hz]
 * @param f_stop          Stop frequency [Hz]
 * @param num_points      Number of frequency points
 * @param z_source_cm     CM source impedance [Ω]
 * @param z_source_dm     DM source impedance [Ω]
 * @param z_load_cm       CM load impedance [Ω]
 * @param z_load_dm       DM load impedance [Ω]
 * @return Complete IL sweep [dynamically allocated]
 * Complexity: O(N · E) where N=num_points, E=network complexity
 */
il_sweep_t *il_frequency_sweep(const filter_element_t *filter_elements,
                                size_t n_elem,
                                double f_start, double f_stop,
                                size_t num_points,
                                double z_source_cm, double z_source_dm,
                                double z_load_cm, double z_load_dm);

/**
 * @brief Compute insertion loss under all CISPR 17 standard conditions.
 *
 * CISPR 17 requires IL measurement under 4 impedance conditions:
 *   50/50, 0.1/100, 100/0.1, and optionally 0.1/0.1 and 100/100.
 *
 * @return CISPR 17 results [dynamically allocated]
 */
il_cispr17_results_t *il_cispr17_comprehensive(
    const filter_element_t *elements, size_t n_elem,
    double f_start, double f_stop, size_t num_points);

/**
 * @brief Compute theoretical IL using the s-domain transfer function method.
 *
 * Uses filter ABCD parameters computed from element values and topology.
 * This is the analytical method vs the numerical sweep method.
 */
double il_theoretical(const dm_lc_params_t *params,
                       il_theory_config_t config, double freq);

/**
 * @brief Estimate high-frequency IL degradation.
 *
 * L6: Above ~10 MHz, parasitic coupling dominates and IL plateaus.
 * This function estimates the shelf level and frequency.
 *
 * @param cm_choke  CM choke model (for parasitic capacitance)
 * @param y_caps    Array of Y-capacitor elements
 * @param n_y_caps  Number of Y-caps
 * @param trace_len_mm Length of PCB trace between filter and connector [mm]
 */
il_hf_degradation_t il_hf_degradation_estimate(
    const cm_choke_full_t *cm_choke,
    const filter_element_t *y_caps, size_t n_y_caps,
    double trace_len_mm);

/**
 * @brief Compute insertion loss measurement uncertainty budget.
 *
 * L6: Following ISO/IEC 17025 (GUM), estimates Type A (repeatability)
 * and Type B (instrument, mismatch, cable) uncertainties and combines
 * them into an expanded uncertainty (k=2, 95% confidence).
 *
 * @param il_nominal     Measured IL [dB]
 * @param vswr_source    Source VSWR (typ. 1.2)
 * @param vswr_load      Load VSWR (typ. 1.2)
 * @param receiver_snr   Receiver SNR at measurement freq [dB]
 * @param repeat_stddev  Standard deviation of repeated measurements [dB]
 */
il_uncertainty_t il_measurement_uncertainty(double il_nominal,
                                             double vswr_source,
                                             double vswr_load,
                                             double receiver_snr,
                                             double repeat_stddev);

/**
 * @brief Estimate system-level IL including connectors and PCB.
 *
 * L7: Cascaded ABCD parameters for the complete signal chain:
 *   Source → Connector → PCB trace → Filter → PCB trace → Connector → Load
 */
double il_system_level(const filter_system_abcd_t *sys,
                        double z_source, double z_load);

/**
 * @brief Compute the maximum theoretical insertion loss for an ideal
 * N-stage LC filter.
 *
 * For an N-stage filter (N = number of L-C pairs):
 *   IL_max(f) = N · 40·log10(f / f_c)  [dB]  for f >> f_c
 *
 * Each LC pair provides 40 dB/decade asymptotic slope.
 * CM choke + 2 Y-caps gives 60 dB/decade.
 *
 * @param n_stages   Number of LC stages
 * @param f_cutoff   Cutoff frequency [Hz]
 * @param freq       Frequency of interest [Hz]
 * @return Maximum theoretical IL [dB] (ignoring parasitics)
 */
double il_ideal_theoretical_max(int n_stages, double f_cutoff, double freq);

/**
 * @brief Compute the IL margin: how much the filter exceeds requirements.
 *
 * Margin(f) = IL_achieved(f) - IL_required(f)  [dB]
 * Positive margin = passes with room to spare.
 * Negative margin = FAIL.
 *
 * @param sweep     Measured/Simulated IL sweep
 * @param limit     EMC limit line
 * @param noise     Pre-filter noise spectrum
 * @return Array of margins [dB] at each frequency point [caller frees]
 */
double *il_margin_vs_limit(const il_sweep_t *sweep,
                            const emc_limit_t *limit,
                            const noise_spectrum_t *noise,
                            size_t *n_out);

/**
 * @brief Free IL sweep data.
 */
void il_sweep_free(il_sweep_t *sweep);

/**
 * @brief Free CISPR 17 results.
 */
void il_cispr17_results_free(il_cispr17_results_t *r);

#ifdef __cplusplus
}
#endif

#endif /* INSERTION_LOSS_H */
