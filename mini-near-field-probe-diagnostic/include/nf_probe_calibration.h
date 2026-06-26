#ifndef NF_PROBE_CALIBRATION_H
#define NF_PROBE_CALIBRATION_H
/**
 * @file nf_probe_calibration.h
 * @brief Near-Field Probe Calibration Methods (L5)
 *
 * L5: Calibration algorithms — TEM cell method, microstrip method,
 *     reference antenna method, 3-antenna method, vector network
 *     analyzer (VNA) de-embedding
 *
 * Ref: IEC 61967-3, "Integrated circuits – Measurement of electromagnetic emissions,
 *      Part 3: Measurement of radiated emissions – Surface scan method"
 *      CISPR 16-1-4 Annex C, "Calibration of EMC antennas"
 *      IEEE Std 1309-2013, "Calibration of Electromagnetic Field Sensors and Probes"
 */

#include <complex.h>
#include <stddef.h>
#include "nf_probe_core.h"
#include "nf_field_theory.h"

/* ============================================================================
 * L5 — Calibration Algorithms
 * ============================================================================
 */

/* ---------- Calibration Standard Types ---------- */

typedef enum {
    NF_CAL_STD_TEM_CELL = 0,
    NF_CAL_STD_MICROSTRIP = 1,
    NF_CAL_STD_COPLANAR_WG = 2,
    NF_CAL_STD_REF_ANTENNA = 3,
    NF_CAL_STD_GTEM_CELL = 4,
    NF_CAL_STD_KNOWN_FIELD = 5
} nf_cal_standard_type_t;

/* ---------- TEM Cell Calibration Parameters ---------- */

typedef struct {
    double septum_height_m;
    double cell_width_m;
    double cell_height_m;
    double characteristic_impedance_ohm;
    double input_power_W;
    double computed_E_field_Vpm;
    double computed_H_field_Apm;
    double field_uniformity_pct;
} nf_tem_cell_params_t;

/* ---------- Microstrip Calibration Standard ---------- */

typedef struct {
    double substrate_height_m;
    double trace_width_m;
    double trace_thickness_m;
    double epsilon_r;
    double characteristic_impedance_ohm;
    double effective_permittivity;
    double computed_E_above_trace_Vpm;
    double computed_H_above_trace_Apm;
    double probe_height_m;
} nf_microstrip_std_t;

/* ---------- Calibration Data Point ---------- */

typedef struct {
    double freq_hz;
    double v_probe_out_V;
    double e_reference_Vpm;
    double h_reference_Apm;
    double pf_e_per_m;
    double pf_h_a_per_vm;
    double phase_shift_rad;
} nf_cal_data_point_t;

/* ---------- Calibration Dataset ---------- */

typedef struct {
    nf_cal_data_point_t *points;
    size_t num_points;
    double freq_start_hz;
    double freq_stop_hz;
    nf_cal_standard_type_t standard_type;
    nf_probe_type_t probe_type;
    char calibration_date[32];
    double temperature_C;
    double humidity_pct;
} nf_cal_dataset_t;

/* ---------- 3-Antenna Calibration Method (L5) ---------- */

typedef struct {
    double S21_db[3];
    double S12_db[3];
    double distance_m[3];
    double gain_dbi[3];
    double uncertainty_db[3];
    int    converged;
} nf_three_antenna_cal_t;

/* ---------- VNA De-embedding Parameters (L5) ---------- */

typedef struct {
    double complex S11_open;
    double complex S11_short;
    double complex S11_load;
    double complex S11_thru;
    double complex S11_dut;
    double complex Z0;
    double complex Z_dut_deembedded;
    double complex Y_dut_deembedded;
} nf_vna_deembed_t;

/* ---------- Interpolation Method for Calibration ---------- */

typedef enum {
    NF_INTERP_LINEAR = 0,
    NF_INTERP_CUBIC_SPLINE = 1,
    NF_INTERP_PCHIP = 2,
    NF_INTERP_RATIONAL = 3
} nf_interp_method_t;

