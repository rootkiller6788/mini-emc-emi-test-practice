#ifndef NF_SCANNING_H
#define NF_SCANNING_H
/**
 * @file nf_scanning.h
 * @brief Near-Field Scanning & Imaging Methods (L5-L6)
 *
 * L5: Scanning algorithms — raster scan, adaptive scan, compressive sampling,
 *     spatial Nyquist criterion, probe trajectory optimization
 * L6: Canonical problems — microstrip line mapping, IC emission hot-spot
 *     localization, differential pair radiation, ground slot coupling
 *
 * Ref: David M. Pozar, "Microwave Engineering", 4th ed, 2012
 *      Constantine A. Balanis, "Antenna Theory", 4th ed, 2016
 *      IEC 61967-3, "Surface scan method for radiated emissions"
 */

#include <complex.h>
#include <stddef.h>
#include "nf_probe_core.h"
#include "nf_field_theory.h"

/* ============================================================================
 * L5 — Scanning Algorithms
 * ============================================================================
 */

/* ---------- Scan Pattern Types ---------- */

typedef enum {
    NF_SCAN_RASTER = 0,
    NF_SCAN_SERPENTINE = 1,
    NF_SCAN_SPIRAL = 2,
    NF_SCAN_ADAPTIVE = 3,
    NF_SCAN_RANDOM = 4,
    NF_SCAN_COMPRESSIVE = 5
} nf_scan_pattern_t;

/* ---------- Scan Configuration ---------- */

typedef struct {
    nf_scan_grid_t grid;
    nf_scan_pattern_t pattern;
    double freq_start_hz;
    double freq_stop_hz;
    double freq_step_hz;
    size_t num_frequencies;
    double dwell_time_s;
    double probe_speed_ms;
    int    bidirectional;
    double acceleration_ms2;
    double settling_time_s;
} nf_scan_config_t;

/* ---------- Scan Result (Single Frequency, Full Grid) ---------- */

typedef struct {
    nf_scan_grid_t grid;
    double freq_hz;
    nf_field_sample_t *field_samples;
    size_t num_samples;
    double scan_duration_s;
    int    scan_complete;
} nf_scan_frame_t;

/* ---------- Full Scan Dataset (Multiple Frequencies) ---------- */

typedef struct {
    nf_scan_config_t config;
    nf_scan_frame_t *frames;
    size_t num_frames;
    char dut_name[128];
    char operator_name[64];
    char scan_date[32];
    double temperature_C;
    int    valid;
} nf_scan_dataset_t;

/* ---------- Adaptive Scan Parameters (L5) ---------- */

typedef struct {
    double refinement_threshold_db;
    double coarsening_threshold_db;
    int    max_refinement_levels;
    int    current_level;
    double min_grid_spacing_m;
    double *importance_map;
    size_t importance_map_size;
} nf_adaptive_scan_t;

/* ---------- Compressive Sampling Parameters (L5) ---------- */

typedef struct {
    double sparsity_ratio;
    size_t num_measurements;
    size_t num_basis_vectors;
    double *measurement_matrix;
    double *basis_matrix;
    double *reconstructed_field;
    int    use_dct_basis;
    double regularization_lambda;
} nf_compressive_scan_t;

/* ---------- Spatial Nyquist Criterion (L5) ---------- */

typedef struct {
    double lambda_min_m;
    double nyquist_spacing_m;
    double actual_spacing_m;
    int    criterion_satisfied;
    double aliasing_error_estimate;
} nf_nyquist_check_t;

/* ---------- Probe Position Correction (L5) ---------- */

typedef struct {
    double x_offset_m;
    double y_offset_m;
    double z_offset_m;
    double roll_deg;
    double pitch_deg;
    double yaw_deg;
    double scale_factor;
} nf_position_correction_t;

/* ============================================================================
 * L6 — Canonical Problems
 * ============================================================================
 */

