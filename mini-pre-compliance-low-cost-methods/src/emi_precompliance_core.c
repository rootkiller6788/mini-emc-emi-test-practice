/**
 * emi_precompliance_core.c — Core Implementation for EMC Pre-Compliance
 *
 * Limit line interpolation, margin evaluation, scan configuration defaults,
 * and standard limit line definitions.
 *
 * Knowledge Coverage:
 *   L1: Pre-defined limit lines for major standards
 *   L2: Limit line interpolation (log-frequency, dB domain)
 *   L4: Noise floor analysis, system sensitivity
 *   L6: Complete limit margin evaluation workflow
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "emi_precompliance_core.h"

/* ─── Limit Line Interpolation ───────────────────────────────────────── */

double emi_limit_line_interpolate(const emi_limit_line_t *limit, double freq_hz) {
    if (!limit || !limit->segments || limit->num_segments <= 0) {
        return -200.0;
    }
    /* Find the segment containing freq_hz */
    for (int i = 0; i < limit->num_segments; i++) {
        const emi_limit_segment_t *seg = &limit->segments[i];
        if (freq_hz >= seg->freq_start_hz && freq_hz <= seg->freq_stop_hz) {
            /* Log-linear interpolation within segment */
            if (seg->freq_start_hz <= 0.0 || seg->freq_stop_hz <= 0.0) {
                return (seg->limit_start_dbuv + seg->limit_stop_dbuv) / 2.0;
            }
            double log_f = log10(freq_hz);
            double log_f1 = log10(seg->freq_start_hz);
            double log_f2 = log10(seg->freq_stop_hz);
            double t = (log_f - log_f1) / (log_f2 - log_f1);
            /* Clamp t to [0,1] */
            if (t < 0.0) t = 0.0;
            if (t > 1.0) t = 1.0;
            return seg->limit_start_dbuv + t * (seg->limit_stop_dbuv - seg->limit_start_dbuv);
        }
    }
    /* Frequency outside all segments — extrapolate from nearest */
    if (freq_hz < limit->segments[0].freq_start_hz) {
        return limit->segments[0].limit_start_dbuv;
    }
    int last = limit->num_segments - 1;
    return limit->segments[last].limit_stop_dbuv;
}

double emi_evaluate_margin(double emission_dbuv, double freq_hz,
                           const emi_limit_line_t *limit) {
    double limit_val = emi_limit_line_interpolate(limit, freq_hz);
    return limit_val - emission_dbuv; /* positive = pass, negative = fail */
}

/* ─── Pre-defined Limit Lines ────────────────────────────────────────── */

/**
 * Initialize CISPR 32 Class B conducted emission limit (150 kHz - 30 MHz).
 *
 * Frequency ranges and limits per CISPR 32 Table A.10 (AC mains):
 *   0.15 - 0.5 MHz: 66 → 56 dBuV quasi-peak (decreasing with log f)
 *                  : 56 → 46 dBuV average
 *   0.5 - 5 MHz:   56 dBuV QP, 46 dBuV AVG
 *   5 - 30 MHz:    60 dBuV QP, 50 dBuV AVG
 */
