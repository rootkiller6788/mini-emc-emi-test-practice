#ifndef EMI_PRECOMPLIANCE_CORE_H
#define EMI_PRECOMPLIANCE_CORE_H

#include <stdint.h>
#include <stddef.h>
#include <math.h>

#define EMI_Z0_FREESPACE       376.9911184307752
#define EMI_C0                  299792458.0
#define EMI_KB                  1.380649e-23
#define EMI_T0                  290.0
#define EMI_PI                  3.14159265358979323846
#define EMI_MDS_THRESHOLD_DBM  (-174.0)
#define EMI_CONDUCTED_RBW_HZ    9000.0
#define EMI_RADIATED_RBW_HZ     120000.0
#define EMI_QP_TC_CHARGE_MS     1.0
#define EMI_QP_TC_DISCHARGE_MS  550.0

typedef enum {
    EMI_EMISSION_CONDUCTED = 0,
    EMI_EMISSION_RADIATED  = 1
} emi_emission_type_t;

typedef enum {
    EMI_DETECTOR_PEAK       = 0,
    EMI_DETECTOR_QUASIPEAK  = 1,
    EMI_DETECTOR_AVERAGE    = 2,
    EMI_DETECTOR_RMS        = 3
} emi_detector_type_t;

typedef enum {
    EMI_STD_CISPR22 = 0,
    EMI_STD_CISPR32 = 1,
    EMI_STD_CISPR11 = 2,
    EMI_STD_CISPR14 = 3,
    EMI_STD_CISPR25 = 4,
    EMI_STD_FCC_PART15 = 5,
    EMI_STD_MIL_STD_461 = 6
} emi_standard_t;

typedef enum {
    EMI_MARGIN_PASS      = 0,
    EMI_MARGIN_MARGINAL  = 1,
    EMI_MARGIN_FAIL      = 2
} emi_margin_status_t;

typedef struct {
    double frequency_hz;
    double amplitude_dbm;
    double amplitude_dbuv;
    double field_strength_dbuvm;
    emi_detector_type_t detector;
    double limit_dbuv;
    double margin_db;
    emi_margin_status_t status;
} emi_emission_point_t;

typedef struct {
    double start_freq_hz;
    double stop_freq_hz;
    double rbw_hz;
    double vbw_hz;
    double sweep_time_s;
    double attenuation_db;
    double preamp_gain_db;
    int    num_points;
    emi_detector_type_t detector;
    emi_standard_t standard;
    int    average_count;
} emi_scan_config_t;

typedef struct {
    emi_scan_config_t config;
    emi_emission_point_t *points;
    int    num_points;
    double max_emission_dbuv;
    double max_emission_freq_hz;
    double worst_margin_db;
    double noise_floor_dbm;
    int    num_marginal_points;
    int    num_failing_points;
    char   timestamp[32];
} emi_scan_result_t;

typedef struct {
    double freq_start_hz;
    double freq_stop_hz;
    double limit_start_dbuv;
    double limit_stop_dbuv;
    emi_detector_type_t detector;
    const char *band_name;
} emi_limit_segment_t;

typedef struct {
    emi_limit_segment_t *segments;
    int    num_segments;
    emi_standard_t standard;
    emi_emission_type_t emission_type;
} emi_limit_line_t;

typedef struct {
    int    num_points;
    double *freq_hz;
    double *loss_db;
    const char *description;
} emi_path_loss_t;

/* emi_uncertainty_item_t and emi_uncertainty_budget_t are defined
 * in emi_correlation_uncertainty.h with full CISPR 16-4-2 detail. */

static inline double emi_dbm_to_dbuv(double p_dbm, double z_ohms) {
    return p_dbm + 90.0 + 10.0 * log10(z_ohms);
}

static inline double emi_dbuv_to_dbm(double v_dbuv, double z_ohms) {
    return v_dbuv - 90.0 - 10.0 * log10(z_ohms);
}

static inline double emi_dbm_to_watts(double p_dbm) {
    return pow(10.0, (p_dbm - 30.0) / 10.0);
}

static inline double emi_watts_to_dbm(double p_w) {
    if (p_w <= 0.0) return -200.0;
    return 10.0 * log10(p_w) + 30.0;
}

static inline double emi_dbuv_to_volts(double v_dbuv) {
    return pow(10.0, v_dbuv / 20.0) * 1.0e-6;
}

static inline double emi_volts_to_dbuv(double v_volts) {
    if (v_volts <= 0.0) return -200.0;
    return 20.0 * log10(v_volts * 1.0e6);
}

static inline double emi_dbuvm_to_vm(double e_dbuvm) {
    return pow(10.0, e_dbuvm / 20.0) * 1.0e-6;
}

static inline double emi_vm_to_dbuvm(double e_vm) {
    if (e_vm <= 0.0) return -200.0;
    return 20.0 * log10(e_vm * 1.0e6);
}

static inline double emi_dbuvm_to_power_density(double e_dbuvm) {
    double e_vm = emi_dbuvm_to_vm(e_dbuvm);
    return (e_vm * e_vm) / EMI_Z0_FREESPACE;
}

static inline double emi_power_density_to_dbuvm(double s_wm2) {
    if (s_wm2 <= 0.0) return -200.0;
    double e_vm = sqrt(s_wm2 * EMI_Z0_FREESPACE);
    return emi_vm_to_dbuvm(e_vm);
}

