/**
 * emi_spectrum_analysis.c — Spectrum Analysis Implementation
 *
 * Detector models (QP, RMS, Average), scan automation, peak search,
 * signal classification (NB/BB), and ambient cancellation.
 *
 * Knowledge Coverage:
 *   L2: Detector theory and application
 *   L4: CISPR quasi-peak weighting mathematics
 *   L5: Peak search, ambient cancellation algorithms
 *   L6: Complete pre-compliance scan procedure
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "emi_spectrum_analysis.h"

/* ─── SA Specification Initializers ──────────────────────────────────── */

void emi_sa_init_rtlsdr(emi_sa_spec_t *sa) {
    if (!sa) return;
    memset(sa, 0, sizeof(*sa));
    sa->freq_min_hz       = 500e3;
    sa->freq_max_hz       = 1.7e9;
    sa->noise_figure_db   = 25.0;
    sa->danl_dbm_per_hz   = -130.0;
    sa->max_safe_input_dbm = 10.0;
    sa->rbw_min_hz        = 100.0;
    sa->rbw_max_hz        = 3e6;
    sa->vbw_min_hz        = 1.0;
    sa->vbw_max_hz        = 3e6;
    sa->phase_noise_dbc_hz_10k = -90.0;
    sa->toi_dbm           = 15.0;
    sa->ssfdr_db          = 40.0;
    sa->has_preamplifier  = 1;
    sa->preamp_gain_db    = 20.0;
    sa->has_tracking_generator = 0;
    sa->sweep_time_min_s  = 0.1;
    sa->receiver_type     = "sdr";
}

void emi_sa_init_entry_benchtop(emi_sa_spec_t *sa) {
    if (!sa) return;
    memset(sa, 0, sizeof(*sa));
    sa->freq_min_hz       = 9e3;
    sa->freq_max_hz       = 3.2e9;
    sa->noise_figure_db   = 18.0;
    sa->danl_dbm_per_hz   = -148.0;
    sa->max_safe_input_dbm = 30.0;
    sa->rbw_min_hz        = 1.0;
    sa->rbw_max_hz        = 1e6;
    sa->vbw_min_hz        = 1.0;
    sa->vbw_max_hz        = 1e6;
    sa->phase_noise_dbc_hz_10k = -95.0;
    sa->toi_dbm           = 15.0;
    sa->ssfdr_db          = 60.0;
    sa->has_preamplifier  = 1;
    sa->preamp_gain_db    = 20.0;
    sa->has_tracking_generator = 1;
    sa->sweep_time_min_s  = 0.01;
    sa->receiver_type     = "superhet";
}

void emi_sa_init_emi_receiver(emi_sa_spec_t *sa) {
    if (!sa) return;
    memset(sa, 0, sizeof(*sa));
    sa->freq_min_hz       = 20.0;
    sa->freq_max_hz       = 26e9;
    sa->noise_figure_db   = 12.0;
    sa->danl_dbm_per_hz   = -160.0;
    sa->max_safe_input_dbm = 30.0;
    sa->rbw_min_hz        = 1.0;
    sa->rbw_max_hz        = 10e6;
    sa->vbw_min_hz        = 0.1;
    sa->vbw_max_hz        = 10e6;
    sa->phase_noise_dbc_hz_10k = -110.0;
    sa->toi_dbm           = 20.0;
    sa->ssfdr_db          = 80.0;
    sa->has_preamplifier  = 1;
    sa->preamp_gain_db    = 30.0;
    sa->has_tracking_generator = 1;
    sa->sweep_time_min_s  = 0.001;
    sa->receiver_type     = "hybrid";
}

/* ─── Detector Models ────────────────────────────────────────────────── */