/* ---------- Microstrip Line Near-Field Mapping (L6) ---------- */

typedef struct {
    double trace_width_m;
    double trace_length_m;
    double substrate_h_m;
    double epsilon_r;
    double char_impedance_ohm;
    double freq_hz;
    double input_power_dbm;
    double termination_impedance_ohm;
    double probe_height_m;
    nf_scan_grid_t scan_grid;
} nf_microstrip_mapping_t;

/* ---------- IC Radiated Emission Hot-Spot (L6) ---------- */

typedef struct {
    double ic_width_m;
    double ic_length_m;
    double ic_height_m;
    double die_position_x_m;
    double die_position_y_m;
    double clock_freq_hz;
    double *emission_map_dbuV;
    size_t map_rows;
    size_t map_cols;
    double hot_spot_x_m;
    double hot_spot_y_m;
    double hot_spot_level_dbuV;
} nf_ic_emission_t;

/* ---------- Differential Pair Radiation (L6) ---------- */

typedef struct {
    double trace_spacing_m;
    double trace_width_m;
    double trace_length_m;
    double substrate_h_m;
    double epsilon_r;
    double differential_impedance_ohm;
    double common_mode_voltage_V;
    double differential_voltage_V;
    double freq_hz;
    double common_mode_radiation_dbuVpm;
    double differential_radiation_dbuVpm;
    double imbalance_factor;
} nf_diff_pair_radiation_t;

/* ---------- Ground Plane Slot Coupling (L6) ---------- */

typedef struct {
    double slot_length_m;
    double slot_width_m;
    double ground_plane_thickness_m;
    double source_distance_m;
    double freq_hz;
    double coupling_coefficient;
    double resonance_freq_hz;
    double shielding_effectiveness_db;
    double radiated_field_through_slot_Vpm;
} nf_slot_coupling_t;

/* ---------- Decoupling Capacitor Evaluation (L6) ---------- */

typedef struct {
    double capacitance_F;
    double esl_H;
    double esr_ohm;
    double mounting_inductance_H;
    double self_resonant_freq_hz;
    double impedance_at_freq_ohm;
    double freq_hz;
    double pdn_impedance_with_cap_ohm;
    double pdn_impedance_without_cap_ohm;
    double improvement_db;
} nf_decoupling_cap_t;

/* ---------- Transmission Line Resonance (L6) ---------- */

typedef struct {
    double line_length_m;
    double char_impedance_ohm;
    double propagation_constant;
    double source_impedance_ohm;
    double load_impedance_ohm;
    double resonance_freqs_hz[10];
    int    num_resonances;
    double voltage_standing_wave_ratio;
    double reflection_coefficient_mag;
} nf_tline_resonance_t;

/* ============================================================================
 * L6 Continued — Additional Canonical Problems
 * ============================================================================
 */

/* ---------- Cable Radiation from Common-Mode Current (L6) ---------- */

typedef struct {
    double cable_length_m;
    double cable_diameter_m;
    double height_above_ground_m;
    double common_mode_current_A;
    double freq_hz;
    double max_E_field_at_3m_Vpm;
    double max_E_field_at_10m_Vpm;
    double radiation_resistance_ohm;
} nf_cable_radiation_t;

/* ---------- Heatsink Emission Modeling (L6) ---------- */

typedef struct {
    double heatsink_width_m;
    double heatsink_length_m;
    double fin_height_m;
    int    num_fins;
    double fin_spacing_m;
    double grounding_inductance_H;
    double freq_hz;
    double dipole_moment_equivalent;
    double radiated_E_at_1m_Vpm;
} nf_heatsink_emission_t;

/* ---------- Via Transition Radiation (L6) ---------- */

typedef struct {
    double via_diameter_m;
    double via_length_m;
    double pad_diameter_m;
    double antipad_diameter_m;
    double epsilon_r;
    double freq_hz;
    double via_impedance_ohm;
    double return_loss_db;
    double radiated_power_dbm;
} nf_via_radiation_t;

