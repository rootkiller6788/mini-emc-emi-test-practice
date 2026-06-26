#ifndef CHAMBER_THEORY_H
#define CHAMBER_THEORY_H

#include <stddef.h>
#include <math.h>
#include "radiated_emission.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * L3+L4: Chamber Theory - Site Attenuation, NSA, SVSWR, Field Uniformity
 *
 * Reference: CISPR 16-1-4:2019 sec 6
 *           ANSI C63.4 sec 5.4
 *           Paul (2006) EMC sec 5.3
 * ========================================================================= */

typedef enum {
    NSA_METHOD_DISCRETE       = 0,
    NSA_METHOD_SWEPT          = 1,
    NSA_METHOD_TIMEDOMAIN     = 2
} nsa_method_t;

typedef enum {
    CHAMBER_VAL_NSA           = 0,
    CHAMBER_VAL_SVSWR         = 1,
    CHAMBER_VAL_FU            = 2,
    CHAMBER_VAL_FREE_SPACE    = 3
} chamber_validation_t;

typedef struct {
    double            frequency_hz;
    double            distance_m;
    double            height_tx_m;
    double            height_rx_m;
    re_polarization_t polarization;
    double            expected_nsa_db;
} nsa_config_t;

typedef struct {
    double frequency_hz;
    double height_tx_m;
    double height_rx_m;
    re_polarization_t polarization;
    double v_direct_dbuv;
    double v_site_dbuv;
    double nsa_measured_db;
    double nsa_theoretical_db;
    double deviation_db;
    int    pass;
} nsa_measurement_t;

#define NSA_MAX_POINTS 512

typedef struct {
    char                chamber_id[64];
    chamber_validation_t validation_type;
    size_t              num_measurements;
    nsa_measurement_t   measurements[NSA_MAX_POINTS];
    double              max_deviation_db;
    double              rms_deviation_db;
    int                 overall_pass;
    char                reference_standard[64];
} nsa_report_t;

typedef struct {
    double frequency_hz;
    double position_x_m;
    double v_max_dbuv;
    double v_min_dbuv;
    double svswr_linear;
    double svswr_db;
    int    pass;
} svswr_point_t;

#define SVSWR_MAX_POINTS 512

typedef struct {
    char          chamber_id[64];
    size_t        num_points;
    svswr_point_t points[SVSWR_MAX_POINTS];
    double        worst_svswr_db;
    int           overall_pass;
} svswr_report_t;

typedef struct {
    double frequency_hz;
    double e_field_points[16];
    double e_mean;
    double e_stddev;
    double uniformity_db;
    int    pass;
} field_uniformity_point_t;

#define FU_MAX_POINTS 128

typedef struct {
    char                     chamber_id[64];
    double                   grid_size_m;
    size_t                   num_freqs;
    field_uniformity_point_t points[FU_MAX_POINTS];
    int                      overall_pass;
} fu_report_t;

typedef struct {
    char            chamber_id[64];
    re_site_type_t  site_type;
    double          dimensions_lwh_m[3];
    double          absorber_type;
    double          absorber_performance_db;
    double          shielding_effectiveness_db;
    double          ambient_noise_dbuvm;
    nsa_report_t    nsa_center;
    nsa_report_t    nsa_left;
    nsa_report_t    nsa_right;
    nsa_report_t    nsa_front;
    nsa_report_t    nsa_back;
    svswr_report_t  svswr;
} chamber_profile_t;

double ct_ground_reflection_coeff(double epsilon_r, double conductivity_sm,
                                   double freq_hz, double grazing_angle_rad,
                                   re_polarization_t polarization);
double ct_nsa_theoretical(double distance_m, double h_tx_m, double h_rx_m,
                           double freq_hz, re_polarization_t polarization);
void ct_nsa_height_scan_opt(double distance_m, double h_tx_m,
                             double freq_hz, re_polarization_t polarization,
                             double *h_rx_opt_m, double *max_v_ratio);
double ct_svswr_from_reflectivity(double absorber_reflectivity_db,
                                   double chamber_gain_db);

#ifdef __cplusplus
}
#endif

#endif /* CHAMBER_THEORY_H */