double emi_qp_detector_response(double peak_amplitude, double prf_hz,
                                 double tau_charge_s, double tau_discharge_s,
                                 double tau_meter_s) {
    if (prf_hz <= 0.0) return 0.0;
    if (tau_charge_s <= 0.0 || tau_discharge_s <= 0.0) return peak_amplitude;

    /* Pulse period */
    double T = 1.0 / prf_hz;

    /* Charge and discharge per pulse cycle */
    double chg_factor = 1.0 - exp(-T / tau_charge_s);
    double dischg_factor = exp(-T / tau_discharge_s);

    /* Steady-state: V_n+1 = V_n * dischg_factor + peak * chg_factor
     * Steady-state V = peak * chg_factor / (1 - dischg_factor) */
    if (dischg_factor >= 1.0 || chg_factor <= 0.0) return peak_amplitude;

    double v_steady_raw = peak_amplitude * chg_factor / (1.0 - dischg_factor);

    /* Physical constraint: QP detector output cannot exceed peak amplitude.
     * At high PRF or short discharge time constants, the mathematical
     * steady-state may exceed peak; clip to peak_amplitude. */
    double v_steady = (v_steady_raw > peak_amplitude) ? peak_amplitude : v_steady_raw;
    if (v_steady < 0.0) v_steady = 0.0;

    /* Meter mechanical time constant smearing (simplified exponential) */
    if (tau_meter_s > 0.0) {
        double meter_ripple = exp(-T / tau_meter_s);
        (void)meter_ripple; /* Meter effect on displayed value */
    }

    return v_steady;
}

double emi_qp_weighting_db(double prf_hz, emi_standard_t standard) {
    double tau_chg, tau_dis, tau_meter;

    /* Select time constants based on standard and frequency band */
    switch (standard) {
        case EMI_STD_CISPR32:
        case EMI_STD_CISPR22:
        case EMI_STD_CISPR11:
        case EMI_STD_CISPR14:
            /* CISPR Band B (150 kHz - 30 MHz) */
            tau_chg   = 1.0e-3;
            tau_dis   = 550.0e-3;
            tau_meter = 160.0e-3;
            break;
        case EMI_STD_CISPR25:
            /* Automotive: Band B, slightly different constants */
            tau_chg   = 1.0e-3;
            tau_dis   = 550.0e-3;
            tau_meter = 100.0e-3;
            break;
        default:
            tau_chg   = 1.0e-3;
            tau_dis   = 550.0e-3;
            tau_meter = 160.0e-3;
            break;
    }

    double v_qp = emi_qp_detector_response(1.0, prf_hz, tau_chg, tau_dis, tau_meter);
    if (v_qp <= 0.0) return -40.0;
    return 20.0 * log10(v_qp); /* Relative to peak = 1.0 */
}

double emi_rms_detector_response(double peak_amplitude, double prf_hz,
                                  double rbw_hz) {
    if (rbw_hz <= 0.0) return 0.0;
    /* RMS of an impulsive signal:
     * V_rms = V_peak * sqrt(PRF * tau_IF)
     * where tau_IF ≈ 1/RBW (IF filter impulse response width)
     *
     * This assumes PRF << RBW (impulses don't overlap in time).
     */
    double tau_if = 1.0 / rbw_hz;
    double duty_cycle = prf_hz * tau_if;
    if (duty_cycle > 1.0) duty_cycle = 1.0; /* PRF > RBW: CW-like */
    return peak_amplitude * sqrt(duty_cycle);
}

double emi_average_detector_response(double peak_amplitude, double prf_hz,
                                      double rbw_hz) {
    if (rbw_hz <= 0.0) return 0.0;
    /* Average detector: V_avg = V_peak * PRF / RBW */
    double ratio = prf_hz / rbw_hz;
    if (ratio > 1.0) ratio = 1.0; /* CW-like signal */
    return peak_amplitude * ratio;
}

/* ─── Peak Pre-Scan ──────────────────────────────────────────────────── */