static inline double emi_efield_to_hfield_far(double e_vm) {
    return e_vm / EMI_Z0_FREESPACE;
}

static inline double emi_hfield_to_efield_far(double h_am) {
    return h_am * EMI_Z0_FREESPACE;
}

static inline double emi_am_to_dbuam(double h_am) {
    if (h_am <= 0.0) return -200.0;
    return 20.0 * log10(h_am) + 120.0;
}

static inline double emi_dbuam_to_am(double h_dbuam) {
    return pow(10.0, (h_dbuam - 120.0) / 20.0);
}

static inline double emi_antenna_factor_from_gain(double freq_hz, double gain_dbi) {
    /* AF = 20*log10(f_MHz) - G_dBi - 29.79
     * where G_dBi is already in dB. Ref: Balanis Eq. 2-128. */
    double freq_mhz = freq_hz / 1.0e6;
    if (freq_mhz <= 0.0) return 0.0;
    return 20.0 * log10(freq_mhz) - gain_dbi - 29.79;
}

static inline double emi_gain_from_antenna_factor(double freq_hz, double af_db) {
    /* Inverse: G_dBi = 20*log10(f_MHz) - AF_dB - 29.79 */
    double freq_mhz = freq_hz / 1.0e6;
    if (freq_mhz <= 0.0) return 0.0;
    return 20.0 * log10(freq_mhz) - af_db - 29.79;
}

static inline double emi_field_strength_from_rx_power(
    double rx_power_dbm, double freq_hz, double af_db,
    double cable_loss_db, double preamp_gain_db)
{
    (void)freq_hz; /* reserved for future frequency-dependent cable loss */
    double v_dbuv = emi_dbm_to_dbuv(rx_power_dbm, 50.0);
    return v_dbuv + af_db + cable_loss_db - preamp_gain_db;
}

double emi_limit_line_interpolate(const emi_limit_line_t *limit, double freq_hz);
double emi_evaluate_margin(double emission_dbuv, double freq_hz,
                           const emi_limit_line_t *limit);

/* Limit line initialization and management */
int  emi_limit_init_cispr32_conducted_classb(emi_limit_line_t *limit, emi_detector_type_t detector);
int  emi_limit_init_cispr32_radiated_classb(emi_limit_line_t *limit, emi_detector_type_t detector);
int  emi_limit_init_fcc15_radiated_classb(emi_limit_line_t *limit);
void emi_limit_line_free(emi_limit_line_t *limit);

/* Scan configuration and system sensitivity */
void emi_scan_config_init(emi_scan_config_t *config, emi_emission_type_t type, emi_standard_t standard);
double emi_min_detectable_field(double freq_hz, double rbw_hz, double nf_db,
                                 double snr_min_db, double af_db,
                                 double cable_loss_db, double preamp_gain_db);
int  emi_check_system_sensitivity(double freq_hz, double limit_dbuvm,
                                   double rbw_hz, double nf_db,
                                   double af_db, double cable_loss_db,
                                   double preamp_gain_db);

/* Emission point array management */
int  emi_emission_points_alloc(emi_emission_point_t **points, int count);
void emi_emission_points_free(emi_emission_point_t *points);
void emi_evaluate_all_margins(emi_scan_result_t *result, const emi_limit_line_t *limit);

static inline double emi_noise_floor_dbm(double rbw_hz, double nf_db) {
    return EMI_MDS_THRESHOLD_DBM + 10.0 * log10(rbw_hz) + nf_db;
}

static inline double emi_sensitivity_dbuv(double rbw_hz, double nf_db,
                                           double required_snr_db) {
    double mds_dbm = emi_noise_floor_dbm(rbw_hz, nf_db);
    double rx_dbm = mds_dbm + required_snr_db;
    return emi_dbm_to_dbuv(rx_dbm, 50.0);
}

static inline double emi_min_sweep_time(double span_hz, double rbw_hz,
                                         double vbw_hz, double k_factor) {
    return k_factor * span_hz / (rbw_hz * vbw_hz);
}

static inline double emi_auto_vbw(double rbw_hz, emi_detector_type_t detector) {
    switch (detector) {
        case EMI_DETECTOR_PEAK:       return rbw_hz * 3.0;
        case EMI_DETECTOR_QUASIPEAK:  return 9000.0;
        case EMI_DETECTOR_AVERAGE:    return rbw_hz;
        case EMI_DETECTOR_RMS:        return rbw_hz * 3.0;
        default:                      return rbw_hz * 3.0;
    }
}

static inline double emi_wavelength(double freq_hz) {
    return EMI_C0 / freq_hz;
}

static inline double emi_far_field_distance(double max_dimension_m, double freq_hz) {
    double lambda = emi_wavelength(freq_hz);
    if (lambda <= 0.0) return 1.0e9;
    return 2.0 * max_dimension_m * max_dimension_m / lambda;
}

static inline int emi_is_reactive_near_field(double distance_m, double freq_hz) {
    double lambda = emi_wavelength(freq_hz);
    double reactive_boundary = lambda / (2.0 * EMI_PI);
    return (distance_m < reactive_boundary) ? 1 : 0;
}

#endif /* EMI_PRECOMPLIANCE_CORE_H */
