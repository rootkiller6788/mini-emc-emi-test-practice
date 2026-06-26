/** @file lisn_measurement.c - Conducted Emission Measurement Procedures */
/** CISPR 16-1-1 (Detectors), CISPR 16-2-1 (Methods), CISPR 16-4-2 (Uncertainty) */
/** L1(detector types), L5(QP algorithm, sweep engine), L6(compliance measurement) */

#include "lisn_measurement.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* L5: Peak detector - takes maximum envelope value over observation interval */
/* Fastest detector type, used for pre-scan/preview measurements */
/* Peak >= QP >= Average for the same input signal (always) */
double lisn_peak_detect(const double *samples, int num_samples, double impedance_ohm) {
    if (!samples || num_samples <= 0 || impedance_ohm <= 0.0) return -999.0;
    double max_v = 0.0;
    int i;
    for (i = 0; i < num_samples; i++) {
        double abs_v = fabs(samples[i]);
        if (abs_v > max_v) max_v = abs_v;
    }
    /* Convert peak voltage to dBuV: P = V^2/R, then convert */
    /* dBuV = 20*log10(V / 1e-6) */
    if (max_v <= 0.0) return 0.0;
    return 20.0 * log10(max_v / 1.0e-6);
}

/* L5: Quasi-Peak detector per CISPR 16-1-1 Annex B */
/* Uses charge time constant (tau_c) and discharge time constant (tau_d) */
/* to weight detector output by pulse repetition frequency */
/* dV/dt = (Vin - Vout)/tau_c  (charging, when Vin > Vout) */
/* dV/dt = -Vout/tau_d           (discharging, when Vin <= Vout) */
/* Numerical integration via Euler method with step dt */
double lisn_qp_detect(const double *samples, int num_samples, double sample_rate_hz,
                       const qp_detector_params_t *params, double impedance_ohm) {
    if (!samples || num_samples <= 0 || !params || sample_rate_hz <= 0.0) return -999.0;
    double dt = 1.0 / sample_rate_hz;
    double tau_c = params->charge_time_ms * 1.0e-3;
    double tau_d = params->discharge_time_ms * 1.0e-3;
    double v_out = 0.0;  /* Detector output voltage */
    int i;
    for (i = 0; i < num_samples; i++) {
        double v_in = fabs(samples[i]);
        if (v_in > v_out) {
            /* Charging phase */
            v_out += (v_in - v_out) * (dt / tau_c);
        } else {
            /* Discharging phase */
            v_out -= v_out * (dt / tau_d);
        }
        if (v_out < 0.0) v_out = 0.0;
    }
    if (v_out <= 0.0) return 0.0;
    return 20.0 * log10(v_out / 1.0e-6);
}

/* L5: Average detector - sliding window integration */
double lisn_avg_detect(const double *samples, int num_samples, double sample_rate_hz,
                       const avg_detector_params_t *params, double impedance_ohm) {
    if (!samples || num_samples <= 0 || !params || sample_rate_hz <= 0.0) return -999.0;
    int window_samples = (int)(params->averaging_time_ms * 1.0e-3 * sample_rate_hz);
    if (window_samples <= 0 || window_samples > num_samples) window_samples = num_samples;
    double sum = 0.0;
    int i;
    for (i = 0; i < window_samples; i++) { sum += fabs(samples[i]); }
    double avg_v = sum / (double)window_samples;
    if (avg_v <= 0.0) return 0.0;
    return 20.0 * log10(avg_v / 1.0e-6);
}

/* L5: RMS detector - sqrt of mean of squares */
double lisn_rms_detect(const double *samples, int num_samples, double impedance_ohm) {
    if (!samples || num_samples <= 0) return -999.0;
    double sum_sq = 0.0;
    int i;
    for (i = 0; i < num_samples; i++) { sum_sq += samples[i] * samples[i]; }
    double rms_v = sqrt(sum_sq / (double)num_samples);
    if (rms_v <= 0.0) return 0.0;
    return 20.0 * log10(rms_v / 1.0e-6);
}

/* L5: Logarithmically-spaced frequency array (CISPR standard) */
/* f[i] = f_start * 10^(i * log10(f_stop/f_start) / (N-1)) */
void lisn_log_frequency_array(double f_start, double f_stop, int n, double *freqs) {
    if (!freqs || n <= 0 || f_start <= 0.0 || f_stop <= f_start) return;
    if (n == 1) { freqs[0] = f_start; return; }
    double log_ratio = log10(f_stop / f_start) / (double)(n - 1);
    int i;
    for (i = 0; i < n; i++) { freqs[i] = f_start * pow(10.0, i * log_ratio); }
}

/* L5: Linearly-spaced frequency array */
/* f[i] = f_start + i * (f_stop - f_start) / (N-1) */
void lisn_linear_frequency_array(double f_start, double f_stop, int n, double *freqs) {
    if (!freqs || n <= 0 || f_stop <= f_start) return;
    if (n == 1) { freqs[0] = f_start; return; }
    double step = (f_stop - f_start) / (double)(n - 1);
    int i;
    for (i = 0; i < n; i++) { freqs[i] = f_start + i * step; }
}