/* ============================================================================
 * API — Scanning Operations
 * ============================================================================
 */

/* Scan configuration */
int  nf_scan_config_init(nf_scan_config_t *cfg,
                          double x_start, double x_end, size_t nx,
                          double y_start, double y_end, size_t ny,
                          double z_height, nf_scan_pattern_t pattern);
void nf_scan_config_set_freq(nf_scan_config_t *cfg,
                              double f_start, double f_stop, double f_step);

/* Grid operations */
int  nf_scan_grid_validate(const nf_scan_grid_t *grid);
int  nf_scan_grid_point_index(const nf_scan_grid_t *grid,
                               size_t ix, size_t iy, size_t *idx);
int  nf_scan_grid_point_coords(const nf_scan_grid_t *grid,
                                size_t idx, double *x, double *y);
double nf_scan_grid_max_spatial_freq(const nf_scan_grid_t *grid);

/* Scan execution simulation */
int  nf_scan_generate_raster_path(const nf_scan_grid_t *grid,
                                   nf_scan_point_t *path, size_t *num_points);
int  nf_scan_adaptive_refine(const nf_scan_frame_t *coarse,
                              nf_adaptive_scan_t *adp,
                              nf_scan_grid_t *refined_grid);

/* Spatial Nyquist check */
int  nf_nyquist_check(const nf_scan_grid_t *grid, double freq_hz,
                       nf_nyquist_check_t *result);

/* Compressive sampling reconstruction */
int  nf_compressive_reconstruct(const nf_compressive_scan_t *cs,
                                 double *measurements,
                                 double *reconstructed);

/* Scan dataset management */
int  nf_scan_dataset_allocate(nf_scan_dataset_t *ds,
                               const nf_scan_config_t *cfg);
void nf_scan_dataset_free(nf_scan_dataset_t *ds);
int  nf_scan_dataset_export_csv(const nf_scan_dataset_t *ds,
                                 const char *filename);

/* ============================================================================
 * API — Canonical Problem Solvers
 * ============================================================================
 */

/* Microstrip line near-field computation */
int  nf_microstrip_nearfield(const nf_microstrip_mapping_t *ms,
                              nf_field_sample_t *field_map,
                              size_t *map_size);

/* IC hot-spot detection */
int  nf_ic_hotspot_detect(nf_ic_emission_t *ic,
                           const double *emission_map,
                           size_t rows, size_t cols,
                           double threshold_dbuV);

/* Differential pair radiation estimation */
int  nf_diff_pair_radiation_estimate(nf_diff_pair_radiation_t *dp);

/* Ground slot coupling analysis */
int  nf_slot_coupling_analyze(nf_slot_coupling_t *sc);

/* Decoupling capacitor PDN analysis */
int  nf_decoupling_impedance(nf_decoupling_cap_t *dc);

/* Transmission line resonance finder */
int  nf_tline_resonance_find(nf_tline_resonance_t *tl, double f_min, double f_max);

/* Cable common-mode radiation (using transmission line + antenna model) */
int  nf_cable_radiation_estimate(nf_cable_radiation_t *cr);

/* Heatsink emission model (dipole moment method) */
int  nf_heatsink_emission_estimate(nf_heatsink_emission_t *hs);

/* Via radiation estimation */
int  nf_via_radiation_estimate(nf_via_radiation_t *vr);

/* Field map statistics */
int  nf_field_map_statistics(const nf_field_sample_t *samples, size_t n,
                              double *max_E, double *max_H,
                              double *mean_E, double *mean_H,
                              double *std_E, double *std_H);

/* Spatial correlation between two field maps */
double nf_field_map_correlation(const nf_field_sample_t *map1,
                                 const nf_field_sample_t *map2,
                                 size_t n);

#endif /* NF_SCANNING_H */
