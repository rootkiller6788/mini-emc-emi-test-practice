#ifndef LISN_MEASUREMENT_H
#define LISN_MEASUREMENT_H
#include "lisn_core.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    double charge_time_ms;
    double discharge_time_ms;
    double if_bandwidth_hz;
    double meter_time_constant_ms;
    double overload_factor_db;
} qp_detector_params_t;

typedef enum {
    QP_BAND_A, QP_BAND_B, QP_BAND_C, QP_BAND_D
} qp_band_t;

typedef struct {
    double averaging_time_ms;
    double if_bandwidth_hz;
} avg_detector_params_t;

typedef struct {
    lisn_detector_t detector_type;
    qp_detector_params_t qp_params;
    avg_detector_params_t avg_params;
    double reference_level_dbuV;
    double rf_attenuation_db;
    double preamp_gain_db;
    int    preamp_enabled;
} lisn_receiver_config_t;

typedef struct {
    double frequency_hz;
    double peak_level_dbuV;
    double qp_level_dbuV;
    double avg_level_dbuV;
    double rms_level_dbuV;
    lisn_detector_t final_detector;
    double final_level_dbuV;
    int    overload;
    double margin_db;
} lisn_emission_result_t;

typedef struct {
    lisn_sweep_config_t  sweep;
    lisn_receiver_config_t receiver;
    int     num_results;
    lisn_emission_result_t *results;
    double  overall_margin_db;
    double  worst_frequency_hz;
    char    test_standard[64];
    char    eut_identification[128];
    char    test_operator[64];
    uint64_t test_timestamp;
} lisn_measurement_dataset_t;

typedef struct {
    char   source[128];
    double value_db;
    double probability_factor;
    double sensitivity_coeff;
    char   distribution[32];
} lisn_uncertainty_component_t;

typedef struct {
    int    num_components;
    lisn_uncertainty_component_t *components;
    double combined_standard_uncertainty_db;
    double expanded_uncertainty_db;
} lisn_uncertainty_budget_t;

double lisn_peak_detect(const double *samples, int num_samples, double impedance_ohm);
double lisn_qp_detect(const double *samples, int num_samples, double sample_rate_hz, const qp_detector_params_t *params, double impedance_ohm);
double lisn_avg_detect(const double *samples, int num_samples, double sample_rate_hz, const avg_detector_params_t *params, double impedance_ohm);
double lisn_rms_detect(const double *samples, int num_samples, double impedance_ohm);
void lisn_log_frequency_array(double f_start_hz, double f_stop_hz, int n_points, double *frequencies);
void lisn_linear_frequency_array(double f_start_hz, double f_stop_hz, int n_points, double *frequencies);
double lisn_apply_corrections(double raw_level_dbuV, double cable_loss_db, double lisn_vdf_db, double attenuation_db, double preamp_gain_db);
double lisn_ambient_correction(double eut_level_dbuV, double ambient_level_dbuV);
void lisn_compute_uncertainty(lisn_uncertainty_budget_t *budget);
void lisn_uncertainty_budget_standard(lisn_uncertainty_budget_t *budget);

#ifdef __cplusplus
}
#endif

/* L5: Time-domain measurement capture */
typedef struct {
    double *samples;
    int num_samples;
    double sample_rate_hz;
    double trigger_level;
    int trigger_slope; /* 1=rising, -1=falling */
} lisn_td_capture_t;

/* L6: Statistical EMC measurement */
typedef struct {
    double mean_dbuV;
    double stddev_dbuV;
    double max_dbuV;
    double min_dbuV;
    double percentile_80;
    double percentile_95;
    int num_samples;
} lisn_statistical_result_t;

/* Additional measurement functions */
void lisn_td_capture_init(lisn_td_capture_t *cap, int num_samples, double rate);
void lisn_td_capture_free(lisn_td_capture_t *cap);
double lisn_find_max_emission_in_band(const lisn_measurement_dataset_t *ds, double f_low, double f_high);
void lisn_compute_statistics(const double *levels, int n, lisn_statistical_result_t *stat);
double lisn_interpolate_level(const lisn_measurement_dataset_t *ds, double f_hz);
void lisn_export_csv(const lisn_measurement_dataset_t *ds, const char *filename);

#endif