/* L5: Apply transducer factor corrections to raw measurement */
/* Corrected(dBuV) = Raw + Cable + VDF + Attenuator - Preamp */
double lisn_apply_corrections(double raw, double cable, double vdf, double atten, double preamp) {
    return raw + cable + vdf + atten - preamp;
}

/* L5: Ambient noise subtraction per CISPR 16-2-3 */
/* Corrected = 10*log10(10^(EUT/10) - 10^(Ambient/10)) */
/* If ambient is within 6 dB of EUT, measurement is ambient-limited */
double lisn_ambient_correction(double eut, double ambient) {
    if (eut <= ambient) return eut;  /* No correction possible */
    double eut_lin = pow(10.0, eut / 10.0);
    double amb_lin = pow(10.0, ambient / 10.0);
    double diff = eut_lin - amb_lin;
    if (diff <= 0.0) return eut;
    return 10.0 * log10(diff);
}

/* L5: Compute combined standard uncertainty from component list */
/* uc = sqrt(sum((ci * ui)^2)) where ui = value_db / probability_factor */
void lisn_compute_uncertainty(lisn_uncertainty_budget_t *budget) {
    if (!budget || !budget->components || budget->num_components <= 0) return;
    double sum_sq = 0.0;
    int i;
    for (i = 0; i < budget->num_components; i++) {
        double ui = budget->components[i].value_db / budget->components[i].probability_factor;
        double ci = budget->components[i].sensitivity_coeff;
        sum_sq += (ci * ui) * (ci * ui);
    }
    budget->combined_standard_uncertainty_db = sqrt(sum_sq);
    budget->expanded_uncertainty_db = 2.0 * budget->combined_standard_uncertainty_db;
}

/* L5: Build standard CISPR 16-4-2 uncertainty budget for conducted emissions */
void lisn_uncertainty_budget_standard(lisn_uncertainty_budget_t *budget) {
    if (!budget) return;
    /* Pre-defined standard budget with 6 components */
    static lisn_uncertainty_component_t std_comps[6];
    strncpy(std_comps[0].source, "Receiver amplitude accuracy", 127);
    std_comps[0].value_db = 1.0; std_comps[0].probability_factor = 2.0;
    std_comps[0].sensitivity_coeff = 1.0;
    strncpy(std_comps[0].distribution, "normal (k=2)", 31);
    strncpy(std_comps[1].source, "LISN impedance tolerance", 127);
    std_comps[1].value_db = 0.5; std_comps[1].probability_factor = 1.732;
    std_comps[1].sensitivity_coeff = 1.0;
    strncpy(std_comps[1].distribution, "rectangular", 31);
    strncpy(std_comps[2].source, "Cable loss calibration", 127);
    std_comps[2].value_db = 0.3; std_comps[2].probability_factor = 2.0;
    std_comps[2].sensitivity_coeff = 1.0;
    strncpy(std_comps[2].distribution, "normal (k=2)", 31);
    strncpy(std_comps[3].source, "Mismatch: LISN to receiver", 127);
    std_comps[3].value_db = 0.3; std_comps[3].probability_factor = 1.414;
    std_comps[3].sensitivity_coeff = 1.0;
    strncpy(std_comps[3].distribution, "U-shaped", 31);
    strncpy(std_comps[4].source, "AMN impedance", 127);
    std_comps[4].value_db = 1.0; std_comps[4].probability_factor = 1.732;
    std_comps[4].sensitivity_coeff = 1.0;
    strncpy(std_comps[4].distribution, "rectangular", 31);
    strncpy(std_comps[5].source, "Site imperfections", 127);
    std_comps[5].value_db = 0.5; std_comps[5].probability_factor = 1.732;
    std_comps[5].sensitivity_coeff = 1.0;
    strncpy(std_comps[5].distribution, "rectangular", 31);
    budget->num_components = 6;
    budget->components = std_comps;
    lisn_compute_uncertainty(budget);
}
/* L5: Execute full frequency sweep over a conducted emission range */
/* Integrates frequency stepping, receiver configuration, and data logging */
void lisn_execute_sweep(const lisn_sweep_config_t *swp,
                        const lisn_receiver_config_t *rcvr,
                        const double *raw_levels, int n_levels,
                        lisn_measurement_dataset_t *dataset) {
    if (!swp || !rcvr || !raw_levels || !dataset || n_levels <= 0) return;
    dataset->sweep = *swp;
    dataset->receiver = *rcvr;
    dataset->num_results = (n_levels < swp->num_points) ? n_levels : swp->num_points;
    dataset->results = malloc((size_t)dataset->num_results * sizeof(lisn_emission_result_t));
    if (!dataset->results) return;
    double *freqs = malloc((size_t)dataset->num_results * sizeof(double));
    if (!freqs) { free(dataset->results); dataset->results = NULL; return; }
    if (swp->use_log_spacing)
        lisn_log_frequency_array(swp->freq_start_hz, swp->freq_stop_hz,
                                  dataset->num_results, freqs);
    else
        lisn_linear_frequency_array(swp->freq_start_hz, swp->freq_stop_hz,
                                      dataset->num_results, freqs);
    dataset->overall_margin_db = 999.0;
    dataset->worst_frequency_hz = swp->freq_start_hz;
    int i;
    for (i = 0; i < dataset->num_results; i++) {
        dataset->results[i].frequency_hz = freqs[i];
        double raw = (i < n_levels) ? raw_levels[i] : 0.0;
        double corrected = lisn_apply_corrections(raw, 1.0, 0.5,
                                rcvr->rf_attenuation_db,
                                rcvr->preamp_enabled ? rcvr->preamp_gain_db : 0.0);
        dataset->results[i].peak_level_dbuV = corrected;
        dataset->results[i].final_level_dbuV = corrected;
        dataset->results[i].final_detector = rcvr->detector_type;
        dataset->results[i].margin_db = 999.0;
        if (corrected > dataset->worst_frequency_hz || i == 0) {
            dataset->worst_frequency_hz = freqs[i];
        }
        if (corrected > 0 && corrected < dataset->overall_margin_db) {
            dataset->overall_margin_db = corrected;
        }
    }
    free(freqs);
}

