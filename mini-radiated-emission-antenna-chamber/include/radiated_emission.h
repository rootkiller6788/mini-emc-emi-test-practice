#ifndef RADIATED_EMISSION_H
#define RADIATED_EMISSION_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * L1: Core Definitions - Radiated Emission Measurement Fundamentals
 * Reference: Paul (2006) Introduction to Electromagnetic Compatibility
 *           CISPR 16-1-4:2019
 * ========================================================================= */

typedef enum {
    RE_DETECTOR_PEAK         = 0,
    RE_DETECTOR_QUASI_PEAK   = 1,
    RE_DETECTOR_AVERAGE      = 2,
    RE_DETECTOR_RMS          = 3,
    RE_DETECTOR_CISPR_AVG    = 4
} re_detector_type_t;

typedef enum {
    RE_STD_CISPR11       = 0,
    RE_STD_CISPR22       = 1,
    RE_STD_CISPR32       = 2,
    RE_STD_FCC_PART15    = 3,
    RE_STD_MIL_STD_461   = 4,
    RE_STD_CISPR25       = 5,
    RE_STD_EN55011       = 6,
    RE_STD_EN55032       = 7
} re_standard_t;

typedef enum {
    RE_SITE_OATS              = 0,
    RE_SITE_SAC_3M            = 1,
    RE_SITE_SAC_10M           = 2,
    RE_SITE_FAR_3M            = 3,
    RE_SITE_FAR_5M            = 4,
    RE_SITE_GTEM              = 5,
    RE_SITE_REVERB            = 6
} re_site_type_t;

typedef enum {
    RE_POL_HORIZONTAL = 0,
    RE_POL_VERTICAL   = 1
} re_polarization_t;

typedef enum {
    RE_BAND_A = 0,
    RE_BAND_B = 1,
    RE_BAND_C = 2,
    RE_BAND_D = 3,
    RE_BAND_E = 4,
    RE_BAND_F = 5
} re_band_t;

typedef struct {
    double frequency_hz;
    double antenna_factor_db;
    double gain_dbi;
    double antenna_impedance;
    double vswr;
} re_antenna_factor_point_t;

#define RE_MAX_AF_POINTS 1024

typedef struct {
    char       antenna_model[64];
    char       serial_number[32];
    char       calibration_date[16];
    double     cal_uncertainty_db;
    size_t     num_points;
    re_antenna_factor_point_t points[RE_MAX_AF_POINTS];
} re_antenna_factor_table_t;

typedef struct {
    re_site_type_t     site_type;
    double             measurement_dist_m;
    double             antenna_height_min_m;
    double             antenna_height_max_m;
    double             turntable_angle_min_deg;
    double             turntable_angle_max_deg;
    double             ground_plane_reflection_coeff;
    re_polarization_t  polarization;
    uint8_t            use_preamp;
    double             preamp_gain_db;
    double             cable_loss_db;
    double             rbw_hz;
    double             measurement_time_ms;
    re_detector_type_t detector;
} re_measurement_config_t;

typedef struct {
    double frequency_hz;
    double receiver_reading_dbm;
    double field_strength_dbuvm;
    double field_strength_vm;
    double limit_dbuvm;
    double margin_db;
    re_polarization_t dominant_pol;
    double antenna_height_m;
    double turntable_angle_deg;
    re_detector_type_t detector;
} re_emission_point_t;

#define RE_MAX_SCAN_POINTS 8192

typedef struct {
    char                  eut_name[128];
    char                  test_standard[64];
    re_measurement_config_t config;
    size_t                num_points;
    re_emission_point_t   points[RE_MAX_SCAN_POINTS];
    double                worst_case_margin_db;
    double                worst_case_freq_hz;
    int                   pass_fail;
} re_emission_scan_t;

typedef enum {
    RE_EUT_TABLE_TOP     = 0,
    RE_EUT_FLOOR_STANDING = 1,
    RE_EUT_COMBINATION   = 2
} re_eut_config_t;

typedef enum {
    RE_EUT_STATE_IDLE        = 0,
    RE_EUT_STATE_ACTIVE_TX   = 1,
    RE_EUT_STATE_ACTIVE_RX   = 2,
    RE_EUT_STATE_WORST_CASE  = 3,
    RE_EUT_STATE_STANDBY     = 4
} re_eut_operating_state_t;

typedef struct {
    char                    name[128];
    re_eut_config_t         config_type;
    re_eut_operating_state_t op_state;
    double                  base_height_m;
    double                  cable_length_m;
    uint8_t                 aux_equipment_present;
} re_eut_descriptor_t;

double re_antenna_factor_from_gain(double frequency_hz, double gain_linear);
double re_field_strength_compute(double receiver_dbm, double af_db,
                                  double cable_loss_db, double preamp_gain_db,
                                  double impedance_ohm);
double re_dbm_to_dbuv(double dbm, double impedance_ohm);
void re_af_table_init_standard(re_antenna_factor_table_t *table,
                                const char *model_name);
int re_af_interpolate(const re_antenna_factor_table_t *table,
                       double freq_hz, double *af_db);
void re_config_init_standard(re_measurement_config_t *config,
                              re_standard_t standard, re_site_type_t site);
double re_get_limit(re_standard_t standard, double freq_hz,
                    re_detector_type_t detector, double dist_m,
                    int class_type);
int re_emission_scan_perform(re_emission_scan_t *scan,
                              const re_eut_descriptor_t *eut,
                              const re_measurement_config_t *config,
                              const re_antenna_factor_table_t *af_tbl);
int re_emission_evaluate_pass_fail(const re_emission_scan_t *scan);
void re_emission_sort_by_margin(re_emission_scan_t *scan);
void re_emission_report_summary(const re_emission_scan_t *scan, FILE *out);

#ifdef __cplusplus
}
#endif

#endif /* RADIATED_EMISSION_H */
