#ifndef NF_DIAGNOSTIC_H
#define NF_DIAGNOSTIC_H
/**
 * @file nf_diagnostic.h
 * @brief EMC Diagnostic Algorithms & Application Models (L6-L8)
 *
 * L6: Canonical diagnostic problems — source identification, emission
 *     ranking, current path tracing, emission mechanism classification
 * L7: Applications — PCB diagnostic workflow, IC-level debugging,
 *     automotive EMC, medical device EMC, smartphone EMI
 * L8: Advanced topics — stochastic field analysis, time-varying
 *     source detection, Bayesian source localization, machine learning
 *     for EMI classification
 *
 * Ref: Clayton R. Paul, "Introduction to EMC", 2006
 *      Henry W. Ott, "Electromagnetic Compatibility Engineering", 2009
 *      Mark I. Montrose, "EMC and the Printed Circuit Board", 1999
 *      IEC 61967 / IEC 62132 (IC EMC standards)
 */

#include <complex.h>
#include <stddef.h>
#include "nf_probe_core.h"
#include "nf_field_theory.h"
#include "nf_scanning.h"
#include "nf_signal_processing.h"

/* ============================================================================
 * L6 — Canonical Diagnostic Problems
 * ============================================================================
 */

/* ---------- EMI Source Identification ---------- */

typedef enum {
    NF_SOURCE_CLOCK_HARMONIC = 0,
    NF_SOURCE_SWITCHING_NOISE = 1,
    NF_SOURCE_CROSSTALK = 2,
    NF_SOURCE_GROUND_BOUNCE = 3,
    NF_SOURCE_PDN_RESONANCE = 4,
    NF_SOURCE_CABLE_CM_RADIATION = 5,
    NF_SOURCE_SLOT_COUPLING = 6,
    NF_SOURCE_ESD_TRANSIENT = 7,
    NF_SOURCE_UNKNOWN = 8
} nf_emi_source_type_t;

typedef struct {
    nf_emi_source_type_t type;
    double freq_hz;
    double magnitude_dbuV;
    double x_m;
    double y_m;
    double z_m;
    double confidence;
    char description[256];
} nf_emi_source_t;

typedef struct {
    nf_emi_source_t *sources;
    size_t num_sources;
    size_t capacity;
    double *source_correlation_matrix;
    size_t num_correlation_pairs;
} nf_source_catalog_t;

/* ---------- Emission Ranking (L6) ---------- */

typedef struct {
    size_t source_index;
    double risk_score;
    double margin_db;
    double limit_dbuV;
    int    pass_fail;
    int    priority;
} nf_emission_rank_t;

/* ---------- Current Path Tracing (L6) ---------- */

typedef struct {
    double complex *current_map_x;
    double complex *current_map_y;
    size_t map_size;
    double *current_magnitude;
    double max_current_A;
    double *divergence_map;
    double *source_density;
    nf_real_vector_t *current_direction;
} nf_current_path_t;

/* ---------- Emission Mechanism Classifier (L6) ---------- */

typedef struct {
    int is_differential_mode;
    int is_common_mode;
    int is_wave_impedance_low;
    int is_wave_impedance_high;
    double dm_cm_ratio_db;
    double wave_impedance_ohm;
    double near_field_phase_diff_deg;
    char classification[128];
} nf_emission_mechanism_t;

/* ============================================================================
 * L7 — Applications
 * ============================================================================
 */

/* ---------- PCB Diagnostic Workflow (L7) ---------- */

typedef struct {
    char pcb_name[128];
    char revision[32];
    double board_width_m;
    double board_length_m;
    int    num_layers;
    double stackup_height_m;
    char stackup_materials[10][64];
    int    num_materials;
    nf_scan_dataset_t *scan_data;
    nf_source_catalog_t *source_catalog;
    nf_emission_rank_t *ranking;
    size_t num_ranked_sources;
    double overall_margin_db;
    int    pass_fcc_class_b;
    int    pass_cispr_22_class_b;
    int    pass_cispr_25;
} nf_pcb_diagnostic_t;

/* ---------- IC-Level EMC Debugging (L7) ---------- */

typedef struct {
    char ic_part_number[64];
    char package_type[32];
    double package_width_m;
    double package_length_m;
    double die_size_m;
    double clock_frequency_hz;
    double supply_voltage_V;
    double current_draw_A;
    int    num_pins;
    double *pin_emission_dbuV;
    size_t num_measured_pins;
    nf_scan_dataset_t *near_field_scan;
    nf_source_catalog_t *emission_sources;
    char debug_recommendation[1024];
} nf_ic_diagnostic_t;