typedef struct {
    nf_interp_method_t method;
    double *x_nodes;
    double *y_nodes;
    double *coeffs;
    size_t num_nodes;
    size_t num_intervals;
} nf_interpolator_t;

/* ---------- Uncertainty Budget (L5) ---------- */

typedef struct {
    double standard_uncertainty_db;
    double probe_positioning_db;
    double impedance_mismatch_db;
    double receiver_nonlinearity_db;
    double noise_contribution_db;
    double repeatability_db;
    double combined_uncertainty_db;
    double expanded_uncertainty_db;
    double coverage_factor_k;
} nf_uncertainty_budget_t;

/* ============================================================================
 * API — Calibration Functions
 * ============================================================================
 */

/* TEM cell field computation */
int  nf_tem_cell_field(double input_power_W, double septum_height_m,
                        double cell_height_m, double impedance_ohm,
                        double *E_field_Vpm, double *H_field_Apm);

/* Microstrip calibration field computation */
int  nf_microstrip_field_above_trace(const nf_microstrip_std_t *ms,
                                      double probe_height_m,
                                      double *E_Vpm, double *H_Apm);

int  nf_microstrip_effective_permittivity(double w, double h,
                                           double epsilon_r, double t,
                                           double *eps_eff, double *Z0);

/* Calibration dataset management */
int  nf_cal_dataset_allocate(nf_cal_dataset_t *ds, size_t num_points);
void nf_cal_dataset_free(nf_cal_dataset_t *ds);
int  nf_cal_dataset_interpolate(const nf_cal_dataset_t *ds,
                                 double freq_hz,
                                 nf_interp_method_t method,
                                 nf_cal_data_point_t *result);

/* Probe factor computation from measurements */
int  nf_cal_compute_probe_factor(const nf_cal_dataset_t *ds,
                                  nf_probe_factor_t *pf);

/* 3-antenna calibration */
int  nf_three_antenna_calibrate(double S21_12, double S21_23, double S21_31,
                                 double dist_12, double dist_23, double dist_31,
                                 double freq_hz,
                                 nf_three_antenna_cal_t *result);

/* VNA de-embedding (SOLT method) */
int  nf_vna_deembed_solt(const nf_vna_deembed_t *deembed,
                          double complex *Z_corrected);

/* VNA TRL calibration computation */
int  nf_vna_trl_compute(double complex S11_meas, double complex S21_meas,
                         double complex S12_meas, double complex S22_meas,
                         double complex *S11_corr, double complex *S21_corr,
                         double complex *S12_corr, double complex *S22_corr);

/* Spatial calibration and alignment */
int  nf_spatial_calibrate(double *measured_positions_x,
                           double *measured_positions_y,
                           double *reference_positions_x,
                           double *reference_positions_y,
                           size_t num_points,
                           double *offset_x, double *offset_y,
                           double *rotation_deg, double *scale);

/* Uncertainty budget computation */
int  nf_uncertainty_budget_compute(const nf_cal_dataset_t *ds,
                                    nf_uncertainty_budget_t *budget);

/* Probe factor frequency interpolation */
int  nf_probe_factor_interpolate(const double *freqs_hz,
                                  const double *pf_e_values,
                                  const double *pf_h_values,
                                  size_t num_freqs,
                                  double target_freq_hz,
                                  double *pf_e_out, double *pf_h_out);

/* Calibration verification with known source */
int  nf_cal_verify_with_reference(const nf_cal_dataset_t *ds,
                                   const nf_probe_factor_t *pf,
                                   const nf_probe_spec_t *ref_probe,
                                   double *max_deviation_db);

/* Interpolator construction and evaluation */
int  nf_interpolator_build(nf_interpolator_t *interp,
                            const double *x, const double *y,
                            size_t n, nf_interp_method_t method);
void nf_interpolator_free(nf_interpolator_t *interp);
double nf_interpolator_eval(const nf_interpolator_t *interp, double x);

#endif /* NF_PROBE_CALIBRATION_H */