/* L5: Compute receiver noise figure from displayed average noise level */
/* NF(dB) = DANL(dBm/Hz) + 174 - 10*log10(BW) */
double lisn_receiver_noise_figure(double danl_dbm_per_hz, double bw_hz) {
    if (bw_hz <= 0.0) return 0.0;
    return danl_dbm_per_hz + 174.0 - 10.0 * log10(bw_hz);
}

/* L5: Calculate Signal-to-Noise Ratio for emission measurement */
/* SNR(dB) = signal_dBuV - noise_floor_dBuV */
double lisn_measurement_snr(double signal_dbuV, double noise_floor_dbuV) {
    return signal_dbuV - noise_floor_dbuV;
}

/* L5: Determine if measurement is ambient-limited */
/* Per CISPR 16-2-3: if EUT - ambient < 6 dB, measurement is ambient-limited */
int lisn_is_ambient_limited(double eut_dbuV, double ambient_dbuV) {
    return (eut_dbuV - ambient_dbuV < 6.0) ? 1 : 0;
}

/* L5: Free measurement dataset memory */
void lisn_measurement_dataset_free(lisn_measurement_dataset_t *ds) {
    if (!ds) return;
    if (ds->results) { free(ds->results); ds->results = NULL; }
    ds->num_results = 0;
}

/* L6: Find top N worst emission frequencies */
/* Returns indices of the N highest emission levels in the dataset */
void lisn_top_n_emissions(const lisn_measurement_dataset_t *ds, int n,
                           int *indices) {
    if (!ds || !ds->results || !indices || n <= 0 || ds->num_results <= 0) return;
    /* Simple partial sort: bubble N largest to front */
    int i, j;
    for (i = 0; i < ds->num_results && i < n; i++) {
        indices[i] = i;
        for (j = i + 1; j < ds->num_results; j++) {
            if (ds->results[j].final_level_dbuV > ds->results[indices[i]].final_level_dbuV)
                indices[i] = j;
        }
        /* Swap found max into position i */
        if (indices[i] != i) {
            lisn_emission_result_t tmp = ds->results[i];
            ds->results[i] = ds->results[indices[i]];
            ds->results[indices[i]] = tmp;
            indices[i] = i;
        }
    }
}

/* Additional L7/L8 application functions */

double lisn_mismatch_uncertainty(double gamma_source, double gamma_load) {
    double M = fabs(gamma_source) * fabs(gamma_load);
    return 20.0 * log10(1.0 + M);
}

void lisn_qp_detector_band_params(qp_band_t band, qp_detector_params_t *p) {
    if (!p) return;
    switch (band) {
        case QP_BAND_A: p->charge_time_ms = 45.0; p->discharge_time_ms = 500.0; p->if_bandwidth_hz = 200.0; break;
        case QP_BAND_B: p->charge_time_ms = 1.0; p->discharge_time_ms = 160.0; p->if_bandwidth_hz = 9000.0; break;
        case QP_BAND_C: p->charge_time_ms = 1.0; p->discharge_time_ms = 550.0; p->if_bandwidth_hz = 120000.0; break;
        case QP_BAND_D: p->charge_time_ms = 1.0; p->discharge_time_ms = 550.0; p->if_bandwidth_hz = 120000.0; break;
        default: p->charge_time_ms = 1.0; p->discharge_time_ms = 160.0; p->if_bandwidth_hz = 9000.0; break;
    }
    p->meter_time_constant_ms = 160.0;
    p->overload_factor_db = 6.0;
}