/* ---------- Automotive EMC Diagnostic (L7) ---------- */

typedef struct {
    char vehicle_ecu_name[128];
    char standard[32];
    double freq_range_lo_hz;
    double freq_range_hi_hz;
    double radiated_limit_dbuVpm;
    double conducted_limit_dbuV;
    int    bci_test_level_mA;
    int    ri_test_level_Vpm;
    double *radiated_emissions;
    double *conducted_emissions;
    size_t num_freq_points;
    int    pass_radiated;
    int    pass_conducted;
    char countermeasures[1024];
} nf_automotive_diag_t;

/* ---------- Medical Device EMC (L7) ---------- */

typedef struct {
    char device_name[128];
    char device_class[16];
    char iec_standard[32];
    double immunity_level_Vpm;
    double emission_limit_dbuVpm;
    int    essential_performance_affected;
    double safety_margin_db;
    double *emission_spectrum;
    size_t num_freq_points;
    char risk_assessment[512];
} nf_medical_emc_t;

/* ---------- Smartphone EMI Debugging (L7) ---------- */

typedef struct {
    char phone_model[64];
    double display_freq_hz;
    double touch_freq_hz;
    double cpu_clock_hz;
    double memory_clock_hz;
    double rf_tx_freq_hz;
    double rf_tx_power_dbm;
    double desired_signal_level_dbm;
    double desense_db;
    double *nfc_emission_spectrum;
    size_t num_nfc_bins;
    int    self_interference_detected;
    char interference_source[128];
} nf_smartphone_emi_t;

/* ============================================================================
 * L7 Continued — More Applications
 * ============================================================================
 */

/* ---------- Aerospace EMC (L7) — RTCA DO-160 / Boeing ---------- */

typedef struct {
    char aircraft_system[128];
    char do160_section[32];
    double freq_range_lo_hz;
    double freq_range_hi_hz;
    double category_limit_dbuVpm;
    int    hIRF_test_level_Vpm;
    double *emission_data;
    size_t num_data_points;
    int    pass_do160;
    double worst_case_margin_db;
} nf_aerospace_emc_t;

/* ---------- Industrial Motor Drive EMI (L7) — IEC 61800-3 / Toyota ---------- */

typedef struct {
    char drive_model[64];
    double rated_power_kW;
    double switching_freq_hz;
    double dc_bus_voltage_V;
    double output_current_A;
    double *conducted_emi_spectrum;
    double *radiated_emi_spectrum;
    size_t num_spectrum_points;
    double filter_attenuation_db;
    int    pass_iec61800;
    double motor_cable_length_m;
} nf_motor_drive_emi_t;

/* ---------- 5G Base Station EMC (L7) — ISO / Supplier Qualification ---------- */

typedef struct {
    char base_station_model[64];
    double freq_tx_lo_hz;
    double freq_tx_hi_hz;
    double tx_power_per_channel_dbm;
    int    num_tx_channels;
    double spurious_emission_limit_dbm;
    double *spurious_spectrum;
    size_t num_spurious_bins;
    double adjacent_channel_power_dbm;
    double alternate_channel_power_dbm;
} nf_5g_bs_emc_t;

/* ============================================================================
 * L8 — Advanced Topics
 * ============================================================================
 */

/* ---------- Stochastic Near-Field Analysis (L8) ---------- */

typedef struct {
    double *field_samples_over_time;
    size_t num_time_samples;
    size_t num_spatial_points;
    double *mean_field;
    double *variance_field;
    double *skewness_field;
    double *kurtosis_field;
    double *temporal_correlation;
    double *spatial_correlation_length;
    int    is_stationary;
    double stationarity_p_value;
} nf_stochastic_field_t;

/* ---------- Bayesian Source Localization (L8) ---------- */

typedef struct {
    double *prior_distribution;
    double *likelihood_function;
    double *posterior_distribution;
    size_t num_hypothesis_points;
    nf_real_vector_t *candidate_source_positions;
    double most_likely_x_m;
    double most_likely_y_m;
    double confidence_ellipse_major;
    double confidence_ellipse_minor;
    double confidence_ellipse_angle;
} nf_bayesian_localizer_t;

/* ---------- Machine Learning EMI Classifier (L8) ---------- */

typedef enum {
    NF_ML_MODEL_KNN = 0,
    NF_ML_MODEL_SVM = 1,
    NF_ML_MODEL_RANDOM_FOREST = 2
} nf_ml_model_type_t;

