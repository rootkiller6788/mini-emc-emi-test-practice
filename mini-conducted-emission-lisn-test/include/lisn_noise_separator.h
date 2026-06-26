#ifndef LISN_NOISE_SEPARATOR_H
#define LISN_NOISE_SEPARATOR_H
#include "lisn_core.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    double frequency_hz;
    double cm_voltage_dbuV;
    double dm_voltage_dbuV;
    double total_voltage_dbuV;
    double cm_ratio_pct;
    double dm_ratio_pct;
} lisn_cm_dm_result_t;

typedef enum {
    SEP_METHOD_RF_COMBINER,
    SEP_METHOD_MATH_DECOMP,
    SEP_METHOD_DUAL_CURRENT
} lisn_separation_method_t;

typedef struct {
    lisn_separation_method_t method;
    double combiner_balance_db;
    double combiner_phase_err_deg;
    double probe_transfer_ohm;
} lisn_separator_config_t;

void lisn_separate_math_decomp(double v_line_dbuV, double v_neutral_dbuV, double frequency_hz, lisn_cm_dm_result_t *result);
double lisn_dbuV_to_voltage(double level_dbuV);
double lisn_voltage_to_dbuV(double voltage_v);
void lisn_separate_rf_combiner(double v_line_dbuV, double v_neutral_dbuV, double frequency_hz, const lisn_separator_config_t *config, lisn_cm_dm_result_t *result);
double lisn_cm_to_dm_conversion(double cm_voltage_dbuV, double z_imbalance_pct, double frequency_hz);
double lisn_dm_to_cm_conversion(double dm_voltage_dbuV, double z_imbalance_pct, double frequency_hz);
void lisn_filter_attenuation_required(double cm_level_dbuV, double dm_level_dbuV, double limit_dbuV, double margin_db, double *atten_cm_db, double *atten_dm_db);
double lisn_lcl_compute(double v_cm_input_dbuV, double v_dm_measured_dbuV);
double lisn_tcl_compute(double v_dm_input_dbuV, double v_cm_measured_dbuV);
void lisn_cm_dm_sweep(const double *v_lines_dbuV, const double *v_neutrals_dbuV, const double *frequencies_hz, int n_points, lisn_cm_dm_result_t *results);

#ifdef __cplusplus
}
#endif

/* L5: Advanced mode separation metrics */
typedef struct {
    double mode_rejection_db;
    double cm_dm_isolation_db;
    double amplitude_balance_db;
    double phase_balance_deg;
} lisn_separator_metrics_t;

/* L3: Modal decomposition matrix */
typedef struct {
    double a11, a12;
    double a21, a22;
} lisn_mode_matrix_t;

/* Additional separator functions */
void lisn_separator_metrics_calc(const lisn_separator_config_t *cfg, double f, lisn_separator_metrics_t *m);
void lisn_mode_matrix_invert(const lisn_mode_matrix_t *m, lisn_mode_matrix_t *inv);
void lisn_apply_mode_matrix(const lisn_mode_matrix_t *mat, double vL, double vN, double *vcm, double *vdm);
double lisn_cmrr_compute(const lisn_separator_metrics_t *m);

#endif