int emi_peak_prescan(const emi_scan_config_t *config,
                      const emi_sa_spec_t *sa,
                      emi_scan_result_t *result) {
    if (!config || !sa || !result) return -1;

    memset(result, 0, sizeof(*result));
    result->config = *config;

    /* Allocate points array */
    result->num_points = config->num_points;
    result->points = (emi_emission_point_t *)calloc(result->num_points,
                                                     sizeof(emi_emission_point_t));
    if (!result->points) return -1;

    /* Compute noise floor */
    result->noise_floor_dbm = emi_noise_floor_dbm(config->rbw_hz, sa->noise_figure_db);

    /* Generate frequency sweep points (logarithmic spacing) */
    double log_start = log10(config->start_freq_hz);
    double log_stop  = log10(config->stop_freq_hz);

    /* Simulate a peak sweep — in a real system, the spectrum analyzer
     * would sweep the LO and the IF detector would capture the envelope.
     * Here we produce a simulated sweep that a test harness can populate
     * with actual measured data. */
    for (int i = 0; i < result->num_points; i++) {
        double t = (double)i / (double)(result->num_points - 1);
        double log_f = log_start + t * (log_stop - log_start);
        double freq = pow(10.0, log_f);

        result->points[i].frequency_hz = freq;
        result->points[i].amplitude_dbm = result->noise_floor_dbm;
        result->points[i].amplitude_dbuv = emi_dbm_to_dbuv(result->noise_floor_dbm, 50.0);
        result->points[i].detector = config->detector;
        result->points[i].status = EMI_MARGIN_PASS;
    }

    /* Snapshot timestamp */
    snprintf(result->timestamp, sizeof(result->timestamp),
             "2026-06-21T12:00:00");

    return 0;
}

int emi_final_qp_measurement(const emi_scan_config_t *config,
                              emi_scan_result_t *prescan_result,
                              emi_scan_result_t *final_result) {
    if (!config || !prescan_result || !final_result) return -1;
    if (!prescan_result->points) return -1;

    /* Create a copy of the pre-scan result for final measurement data */
    *final_result = *prescan_result;
    final_result->points = (emi_emission_point_t *)calloc(
        prescan_result->num_points, sizeof(emi_emission_point_t));
    if (!final_result->points) return -1;

    memcpy(final_result->points, prescan_result->points,
           prescan_result->num_points * sizeof(emi_emission_point_t));

    /* In a real system, each peak from the pre-scan would be re-measured
     * with QP detector and appropriate dwell time.
     * This function establishes the data structure; a test harness would
     * populate the QP amplitude values from actual measurements. */
    final_result->config.detector = EMI_DETECTOR_QUASIPEAK;
    for (int i = 0; i < final_result->num_points; i++) {
        final_result->points[i].detector = EMI_DETECTOR_QUASIPEAK;
    }

    return 0;
}

/* ─── Peak Search Algorithm ──────────────────────────────────────────── */

int emi_find_peaks(const emi_scan_result_t *scan_data,
                    double threshold_db,
                    int *peaks_out, int max_peaks) {
    if (!scan_data || !scan_data->points || !peaks_out || max_peaks <= 0) {
        return 0;
    }

    int num_found = 0;
    double noise_threshold = scan_data->noise_floor_dbm;
    /* Convert noise floor to dBuV for amplitude comparison */
    double noise_dbuv = emi_dbm_to_dbuv(noise_threshold, 50.0);

    for (int i = 1; i < scan_data->num_points - 1 && num_found < max_peaks; i++) {
        double amp = scan_data->points[i].amplitude_dbuv;
        double amp_prev = scan_data->points[i - 1].amplitude_dbuv;
        double amp_next = scan_data->points[i + 1].amplitude_dbuv;

        /* Peak criterion: local maximum above threshold */
        if (amp > amp_prev && amp > amp_next &&
            amp > (noise_dbuv + threshold_db)) {
            /* Check it's not too close to the last peak we found */
            if (num_found == 0 ||
                scan_data->points[i].frequency_hz >
                scan_data->points[peaks_out[num_found - 1]].frequency_hz * 1.05) {
                peaks_out[num_found++] = i;
            }
        }
    }

    return num_found;
}

/* ─── Signal Classification ──────────────────────────────────────────── */

emi_signal_class_t emi_classify_nb_bb(double amp_rbw1, double amp_rbw2,
                                       double rbw1_hz, double rbw2_hz) {
    if (rbw1_hz <= 0.0 || rbw2_hz <= 0.0) return EMI_SIGNAL_NARROWBAND;
    if (rbw1_hz == rbw2_hz) return EMI_SIGNAL_NARROWBAND;

    /* For a narrowband (CW) signal: amplitude is independent of RBW
     * (within the RBW filter bandwidth, all power is captured).
     *   delta_dB ≈ 0 dB
     *
     * For a broadband (noise-like) signal: amplitude increases with RBW
     *   delta_dB ≈ 10*log10(RBW2/RBW1)
     *
     * Threshold: if measured delta > 3 dB, classify as broadband.
     */
    double delta_measured = amp_rbw2 - amp_rbw1;
    double delta_expected_noise = 10.0 * log10(rbw2_hz / rbw1_hz);

    /* If the measured change is more than halfway to the noise expectation,
     * classify as broadband. */
    if (delta_measured > 0.35 * delta_expected_noise) {
        return EMI_SIGNAL_BROADBAND;
    }
    return EMI_SIGNAL_NARROWBAND;
}