typedef struct {
    nf_ml_model_type_t model_type;
    double *training_features;
    int    *training_labels;
    size_t num_training_samples;
    size_t num_features;
    double *svm_weights;
    double svm_bias;
    double *prototype_vectors;
    size_t num_prototypes;
    int    *prototype_labels;
    int    trained;
    double training_accuracy;
} nf_ml_emi_classifier_t;

/* ---------- Time-Varying Near-Field (L8) ---------- */

typedef struct {
    double complex *field_time_series;
    size_t num_time_steps;
    double dt_s;
    double *instantaneous_frequency;
    double *instantaneous_amplitude;
    double *hilbert_envelope;
    double *zero_crossing_times;
    size_t num_zero_crossings;
} nf_time_varying_field_t;

/* ---------- Adaptive Probe Positioning (L8) — Lyapunov-based ---------- */

typedef struct {
    double current_x_m;
    double current_y_m;
    double current_z_m;
    double target_field_strength;
    double *field_gradient_x;
    double *field_gradient_y;
    double step_size;
    double convergence_threshold;
    int    converged;
    size_t iterations;
    double lyapunov_function_value;
} nf_adaptive_probe_pos_t;

/* ============================================================================
 * API — Diagnostic Functions
 * ============================================================================
 */

/* Source catalog management */
int  nf_source_catalog_init(nf_source_catalog_t *cat, size_t capacity);
void nf_source_catalog_free(nf_source_catalog_t *cat);
int  nf_source_catalog_add(nf_source_catalog_t *cat,
                            nf_emi_source_type_t type,
                            double freq, double mag,
                            double x, double y, double z,
                            double confidence, const char *desc);
int  nf_source_catalog_rank(const nf_source_catalog_t *cat,
                             const double *limits_dbuV,
                             nf_emission_rank_t *ranking,
                             size_t *num_ranked);

/* Emission mechanism classification */
int  nf_emission_mechanism_classify(const nf_field_sample_t *sample,
                                     double freq_hz,
                                     nf_emission_mechanism_t *result);

/* Current path reconstruction from H-field measurements */
int  nf_current_path_reconstruct(const nf_field_sample_t *H_samples,
                                  const nf_scan_grid_t *grid,
                                  nf_current_path_t *path);
void nf_current_path_free(nf_current_path_t *path);

/* ============================================================================
 * API — Application-Level Functions (L7)
 * ============================================================================
 */

/* PCB diagnostic */
int  nf_pcb_diagnostic_init(nf_pcb_diagnostic_t *pcb,
                              const char *name, const char *rev,
                              double width, double height);
int  nf_pcb_diagnostic_evaluate(nf_pcb_diagnostic_t *pcb,
                                 const nf_scan_dataset_t *scan,
                                 double fcc_class_b_limit_dbuVpm,
                                 double cispr22_class_b_limit_dbuVpm,
                                 double cispr25_limit_dbuVpm);
void nf_pcb_diagnostic_free(nf_pcb_diagnostic_t *pcb);

/* IC diagnostic */
int  nf_ic_diagnostic_init(nf_ic_diagnostic_t *ic,
                             const char *part_number,
                             double clock_freq_hz,
                             double supply_V, double current_A);
int  nf_ic_diagnostic_analyze(nf_ic_diagnostic_t *ic,
                                const nf_scan_dataset_t *scan);
void nf_ic_diagnostic_free(nf_ic_diagnostic_t *ic);

/* Automotive diagnostic */
int  nf_automotive_diag_init(nf_automotive_diag_t *auto_d,
                               const char *ecu_name,
                               const char *standard,
                               double flo, double fhi);
int  nf_automotive_diag_evaluate(nf_automotive_diag_t *auto_d,
                                  const double *radiated_data,
                                  const double *conducted_data,
                                  size_t n);
void nf_automotive_diag_free(nf_automotive_diag_t *auto_d);

/* Medical device EMC */
int  nf_medical_emc_init(nf_medical_emc_t *med,
                           const char *name, const char *cls,
                           const char *iec_std);
int  nf_medical_emc_evaluate(nf_medical_emc_t *med,
                              const double *spectrum, size_t n,
                              double limit, double immunity_level);
void nf_medical_emc_free(nf_medical_emc_t *med);

/* Smartphone EMI */
int  nf_smartphone_emi_init(nf_smartphone_emi_t *phone,
                              const char *model,
                              double cpu_clk, double rf_freq,
                              double rf_power);
int  nf_smartphone_emi_detect_desense(nf_smartphone_emi_t *phone,
                                       const double *nfc_spectrum,
                                       size_t n, double noise_floor);