int emi_limit_init_cispr32_conducted_classb(emi_limit_line_t *limit,
                                             emi_detector_type_t detector) {
    if (!limit) return -1;
    /* 3 segments for the piecewise curve */
    limit->segments = (emi_limit_segment_t *)calloc(3, sizeof(emi_limit_segment_t));
    if (!limit->segments) return -1;
    limit->num_segments = 3;
    limit->standard = EMI_STD_CISPR32;
    limit->emission_type = EMI_EMISSION_CONDUCTED;

    if (detector == EMI_DETECTOR_QUASIPEAK) {
        /* Segment 1: 0.15 MHz (66) -> 0.5 MHz (56) */
        limit->segments[0].freq_start_hz = 150e3;
        limit->segments[0].freq_stop_hz  = 500e3;
        limit->segments[0].limit_start_dbuv = 66.0;
        limit->segments[0].limit_stop_dbuv  = 56.0;
        limit->segments[0].detector = EMI_DETECTOR_QUASIPEAK;
        limit->segments[0].band_name = "CISPR32 Class B QP 0.15-0.5 MHz";
        /* Segment 2: 0.5 MHz (56) -> 5 MHz (56) — flat */
        limit->segments[1].freq_start_hz = 500e3;
        limit->segments[1].freq_stop_hz  = 5e6;
        limit->segments[1].limit_start_dbuv = 56.0;
        limit->segments[1].limit_stop_dbuv  = 56.0;
        limit->segments[1].detector = EMI_DETECTOR_QUASIPEAK;
        limit->segments[1].band_name = "CISPR32 Class B QP 0.5-5 MHz";
        /* Segment 3: 5 MHz (60) -> 30 MHz (60) — flat */
        limit->segments[2].freq_start_hz = 5e6;
        limit->segments[2].freq_stop_hz  = 30e6;
        limit->segments[2].limit_start_dbuv = 60.0;
        limit->segments[2].limit_stop_dbuv  = 60.0;
        limit->segments[2].detector = EMI_DETECTOR_QUASIPEAK;
        limit->segments[2].band_name = "CISPR32 Class B QP 5-30 MHz";
    } else {
        /* Average detector limits */
        limit->segments[0].freq_start_hz = 150e3;
        limit->segments[0].freq_stop_hz  = 500e3;
        limit->segments[0].limit_start_dbuv = 56.0;
        limit->segments[0].limit_stop_dbuv  = 46.0;
        limit->segments[0].detector = EMI_DETECTOR_AVERAGE;
        limit->segments[0].band_name = "CISPR32 Class B AVG 0.15-0.5 MHz";
        limit->segments[1].freq_start_hz = 500e3;
        limit->segments[1].freq_stop_hz  = 5e6;
        limit->segments[1].limit_start_dbuv = 46.0;
        limit->segments[1].limit_stop_dbuv  = 46.0;
        limit->segments[1].detector = EMI_DETECTOR_AVERAGE;
        limit->segments[1].band_name = "CISPR32 Class B AVG 0.5-5 MHz";
        limit->segments[2].freq_start_hz = 5e6;
        limit->segments[2].freq_stop_hz  = 30e6;
        limit->segments[2].limit_start_dbuv = 50.0;
        limit->segments[2].limit_stop_dbuv  = 50.0;
        limit->segments[2].detector = EMI_DETECTOR_AVERAGE;
        limit->segments[2].band_name = "CISPR32 Class B AVG 5-30 MHz";
    }
    return 0;
}

/**
 * Initialize CISPR 32 Class B radiated emission limit (30 MHz - 6 GHz).
 *
 * Per CISPR 32 Table A.5 (3m distance):
 *   30 - 230 MHz:   40 dBuV/m QP
 *   230 - 1000 MHz: 47 dBuV/m QP
 *   1 - 3 GHz:      70 dBuV/m Peak, 50 dBuV/m AVG
 *   3 - 6 GHz:      74 dBuV/m Peak, 54 dBuV/m AVG
 */
int emi_limit_init_cispr32_radiated_classb(emi_limit_line_t *limit,
                                            emi_detector_type_t detector) {
    if (!limit) return -1;
    limit->segments = (emi_limit_segment_t *)calloc(4, sizeof(emi_limit_segment_t));
    if (!limit->segments) return -1;
    limit->num_segments = 4;
    limit->standard = EMI_STD_CISPR32;
    limit->emission_type = EMI_EMISSION_RADIATED;

    if (detector == EMI_DETECTOR_QUASIPEAK) {
        limit->segments[0].freq_start_hz = 30e6;
        limit->segments[0].freq_stop_hz  = 230e6;
        limit->segments[0].limit_start_dbuv = 40.0;
        limit->segments[0].limit_stop_dbuv  = 40.0;
        limit->segments[0].detector = EMI_DETECTOR_QUASIPEAK;
        limit->segments[0].band_name = "CISPR32 Class B Rad QP 30-230 MHz";
        limit->segments[1].freq_start_hz = 230e6;
        limit->segments[1].freq_stop_hz  = 1e9;
        limit->segments[1].limit_start_dbuv = 47.0;
        limit->segments[1].limit_stop_dbuv  = 47.0;
        limit->segments[1].detector = EMI_DETECTOR_QUASIPEAK;
        limit->segments[1].band_name = "CISPR32 Class B Rad QP 230-1000 MHz";
    } else if (detector == EMI_DETECTOR_PEAK) {
        limit->segments[2].freq_start_hz = 1e9;
        limit->segments[2].freq_stop_hz  = 3e9;
        limit->segments[2].limit_start_dbuv = 70.0;
        limit->segments[2].limit_stop_dbuv  = 70.0;
        limit->segments[2].detector = EMI_DETECTOR_PEAK;
        limit->segments[2].band_name = "CISPR32 Class B Rad Peak 1-3 GHz";
        limit->segments[3].freq_start_hz = 3e9;
        limit->segments[3].freq_stop_hz  = 6e9;
        limit->segments[3].limit_start_dbuv = 74.0;
        limit->segments[3].limit_stop_dbuv  = 74.0;
        limit->segments[3].detector = EMI_DETECTOR_PEAK;
        limit->segments[3].band_name = "CISPR32 Class B Rad Peak 3-6 GHz";
    } else {
        /* Average limits for > 1GHz */
        limit->segments[2].freq_start_hz = 1e9;
        limit->segments[2].freq_stop_hz  = 3e9;
        limit->segments[2].limit_start_dbuv = 50.0;
        limit->segments[2].limit_stop_dbuv  = 50.0;
        limit->segments[2].detector = EMI_DETECTOR_AVERAGE;
        limit->segments[2].band_name = "CISPR32 Class B Rad AVG 1-3 GHz";
        limit->segments[3].freq_start_hz = 3e9;
        limit->segments[3].freq_stop_hz  = 6e9;
        limit->segments[3].limit_start_dbuv = 54.0;
        limit->segments[3].limit_stop_dbuv  = 54.0;
        limit->segments[3].detector = EMI_DETECTOR_AVERAGE;
        limit->segments[3].band_name = "CISPR32 Class B Rad AVG 3-6 GHz";
    }
    return 0;
}

