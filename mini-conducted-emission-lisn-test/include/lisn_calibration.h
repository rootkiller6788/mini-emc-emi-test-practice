#ifndef LISN_CALIBRATION_H
#define LISN_CALIBRATION_H
#include "lisn_core.h"
#include "lisn_impedance.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    double frequency_hz;
    double vdf_db;
    double insertion_loss_db;
    double correction_factor_db;
} lisn_cal_point_t;

typedef struct {
    lisn_config_t       lisn_info;
    int                 num_points;
    lisn_cal_point_t   *points;
    double              overall_correction_db;
    double              calibration_uncertainty_db;
    uint64_t            cal_date;
    uint64_t            cal_due_date;
    char                cal_lab[128];
    char                cal_certificate[64];
} lisn_calibration_t;

typedef struct {
    double frequency_hz;
    double measured_z_ohm;
    double nominal_z_ohm;
    double deviation_pct;
    double measured_phase_deg;
    double nominal_phase_deg;
    double phase_deviation_deg;
    int    impedance_pass;
    int    phase_pass;
} lisn_impedance_verification_t;

double lisn_cal_measure_insertion_loss(const lisn_network_model_t *model, double f_hz);
void lisn_cal_verify_impedance(const lisn_network_model_t *model, double f_hz, lisn_impedance_verification_t *result);
double lisn_cal_measure_isolation(const lisn_network_model_t *model, double f_hz);
double lisn_cal_voltage_division_factor(const lisn_network_model_t *model, double f_hz);
void lisn_cal_generate_table(const lisn_network_model_t *model, double f_start_hz, double f_stop_hz, int n_points, lisn_calibration_t *cal);
double lisn_cal_lcl(const lisn_network_model_t *model, double f_hz);
double lisn_cal_correction_factor(const lisn_calibration_t *cal, double f_hz, double cable_loss);
void lisn_calibration_free(lisn_calibration_t *cal);

#ifdef __cplusplus
}
#endif

/* L5: Advanced calibration methods */
typedef enum {
    CAL_METHOD_DIRECT,
    CAL_METHOD_SUBSTITUTION,
    CAL_METHOD_NETWORK_ANALYZER,
    CAL_METHOD_TDR,
    CAL_METHOD_IMPEDANCE_ANALYZER
} lisn_cal_method_t;

/* Calibration verification limits per CISPR 16-1-2 */
typedef struct {
    double freq_hz;
    double il_max_db;
    double vdf_max_error_db;
    double isolation_min_db;
    double lcl_min_db;
} lisn_cal_limits_t;

/* Additional calibration functions */
void lisn_cal_get_standard_limits(double f_hz, lisn_cal_limits_t *limits);
int lisn_cal_verify_full(const lisn_network_model_t *m, double fs, double fe, int n, int *all_pass);
double lisn_cal_interpolate_vdf(const lisn_calibration_t *cal, double f_hz);
void lisn_cal_print_certificate(const lisn_calibration_t *cal);
int lisn_cal_is_expired(const lisn_calibration_t *cal, uint64_t current_date);

#endif