void nf_smartphone_emi_free(nf_smartphone_emi_t *phone);

/* Aerospace EMC */
int  nf_aerospace_emc_init(nf_aerospace_emc_t *aero,
                             const char *system, const char *section);
int  nf_aerospace_emc_evaluate(nf_aerospace_emc_t *aero,
                                const double *emissions, size_t n,
                                double limit);
void nf_aerospace_emc_free(nf_aerospace_emc_t *aero);

/* Motor drive EMI */
int  nf_motor_drive_emi_init(nf_motor_drive_emi_t *drive,
                               const char *model, double power_kW,
                               double fsw_hz);
int  nf_motor_drive_emi_evaluate(nf_motor_drive_emi_t *drive,
                                  const double *conducted, size_t nc,
                                  const double *radiated, size_t nr,
                                  double conducted_limit,
                                  double radiated_limit);
void nf_motor_drive_emi_free(nf_motor_drive_emi_t *drive);

/* 5G base station EMC */
int  nf_5g_bs_emc_init(nf_5g_bs_emc_t *bs,
                         const char *model,
                         double flo, double fhi, double tx_power);
int  nf_5g_bs_emc_evaluate(nf_5g_bs_emc_t *bs,
                            const double *spurious, size_t n,
                            double limit);
void nf_5g_bs_emc_free(nf_5g_bs_emc_t *bs);

/* ============================================================================
 * API — Advanced Topic Functions (L8)
 * ============================================================================
 */

/* Stochastic field analysis */
int  nf_stochastic_field_init(nf_stochastic_field_t *sf,
                               size_t num_time, size_t num_spatial);
void nf_stochastic_field_free(nf_stochastic_field_t *sf);
int  nf_stochastic_field_analyze(nf_stochastic_field_t *sf);
int  nf_stochastic_stationarity_test(const double *time_series,
                                      size_t N, size_t num_segments,
                                      double *p_value);

/* Bayesian source localization */
int  nf_bayesian_localize_init(nf_bayesian_localizer_t *bl,
                                const nf_scan_grid_t *grid,
                                const nf_field_sample_t *measurements,
                                size_t num_candidates);
void nf_bayesian_localize_free(nf_bayesian_localizer_t *bl);
int  nf_bayesian_localize_run(nf_bayesian_localizer_t *bl);
int  nf_bayesian_localize_update(nf_bayesian_localizer_t *bl,
                                  const nf_field_sample_t *new_measurement);

/* ML EMI classifier */
int  nf_ml_classifier_init(nf_ml_emi_classifier_t *ml,
                            nf_ml_model_type_t type,
                            size_t num_features);
void nf_ml_classifier_free(nf_ml_emi_classifier_t *ml);
int  nf_ml_classifier_train(nf_ml_emi_classifier_t *ml,
                             const double *features,
                             const int *labels,
                             size_t n_samples);
int  nf_ml_classifier_predict(const nf_ml_emi_classifier_t *ml,
                               const double *features,
                               int *label, double *confidence);
int  nf_ml_classifier_knn_predict(const nf_ml_emi_classifier_t *ml,
                                   const double *features,
                                   int *label, double *confidence);
int  nf_ml_classifier_svm_predict(const double *weights, double bias,
                                   const double *features,
                                   size_t num_features,
                                   int *label, double *margin);

/* Time-varying field analysis */
int  nf_time_varying_field_init(nf_time_varying_field_t *tvf,
                                 size_t num_steps, double dt);
void nf_time_varying_field_free(nf_time_varying_field_t *tvf);
int  nf_time_varying_field_analyze(nf_time_varying_field_t *tvf);
int  nf_hilbert_envelope(const double *real_signal, size_t N,
                          double *envelope);

/* Adaptive probe positioning (Lyapunov-based gradient ascent) */
int  nf_adaptive_probe_init(nf_adaptive_probe_pos_t *ap,
                              double start_x, double start_y,
                              double start_z, double step_size);
int  nf_adaptive_probe_step(nf_adaptive_probe_pos_t *ap,
                             double field_strength,
                             double grad_x, double grad_y);

/* Monte Carlo uncertainty propagation for near-field measurements */
int  nf_monte_carlo_uncertainty(const double *nominal_params,
                                 const double *param_stddevs,
                                 size_t num_params,
                                 double (*forward_model)(const double*),
                                 size_t num_trials,
                                 double *mean_output,
                                 double *std_output,
                                 double *confidence_interval_95);

#endif /* NF_DIAGNOSTIC_H */