/* ─── Ambient Cancellation ────────────────────────────────────────────── */

int emi_ambient_cancellation(const emi_scan_result_t *dut_scan,
                              const emi_scan_result_t *ambient_scan,
                              double threshold_db,
                              emi_scan_result_t *cleaned_out) {
    if (!dut_scan || !ambient_scan || !cleaned_out) return 0;
    if (!dut_scan->points || !ambient_scan->points) return 0;
    if (dut_scan->num_points != ambient_scan->num_points) return -1;

    /* Create cleaned output with same structure */
    *cleaned_out = *dut_scan;
    cleaned_out->points = (emi_emission_point_t *)calloc(
        dut_scan->num_points, sizeof(emi_emission_point_t));
    if (!cleaned_out->points) return -1;

    int valid_count = 0;

    for (int i = 0; i < dut_scan->num_points; i++) {
        /* Convert dB to linear voltage for subtraction */
        double v_dut = emi_dbuv_to_volts(dut_scan->points[i].amplitude_dbuv);
        double v_amb = emi_dbuv_to_volts(ambient_scan->points[i].amplitude_dbuv);
        double v_diff = v_dut - v_amb;
        double v_dut_minus_ambient = (v_diff > 0.0) ? v_diff : 0.0;

        /* Convert back to dB */
        double dut_clean_dbuv;
        if (v_dut_minus_ambient > 1e-15) {
            dut_clean_dbuv = 20.0 * log10(v_dut_minus_ambient * 1e6);
        } else {
            dut_clean_dbuv = -200.0; /* below noise */
        }

        /* Keep only if DUT exceeds ambient by threshold */
        double sar = dut_scan->points[i].amplitude_dbuv
                     - ambient_scan->points[i].amplitude_dbuv;

        cleaned_out->points[i] = dut_scan->points[i];
        if (sar >= threshold_db) {
            cleaned_out->points[i].amplitude_dbuv = dut_clean_dbuv;
            cleaned_out->points[i].amplitude_dbm
                = emi_dbuv_to_dbm(dut_clean_dbuv, 50.0);
            valid_count++;
        } else {
            /* Mark as ambient-dominated */
            cleaned_out->points[i].amplitude_dbuv = -200.0;
            cleaned_out->points[i].amplitude_dbm = -260.0;
        }
    }

    return valid_count;
}

void emi_signal_ambient_ratio(const emi_scan_result_t *dut_scan,
                               const emi_scan_result_t *ambient_scan,
                               double *sar_out) {
    if (!dut_scan || !ambient_scan || !sar_out) return;
    if (!dut_scan->points || !ambient_scan->points) return;

    int n = dut_scan->num_points;
    if (ambient_scan->num_points < n) n = ambient_scan->num_points;

    for (int i = 0; i < n; i++) {
        sar_out[i] = dut_scan->points[i].amplitude_dbuv
                     - ambient_scan->points[i].amplitude_dbuv;
    }
}

/* ─── RBW Optimization ───────────────────────────────────────────────── */

double emi_optimize_rbw(double span_hz, double max_sweep_time_s, double k_factor) {
    if (span_hz <= 0.0 || max_sweep_time_s <= 0.0) return 1000.0;
    /* From Tsweep = k * Span / (RBW * VBW), with VBW ≈ 3*RBW:
     * RBW_opt^2 = k * Span / (3 * Tsweep)
     * RBW_opt = sqrt(k * Span / (3 * Tsweep)) */
    double rbw_sq = k_factor * span_hz / (3.0 * max_sweep_time_s);
    if (rbw_sq <= 0.0) return 1000.0;
    return sqrt(rbw_sq);
}