/**
 * Initialize FCC Part 15 Class B radiated limits.
 *   30 - 88 MHz:    40 dBuV/m (3m)
 *   88 - 216 MHz:   43.5 dBuV/m (3m)
 *   216 - 960 MHz:  46 dBuV/m (3m)
 *   > 960 MHz:      54 dBuV/m AVG / 74 dBuV/m Peak (3m)
 */
int emi_limit_init_fcc15_radiated_classb(emi_limit_line_t *limit) {
    if (!limit) return -1;
    limit->segments = (emi_limit_segment_t *)calloc(3, sizeof(emi_limit_segment_t));
    if (!limit->segments) return -1;
    limit->num_segments = 3;
    limit->standard = EMI_STD_FCC_PART15;
    limit->emission_type = EMI_EMISSION_RADIATED;

    limit->segments[0].freq_start_hz = 30e6;
    limit->segments[0].freq_stop_hz  = 88e6;
    limit->segments[0].limit_start_dbuv = 40.0;
    limit->segments[0].limit_stop_dbuv  = 40.0;
    limit->segments[0].detector = EMI_DETECTOR_QUASIPEAK;
    limit->segments[0].band_name = "FCC15 Class B 30-88 MHz";

    limit->segments[1].freq_start_hz = 88e6;
    limit->segments[1].freq_stop_hz  = 216e6;
    limit->segments[1].limit_start_dbuv = 43.5;
    limit->segments[1].limit_stop_dbuv  = 43.5;
    limit->segments[1].detector = EMI_DETECTOR_QUASIPEAK;
    limit->segments[1].band_name = "FCC15 Class B 88-216 MHz";

    limit->segments[2].freq_start_hz = 216e6;
    limit->segments[2].freq_stop_hz  = 960e6;
    limit->segments[2].limit_start_dbuv = 46.0;
    limit->segments[2].limit_stop_dbuv  = 46.0;
    limit->segments[2].detector = EMI_DETECTOR_QUASIPEAK;
    limit->segments[2].band_name = "FCC15 Class B 216-960 MHz";

    return 0;
}

/**
 * Free the resources allocated for a limit line.
 */
void emi_limit_line_free(emi_limit_line_t *limit) {
    if (limit && limit->segments) {
        free(limit->segments);
        limit->segments = NULL;
        limit->num_segments = 0;
    }
}

/* ─── Scan Config Defaults ───────────────────────────────────────────── */

/**
 * Initialize a scan configuration with sensible defaults.
 * For conducted emissions: 150 kHz - 30 MHz, 9 kHz RBW, peak detector.
 * For radiated emissions: 30 MHz - 1 GHz, 120 kHz RBW, peak detector.
 */
void emi_scan_config_init(emi_scan_config_t *config, emi_emission_type_t type,
                           emi_standard_t standard) {
    if (!config) return;
    memset(config, 0, sizeof(*config));

    if (type == EMI_EMISSION_CONDUCTED) {
        config->start_freq_hz = 150e3;
        config->stop_freq_hz  = 30e6;
        config->rbw_hz        = EMI_CONDUCTED_RBW_HZ;
    } else {
        config->start_freq_hz = 30e6;
        config->stop_freq_hz  = 1e9;
        config->rbw_hz        = EMI_RADIATED_RBW_HZ;
    }
    config->vbw_hz         = emi_auto_vbw(config->rbw_hz, EMI_DETECTOR_PEAK);
    config->attenuation_db = 10.0;
    config->preamp_gain_db = 0.0;
    config->detector       = EMI_DETECTOR_PEAK;
    config->standard       = standard;
    config->average_count  = 1;
    /* Compute reasonable number of points */
    double span_hz = config->stop_freq_hz - config->start_freq_hz;
    config->num_points = (int)(span_hz / (config->rbw_hz / 2.0));
    if (config->num_points > 10000) config->num_points = 10000;
    if (config->num_points < 100)  config->num_points = 100;
    /* Minimum sweep time */
    config->sweep_time_s = emi_min_sweep_time(span_hz, config->rbw_hz,
                                               config->vbw_hz, 3.0);
}

