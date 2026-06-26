#ifndef LISN_CORE_H
#define LISN_CORE_H

#include <stddef.h>
#include <stdint.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LISN_V_50UH, LISN_V_5UH, LISN_V_250UH, LISN_DELTA,
    LISN_CISPR25_5UH, LISN_CISPR25_1UH, LISN_DO160, LISN_MIL461, LISN_CUSTOM
} lisn_type_t;

typedef enum { LISN_PORT_EUT, LISN_PORT_MAINS, LISN_PORT_RF_OUT, LISN_PORT_AUX } lisn_port_t;

typedef struct {
    lisn_type_t type; double inductance_uh; double resistance_ohm;
    double coupling_cap_uf; double freq_start_hz; double freq_stop_hz;
    double nominal_impedance_ohm; int num_lines;
    char manufacturer[64]; char model[64]; char serial_number[64];
} lisn_config_t;

typedef struct {
    double frequency_hz; double z_real_ohm; double z_imag_ohm;
    double z_magnitude_ohm; double z_phase_rad; double vdf; double insertion_loss_db;
} lisn_impedance_state_t;

typedef struct {
    double frequency_hz; double level_dbuV; double cable_loss_db;
    double transducer_factor_db; double corrected_level_dbuV;
    double noise_floor_dbuV; int is_ambient;
} lisn_meas_point_t;

typedef struct {
    double freq_start_hz; double freq_stop_hz; double step_hz;
    double if_bandwidth_hz; double dwell_time_ms; int num_points; int use_log_spacing;
} lisn_sweep_config_t;

typedef enum {
    DETECTOR_PEAK, DETECTOR_QUASI_PEAK, DETECTOR_AVERAGE,
    DETECTOR_RMS, DETECTOR_RMS_AVERAGE, DETECTOR_CISPR_AVG
} lisn_detector_t;

typedef enum { LISN_MODE_NORMAL, LISN_MODE_CALIBRATION, LISN_MODE_AMBIENT, LISN_MODE_VERIFICATION } lisn_mode_t;

typedef struct {
    lisn_mode_t mode; lisn_config_t config; lisn_port_t active_port;
    double temperature_c; double humidity_pct; double supply_voltage_v;
    double supply_frequency_hz; int ground_plane_connected; uint64_t timestamp;
} lisn_state_t;

typedef struct { double freq_hz; double z_min_ohm; double z_max_ohm; double phase_min_deg; double phase_max_deg; } lisn_tolerance_band_t;
typedef enum { CISPR_BW_200HZ, CISPR_BW_9KHZ, CISPR_BW_120KHZ, CISPR_BW_1MHZ } cispr_bandwidth_t;
typedef struct { double pole_real; double pole_imag; double zero_real; double zero_imag; double dc_gain; } lisn_pole_zero_t;
typedef struct { double rise_time_ns; double overshoot_pct; double settling_time_ns; double propagation_delay_ns; } lisn_transient_response_t;

/* Core LISN functions */
double lisn_impedance_magnitude(double L_uh, double R_ohm, double f_hz);
double lisn_impedance_phase_rad(double L_uh, double R_ohm, double f_hz);
double lisn_voltage_division_factor(const lisn_config_t *config, double f_hz, double R_load);
double lisn_insertion_loss_db(const lisn_config_t *config, double f_hz);
double lisn_corner_frequency_hz(double L_uh, double R_ohm);
double lisn_transfer_function_magnitude(const lisn_config_t *config, double f_hz, double R_load, double C_couple);
void lisn_init_cispr22_standard(lisn_config_t *config);
void lisn_init_cispr25_standard(lisn_config_t *config);
void lisn_init_mil461_standard(lisn_config_t *config);
int lisn_validate_impedance_compliance(const lisn_config_t *config, double f_hz, double z_tol_pct);
void lisn_compute_impedance_state(const lisn_config_t *config, double f_hz, double R_load, lisn_impedance_state_t *state);
double lisn_impedance_at_frequency(const lisn_config_t *cfg, double f_hz);
double lisn_phase_margin_degrees(const lisn_config_t *cfg, double f_hz);
int lisn_check_frequency_in_range(const lisn_config_t *cfg, double f_hz);
void lisn_get_standard_bandwidth(double f_hz, double *bw_hz, cispr_bandwidth_t *band);
double lisn_thermal_noise_floor_dbm(double bandwidth_hz, double temperature_c);
void lisn_compute_pole_zero(const lisn_config_t *cfg, lisn_pole_zero_t *pz);
double lisn_z_parameter_to_s(double z_re, double z_im, double f_hz);
void lisn_serialize_config(const lisn_config_t *cfg, char *buffer, int buf_size);
int lisn_deserialize_config(const char *buffer, lisn_config_t *cfg);

#ifdef __cplusplus
}
#endif

/* Additional LISN utility function declarations */
double lisn_impedance_deviation_pct(const lisn_config_t *cfg, double f);
int lisn_impedance_region(const lisn_config_t *cfg, double f);
double lisn_impedance_bandwidth_hz(const lisn_config_t *cfg);
double lisn_effective_inductance_uh(double z_measured_ohm, double R_ohm, double f);
double lisn_power_dissipation_watts(double eut_voltage_rms, double R_ohm);
double lisn_time_constant_ns(double L_uh, double R_ohm);
int lisn_configs_equivalent(const lisn_config_t *a, const lisn_config_t *b, double tol_pct);
double lisn_max_current_estimate(double wire_awg, int parallel_wires);

#endif /* LISN_CORE_H */