/* ─── System Sensitivity Analysis ────────────────────────────────────── */

/**
 * Compute the minimum measurable field strength for a given system config.
 * E_min_dBuVm = NF_dB + 10*log10(RBW) - 174 + SNR_min + AF + CableLoss - PreampGain + 107
 *
 * This is a key metric for pre-compliance: determines the noise floor
 * of the measurement system and whether emissions at the limit line
 * can be reliably measured.
 *
 * @param freq_hz       Frequency of interest
 * @param rbw_hz        Resolution bandwidth
 * @param nf_db         Overall system noise figure
 * @param snr_min_db    Minimum required SNR for detection (typ. 6 dB)
 * @param af_db         Antenna factor at this frequency
 * @param cable_loss_db Cable loss
 * @param preamp_gain_db Preamplifier gain (0 if not used)
 * @return Minimum detectable field strength, dBuV/m
 */
double emi_min_detectable_field(double freq_hz, double rbw_hz, double nf_db,
                                 double snr_min_db, double af_db,
                                 double cable_loss_db, double preamp_gain_db) {
    double mds_dbm = emi_noise_floor_dbm(rbw_hz, nf_db);
    double rx_sensitivity_dbm = mds_dbm + snr_min_db;
    return emi_field_strength_from_rx_power(rx_sensitivity_dbm, freq_hz,
                                             af_db, cable_loss_db, preamp_gain_db);
}

/**
 * Check if the measurement system can resolve the limit line.
 * The noise floor should be at least 6 dB below the limit for
 * reliable measurements.
 *
 * @return 1 if the system is adequate, 0 if noise floor is too high
 */
int emi_check_system_sensitivity(double freq_hz, double limit_dbuvm,
                                  double rbw_hz, double nf_db,
                                  double af_db, double cable_loss_db,
                                  double preamp_gain_db) {
    double e_min = emi_min_detectable_field(freq_hz, rbw_hz, nf_db,
                                             6.0, af_db, cable_loss_db,
                                             preamp_gain_db);
    return (e_min + 6.0 <= limit_dbuvm) ? 1 : 0;
}

/* ─── Emission Point Array Operations ────────────────────────────────── */

/**
 * Allocate and initialize an array of emission points.
 */
int emi_emission_points_alloc(emi_emission_point_t **points, int count) {
    if (!points || count <= 0) return -1;
    *points = (emi_emission_point_t *)calloc(count, sizeof(emi_emission_point_t));
    return (*points) ? 0 : -1;
}

/**
 * Free emission point array.
 */
void emi_emission_points_free(emi_emission_point_t *points) {
    free(points);
}

/**
 * Evaluate margin against a limit for all points in a scan result.
 * Updates the margin_db and status fields of each point.
 */
void emi_evaluate_all_margins(emi_scan_result_t *result,
                               const emi_limit_line_t *limit) {
    if (!result || !limit || !result->points) return;
    result->worst_margin_db = 1e9;
    result->num_marginal_points = 0;
    result->num_failing_points = 0;

    for (int i = 0; i < result->num_points; i++) {
        emi_emission_point_t *pt = &result->points[i];
        pt->limit_dbuv = emi_limit_line_interpolate(limit, pt->frequency_hz);
        pt->margin_db  = pt->limit_dbuv - pt->amplitude_dbuv;

        if (pt->margin_db >= 6.0) {
            pt->status = EMI_MARGIN_PASS;
        } else if (pt->margin_db >= 0.0) {
            pt->status = EMI_MARGIN_MARGINAL;
            result->num_marginal_points++;
        } else {
            pt->status = EMI_MARGIN_FAIL;
            result->num_failing_points++;
        }

        if (pt->margin_db < result->worst_margin_db) {
            result->worst_margin_db = pt->margin_db;
        }
        if (pt->amplitude_dbuv > result->max_emission_dbuv) {
            result->max_emission_dbuv = pt->amplitude_dbuv;
            result->max_emission_freq_hz = pt->frequency_hz;
        }
    }
}
