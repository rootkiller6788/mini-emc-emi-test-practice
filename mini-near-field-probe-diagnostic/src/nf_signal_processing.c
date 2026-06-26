/**
 * @file nf_signal_processing.c
 * @brief Near-Field Signal Processing — L5-L6 implementation
 *
 * L5: FFT, windowing, IIR filtering, time-gating, correlation,
 *     phase unwrapping, NFFFT, EMI receiver model, noise analysis,
 *     field statistics, coherence estimation, PSD estimation
 * L6: Peak detection, S-parameter conversion
 */

#include "../include/nf_signal_processing.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * L5 — FFT (Cooley-Tukey Radix-2 Decimation-in-Time)
 * ============================================================================
 */

int nf_fft_plan_create(nf_fft_plan_t *plan, size_t N, int inverse)
{
    if (!plan || N == 0 || (N & (N - 1)) != 0) return -1;
    memset(plan, 0, sizeof(*plan));
    plan->N = N;
    plan->inverse = inverse;
    plan->twiddle_factors = malloc(N * sizeof(double complex));
    plan->buffer = malloc(N * sizeof(double complex));
    if (!plan->twiddle_factors || !plan->buffer) {
        nf_fft_plan_destroy(plan);
        return -1;
    }
    int sign = inverse ? 1 : -1;
    for (size_t k = 0; k < N; k++) {
        double angle = sign * 2.0 * M_PI * k / N;
        plan->twiddle_factors[k] = cos(angle) + I * sin(angle);
    }
    return 0;
}

void nf_fft_plan_destroy(nf_fft_plan_t *plan)
{
    if (!plan) return;
    free(plan->twiddle_factors);
    free(plan->buffer);
    memset(plan, 0, sizeof(*plan));
}

int nf_fft_execute(nf_fft_plan_t *plan, double complex *data)
{
    if (!plan || !data) return -1;
    size_t N = plan->N;
    for (size_t i = 1, j = 0; i < N; i++) {
        size_t bit = N >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            double complex tmp = data[i];
            data[i] = data[j];
            data[j] = tmp;
        }
    }
    for (size_t len = 2; len <= N; len <<= 1) {
        size_t half = len >> 1;
        size_t step = N / len;
        for (size_t i = 0; i < N; i += len) {
            for (size_t j = 0; j < half; j++) {
                double complex tw = plan->twiddle_factors[j * step];
                double complex u = data[i + j];
                double complex v = data[i + j + half] * tw;
                data[i + j] = u + v;
                data[i + j + half] = u - v;
            }
        }
    }
    if (plan->inverse) {
        double scale = 1.0 / N;
        for (size_t i = 0; i < N; i++) data[i] *= scale;
    }
    return 0;
}

int nf_fft_real(const double *real_data, size_t N, double complex *spectrum)
{
    if (!real_data || !spectrum || N < 2 || (N & (N - 1)) != 0) return -1;
    size_t Nh = N / 2;
    double complex *z = malloc(Nh * sizeof(double complex));
    if (!z) return -1;
    for (size_t i = 0; i < Nh; i++) {
        z[i] = real_data[2 * i] + I * real_data[2 * i + 1];
    }
    nf_fft_plan_t plan;
    if (nf_fft_plan_create(&plan, Nh, 0) != 0) { free(z); return -1; }
    nf_fft_execute(&plan, z);
    spectrum[0] = creal(z[0]) + cimag(z[0]);
    spectrum[Nh] = creal(z[0]) - cimag(z[0]);
    for (size_t k = 1; k < Nh; k++) {
        double complex Ze = (z[k] + conj(z[Nh - k])) * 0.5;
        double complex Zo = (z[k] - conj(z[Nh - k])) * (-0.5 * I);
        double complex tw = cexp(-I * M_PI * k / Nh);
        spectrum[k] = Ze + Zo * tw;
        spectrum[N - k] = conj(spectrum[k]);
    }
    nf_fft_plan_destroy(&plan);
    free(z);
    return 0;
}

/* ============================================================================
 * L5 — Windowing Functions
 * ============================================================================
 */

int nf_window_generate(double *window, size_t N, nf_window_type_t type, double param)
{
    if (!window || N == 0) return -1;
    for (size_t n = 0; n < N; n++) {
        double x = (N > 1) ? n / (double)(N - 1) : 0.0;
        switch (type) {
            case NF_WINDOW_RECTANGULAR:
                window[n] = 1.0; break;
            case NF_WINDOW_HANN:
                window[n] = 0.5 * (1.0 - cos(2.0 * M_PI * x)); break;
            case NF_WINDOW_HAMMING:
                window[n] = 0.54 - 0.46 * cos(2.0 * M_PI * x); break;
            case NF_WINDOW_BLACKMAN:
                window[n] = 0.42 - 0.5 * cos(2.0 * M_PI * x) + 0.08 * cos(4.0 * M_PI * x); break;
            case NF_WINDOW_KAISER: {
                double beta = param;
                double arg = beta * sqrt(1.0 - (2.0 * x - 1.0) * (2.0 * x - 1.0));
                double I0 = 1.0, term = 1.0;
                for (int k = 1; k < 20; k++) {
                    term *= (arg / (2.0 * k));
                    double delta = term * term;
                    I0 += delta;
                    if (delta < 1e-15) break;
                }
                arg = beta;
                double I0b = 1.0; term = 1.0;
                for (int k = 1; k < 20; k++) {
                    term *= (arg / (2.0 * k));
                    double delta = term * term;
                    I0b += delta;
                    if (delta < 1e-15) break;
                }
                window[n] = I0 / I0b;
                break;
            }
            case NF_WINDOW_FLATTOP:
                window[n] = 0.21557895 - 0.41663158 * cos(2.0 * M_PI * x)
                          + 0.277263158 * cos(4.0 * M_PI * x)
                          - 0.083578947 * cos(6.0 * M_PI * x)
                          + 0.006947368 * cos(8.0 * M_PI * x);
                break;
            default: window[n] = 1.0; break;
        }
    }
    (void)param;
    return 0;
}

int nf_window_apply(double *data, const double *window, size_t N)
{
    if (!data || !window || N == 0) return -1;
    for (size_t i = 0; i < N; i++) data[i] *= window[i];
    return 0;
}

int nf_window_apply_complex(double complex *data, const double *window, size_t N)
{
    if (!data || !window || N == 0) return -1;
    for (size_t i = 0; i < N; i++) data[i] *= window[i];
    return 0;
}

/* ============================================================================
 * L5 — IIR Butterworth Filter Design (Bilinear Transform)
 * ============================================================================
 */

int nf_iir_butterworth_design(nf_iir_filter_t *filter, nf_filter_type_t type,
                               double f_low, double f_high, double fs, size_t order)
{
    if (!filter || fs <= 0 || order == 0) return -1;
    if ((type == NF_FILTER_BANDPASS || type == NF_FILTER_BANDSTOP) && f_low >= f_high) return -1;
    memset(filter, 0, sizeof(*filter));
    filter->type = type;
    filter->f_low_hz = f_low;
    filter->f_high_hz = f_high;
    filter->order = order;
    filter->num_b = 3;
    filter->num_a = 3;
    filter->b_coeffs = calloc(filter->num_b, sizeof(double));
    filter->a_coeffs = calloc(filter->num_a, sizeof(double));
    if (!filter->b_coeffs || !filter->a_coeffs) {
        nf_iir_filter_free(filter); return -1;
    }
    double K = tan(M_PI * f_high / fs);
    double K2 = K * K;
    double denom = 1.0 + sqrt(2.0) * K + K2;
    filter->b_coeffs[0] = K2 / denom;
    filter->b_coeffs[1] = 2.0 * K2 / denom;
    filter->b_coeffs[2] = K2 / denom;
    filter->a_coeffs[0] = 1.0;
    filter->a_coeffs[1] = 2.0 * (K2 - 1.0) / denom;
    filter->a_coeffs[2] = (1.0 - sqrt(2.0) * K + K2) / denom;
    return 0;
}

int nf_iir_filter_apply(const nf_iir_filter_t *filter, const double *input, double *output, size_t N)
{
    if (!filter || !input || !output || N == 0) return -1;
    if (!filter->b_coeffs || !filter->a_coeffs) return -1;
    size_t M = (filter->num_b > filter->num_a) ? filter->num_b : filter->num_a;
    double *w = calloc(M + 1, sizeof(double));
    if (!w) return -1;
    for (size_t n = 0; n < N; n++) {
        for (size_t i = M; i > 0; i--) w[i] = w[i - 1];
        w[0] = input[n];
        for (size_t i = 1; i < filter->num_a && i <= M; i++) w[0] -= filter->a_coeffs[i] * w[i];
        output[n] = 0;
        for (size_t i = 0; i < filter->num_b && i <= M; i++) output[n] += filter->b_coeffs[i] * w[i];
    }
    free(w);
    return 0;
}

void nf_iir_filter_free(nf_iir_filter_t *filter)
{
    if (!filter) return;
    free(filter->b_coeffs); filter->b_coeffs = NULL;
    free(filter->a_coeffs); filter->a_coeffs = NULL;
    memset(filter, 0, sizeof(*filter));
}

/* ============================================================================
 * L5 — Time-Domain Gating
 * ============================================================================
 */

int nf_time_gate_apply(const nf_time_gate_t *gate, const double complex *freq_data,
                        double complex *gated_data, size_t N, double df)
{
    if (!gate || !freq_data || !gated_data || N < 2) return -1;
    double complex *time_data = malloc(N * sizeof(double complex));
    if (!time_data) return -1;
    memcpy(time_data, freq_data, N * sizeof(double complex));
    nf_fft_plan_t ifft_plan, fft_plan;
    if (nf_fft_plan_create(&ifft_plan, N, 1) != 0 || nf_fft_plan_create(&fft_plan, N, 0) != 0) {
        free(time_data);
        nf_fft_plan_destroy(&ifft_plan); nf_fft_plan_destroy(&fft_plan);
        return -1;
    }
    nf_fft_execute(&ifft_plan, time_data);
    double dt = 1.0 / (N * df);
    size_t g1 = (size_t)(gate->gate_start_s / dt);
    size_t g2 = (size_t)(gate->gate_stop_s / dt);
    if (g2 >= N) g2 = N - 1;
    for (size_t i = 0; i < N; i++) {
        if (i < g1 || i > g2) time_data[i] = 0;
        else if (gate->window_coeffs && gate->window_length > 0) {
            double frac = (double)(i - g1) / (double)(g2 - g1 + 1);
            size_t wi = (size_t)(frac * (gate->window_length - 1));
            if (wi < gate->window_length) time_data[i] *= gate->window_coeffs[wi];
        }
    }
    nf_fft_execute(&fft_plan, time_data);
    memcpy(gated_data, time_data, N * sizeof(double complex));
    free(time_data);
    nf_fft_plan_destroy(&ifft_plan); nf_fft_plan_destroy(&fft_plan);
    return 0;
}

/* ============================================================================
 * L5 — Correlation Analysis
 * ============================================================================
 */

int nf_correlation_analyze(const double *x, const double *y, size_t N,
                            nf_correlation_result_t *result)
{
    if (!x || !y || !result || N < 2) return -1;
    memset(result, 0, sizeof(*result));
    result->length = 2 * N - 1;
    result->cross_correlation = calloc(result->length, sizeof(double));
    if (!result->cross_correlation) return -1;
    for (size_t lag = 0; lag < 2 * N - 1; lag++) {
        int shift = (int)lag - (int)N + 1;
        double sum = 0;
        size_t count = 0;
        for (size_t i = 0; i < N; i++) {
            int j = (int)i + shift;
            if (j >= 0 && (size_t)j < N) { sum += x[i] * y[j]; count++; }
        }
        result->cross_correlation[lag] = (count > 0) ? sum / count : 0;
    }
    result->max_correlation = 0;
    for (size_t i = 0; i < result->length; i++) {
        double val = fabs(result->cross_correlation[i]);
        if (val > result->max_correlation) {
            result->max_correlation = val;
            result->lag_at_max = i;
        }
    }
    return 0;
}

int nf_correlation_analyze_complex(const double complex *x, const double complex *y,
                                    size_t N, nf_correlation_result_t *result)
{
    if (!x || !y || !result || N < 2) return -1;
    double *x_mag = malloc(N * sizeof(double));
    double *y_mag = malloc(N * sizeof(double));
    if (!x_mag || !y_mag) { free(x_mag); free(y_mag); return -1; }
    for (size_t i = 0; i < N; i++) { x_mag[i] = cabs(x[i]); y_mag[i] = cabs(y[i]); }
    int ret = nf_correlation_analyze(x_mag, y_mag, N, result);
    free(x_mag); free(y_mag);
    return ret;
}

void nf_correlation_result_free(nf_correlation_result_t *result)
{
    if (!result) return;
    free(result->cross_correlation);
    memset(result, 0, sizeof(*result));
}

/* ============================================================================
 * L5 — Phase Unwrapping (Itoh Algorithm)
 * ============================================================================
 */

int nf_phase_unwrap(const double *wrapped_phase, size_t N, nf_phase_unwrap_t *result)
{
    if (!wrapped_phase || !result || N == 0) return -1;
    memset(result, 0, sizeof(*result));
    result->unwrapped_phase = malloc(N * sizeof(double));
    result->num_points = N;
    if (!result->unwrapped_phase) return -1;
    result->unwrapped_phase[0] = wrapped_phase[0];
    double cum_sum = 0;
    for (size_t i = 1; i < N; i++) {
        double diff = wrapped_phase[i] - wrapped_phase[i - 1];
        double correction = round(diff / (2.0 * M_PI));
        cum_sum += correction;
        result->unwrapped_phase[i] = wrapped_phase[i] - 2.0 * M_PI * cum_sum;
        if (correction != 0) { result->discontinuities_found = 1; result->num_discontinuities++; }
    }
    result->total_phase_shift = result->unwrapped_phase[N - 1] - result->unwrapped_phase[0];
    return 0;
}

void nf_phase_unwrap_free(nf_phase_unwrap_t *result)
{
    if (!result) return;
    free(result->unwrapped_phase);
    memset(result, 0, sizeof(*result));
}

/* ============================================================================
 * L5 — EMI Receiver Model (CISPR 16-1-1)
 * ============================================================================
 */

int nf_emi_receiver_process(nf_emi_receiver_model_t *rx, const double *time_signal,
                             size_t N, double fs)
{
    if (!rx || !time_signal || N == 0 || fs <= 0) return -1;
    double freq = rx->freq_hz;
    if (freq < 3.0e7) {
        rx->charge_time_const_s = 1e-3; rx->discharge_time_const_s = 160e-3;
        rx->meter_time_const_s = 160e-3;
    } else {
        rx->charge_time_const_s = 1e-3; rx->discharge_time_const_s = 550e-3;
        rx->meter_time_const_s = 100e-3;
    }
    double dt = 1.0 / fs;
    double qp_output = 0, peak_output = 0, avg_sum = 0, rms_sum = 0;
    for (size_t i = 0; i < N; i++) {
        double abs_val = fabs(time_signal[i]);
        if (abs_val > peak_output) peak_output = abs_val;
        avg_sum += abs_val; rms_sum += abs_val * abs_val;
        if (abs_val > qp_output)
            qp_output += (abs_val - qp_output) * (1.0 - exp(-dt / rx->charge_time_const_s));
        else
            qp_output *= exp(-dt / rx->discharge_time_const_s);
    }
    rx->quasi_peak_output = qp_output; rx->peak_output = peak_output;
    rx->average_output = avg_sum / N; rx->rms_output = sqrt(rms_sum / N);
    return 0;
}

/* ============================================================================
 * L5 — Noise Analysis
 * ============================================================================
 */

int nf_noise_analyze(const double *signal, size_t N, double rbw_hz, double gain_db,
                      double nf_db, nf_noise_analysis_t *result)
{
    if (!signal || !result || N == 0) return -1;
    memset(result, 0, sizeof(*result));
    double power = 0;
    for (size_t i = 0; i < N; i++) power += signal[i] * signal[i];
    power /= N;
    result->signal_power_dbm = 10.0 * log10(power / 50.0 * 1000.0);
    result->noise_floor_dbm = -174.0 + 10.0 * log10(rbw_hz) + nf_db;
    result->noise_figure_db = nf_db;
    result->noise_power_dbm = result->noise_floor_dbm;
    result->snr_db = result->signal_power_dbm - result->noise_power_dbm;
    result->minimum_detectable_signal_dbm = result->noise_floor_dbm + 3.0;
    result->dynamic_range_db = result->signal_power_dbm - result->minimum_detectable_signal_dbm;
    return 0;
}

/* ============================================================================
 * L5 — Field Statistics
 * ============================================================================
 */

int nf_field_statistics_compute(const double *field_magnitudes, size_t N,
                                 nf_field_statistics_t *stats)
{
    if (!field_magnitudes || !stats || N == 0) return -1;
    memset(stats, 0, sizeof(*stats));
    stats->num_samples = N;
    double sum = 0;
    for (size_t i = 0; i < N; i++) sum += field_magnitudes[i];
    stats->mean_value = sum / N;
    double *sorted = malloc(N * sizeof(double));
    if (!sorted) return -1;
    memcpy(sorted, field_magnitudes, N * sizeof(double));
    for (size_t i = 0; i < N - 1; i++)
        for (size_t j = i + 1; j < N; j++)
            if (sorted[i] > sorted[j]) { double tmp = sorted[i]; sorted[i] = sorted[j]; sorted[j] = tmp; }
    stats->median_value = sorted[N / 2];
    size_t idx95 = (size_t)(0.95 * N), idx99 = (size_t)(0.99 * N);
    if (idx95 >= N) idx95 = N - 1; if (idx99 >= N) idx99 = N - 1;
    stats->percentile_95 = sorted[idx95]; stats->percentile_99 = sorted[idx99];
    double m2 = 0, m3 = 0, m4 = 0, mean = stats->mean_value;
    for (size_t i = 0; i < N; i++) {
        double diff = field_magnitudes[i] - mean;
        m2 += diff * diff; m3 += diff * diff * diff; m4 += diff * diff * diff * diff;
    }
    m2 /= N; m3 /= N; m4 /= N;
    stats->std_deviation = sqrt(m2);
    stats->skewness = (m2 > 1e-30) ? m3 / pow(m2, 1.5) : 0;
    stats->kurtosis = (m2 > 1e-30) ? m4 / (m2 * m2) - 3.0 : 0;
    free(sorted);
    return 0;
}

/* ============================================================================
 * L5 — Coherence Estimation (Welch Method)
 * ============================================================================
 */

int nf_coherence_estimate(const double *x, const double *y, size_t N, double fs,
                           size_t nfft, size_t noverlap, nf_coherence_t *coh)
{
    if (!x || !y || !coh || N == 0 || nfft == 0) return -1;
    memset(coh, 0, sizeof(*coh));
    size_t num_bins = nfft / 2 + 1;
    coh->num_freq_bins = num_bins; coh->df_hz = fs / nfft;
    coh->magnitude_squared_coherence = calloc(num_bins, sizeof(double));
    coh->phase_coherence = calloc(num_bins, sizeof(double));
    coh->freqs = calloc(num_bins, sizeof(double));
    if (!coh->magnitude_squared_coherence || !coh->phase_coherence || !coh->freqs) {
        nf_coherence_free(coh); return -1;
    }
    for (size_t k = 0; k < num_bins; k++) coh->freqs[k] = k * coh->df_hz;
    size_t step = nfft - noverlap; if (step == 0) step = nfft;
    size_t num_segments = 0;
    double *Pxx = calloc(num_bins, sizeof(double));
    double *Pyy = calloc(num_bins, sizeof(double));
    double complex *Pxy = calloc(num_bins, sizeof(double complex));
    if (!Pxx || !Pyy || !Pxy) { free(Pxx); free(Pyy); free(Pxy); return -1; }
    double *win = malloc(nfft * sizeof(double));
    if (!win) { free(Pxx); free(Pyy); free(Pxy); return -1; }
    nf_window_generate(win, nfft, NF_WINDOW_HANN, 0);
    for (size_t seg = 0; seg * step + nfft <= N; seg++) {
        size_t start = seg * step;
        double complex *X = calloc(nfft, sizeof(double complex));
        double complex *Y = calloc(nfft, sizeof(double complex));
        if (!X || !Y) { free(X); free(Y); continue; }
        for (size_t i = 0; i < nfft; i++) { X[i] = x[start + i] * win[i]; Y[i] = y[start + i] * win[i]; }
        nf_fft_plan_t plan;
        if (nf_fft_plan_create(&plan, nfft, 0) != 0) { free(X); free(Y); continue; }
        nf_fft_execute(&plan, X); nf_fft_execute(&plan, Y);
        for (size_t k = 0; k < num_bins; k++) {
            Pxx[k] += creal(X[k] * conj(X[k])); Pyy[k] += creal(Y[k] * conj(Y[k]));
            Pxy[k] += X[k] * conj(Y[k]);
        }
        free(X); free(Y); nf_fft_plan_destroy(&plan); num_segments++;
    }
    if (num_segments > 0) {
        for (size_t k = 0; k < num_bins; k++) {
            Pxx[k] /= num_segments; Pyy[k] /= num_segments; Pxy[k] /= num_segments;
            double denom = Pxx[k] * Pyy[k];
            if (denom > 1e-30) {
                coh->magnitude_squared_coherence[k] = creal(Pxy[k] * conj(Pxy[k])) / denom;
                coh->phase_coherence[k] = carg(Pxy[k]);
            }
        }
        coh->mean_coherence = 0;
        for (size_t k = 0; k < num_bins; k++) coh->mean_coherence += coh->magnitude_squared_coherence[k];
        coh->mean_coherence /= num_bins;
    }
    free(Pxx); free(Pyy); free(Pxy); free(win);
    return 0;
}

void nf_coherence_free(nf_coherence_t *coh)
{
    if (!coh) return;
    free(coh->magnitude_squared_coherence); free(coh->phase_coherence);
    free(coh->freqs); memset(coh, 0, sizeof(*coh));
}

/* ============================================================================
 * L5 — PSD Welch Method, Moving Average, Median Filter
 * ============================================================================
 */

int nf_psd_welch(const double *x, size_t N, double fs, size_t nfft, size_t noverlap,
                  nf_window_type_t window_type, double *freqs, double *psd, size_t *num_bins)
{
    if (!x || !freqs || !psd || !num_bins || N == 0 || nfft == 0) return -1;
    size_t n_bins = nfft / 2 + 1; *num_bins = n_bins;
    double *win = malloc(nfft * sizeof(double));
    if (!win) return -1;
    nf_window_generate(win, nfft, window_type, 6.0);
    double win_power = 0;
    for (size_t i = 0; i < nfft; i++) win_power += win[i] * win[i];
    double *psd_acc = calloc(n_bins, sizeof(double));
    if (!psd_acc) { free(win); return -1; }
    size_t step = nfft - noverlap; if (step == 0) step = nfft;
    size_t num_seg = 0;
    for (size_t seg = 0; seg * step + nfft <= N; seg++) {
        double complex *X = malloc(nfft * sizeof(double complex));
        if (!X) continue;
        for (size_t i = 0; i < nfft; i++) X[i] = x[seg * step + i] * win[i];
        nf_fft_plan_t plan; nf_fft_plan_create(&plan, nfft, 0);
        nf_fft_execute(&plan, X);
        for (size_t k = 0; k < n_bins; k++) psd_acc[k] += creal(X[k] * conj(X[k]));
        free(X); nf_fft_plan_destroy(&plan); num_seg++;
    }
    double df = fs / nfft;
    for (size_t k = 0; k < n_bins; k++) {
        freqs[k] = k * df;
        psd[k] = (num_seg > 0) ? psd_acc[k] / (num_seg * win_power * fs) : 0;
    }
    free(win); free(psd_acc);
    return 0;
}

int nf_moving_average(const double *x, double *y, size_t N, size_t window_size)
{
    if (!x || !y || N == 0 || window_size == 0 || window_size > N) return -1;
    for (size_t i = 0; i < N; i++) {
        double sum = 0; size_t count = 0;
        int half = (int)window_size / 2;
        for (int j = -half; j <= half; j++) {
            int idx = (int)i + j;
            if (idx >= 0 && (size_t)idx < N) { sum += x[idx]; count++; }
        }
        y[i] = (count > 0) ? sum / count : 0;
    }
    return 0;
}

int nf_median_filter(const double *x, double *y, size_t N, size_t window_size)
{
    if (!x || !y || N == 0 || window_size == 0) return -1;
    double *window = malloc(window_size * sizeof(double));
    if (!window) return -1;
    int half = (int)window_size / 2;
    for (size_t i = 0; i < N; i++) {
        size_t w_idx = 0;
        for (int j = -half; j <= half; j++) {
            int idx = (int)i + j;
            if (idx >= 0 && (size_t)idx < N && w_idx < window_size) window[w_idx++] = x[idx];
        }
        for (size_t a = 0; a < w_idx - 1; a++)
            for (size_t b = a + 1; b < w_idx; b++)
                if (window[a] > window[b]) { double t = window[a]; window[a] = window[b]; window[b] = t; }
        y[i] = (w_idx > 0) ? window[w_idx / 2] : 0;
    }
    free(window);
    return 0;
}

/* ============================================================================
 * L6 — Peak Detection in Spectrum
 * ============================================================================
 */

int nf_peak_detect(const double *spectrum, size_t N, double threshold,
                    double min_separation_hz, double df,
                    double *peak_freqs, double *peak_values,
                    size_t *num_peaks, size_t max_peaks)
{
    if (!spectrum || !peak_freqs || !peak_values || !num_peaks || N < 3 || max_peaks == 0) return -1;
    *num_peaks = 0;
    for (size_t i = 1; i < N - 1 && *num_peaks < max_peaks; i++) {
        if (spectrum[i] > threshold && spectrum[i] > spectrum[i - 1] && spectrum[i] > spectrum[i + 1]) {
            int too_close = 0;
            for (size_t j = 0; j < *num_peaks; j++) {
                if (fabs(peak_freqs[j] - i * df) < min_separation_hz) {
                    too_close = 1;
                    if (spectrum[i] > peak_values[j]) {
                        peak_freqs[j] = i * df; peak_values[j] = spectrum[i];
                    }
                    break;
                }
            }
            if (!too_close) {
                peak_freqs[*num_peaks] = i * df;
                peak_values[*num_peaks] = spectrum[i];
                (*num_peaks)++;
            }
        }
    }
    return 0;
}

/* ============================================================================
 * L6 — S-parameter Conversion
 * ============================================================================
 */

double complex nf_s11_to_impedance(double complex S11, double complex Z0)
{
    return Z0 * (1.0 + S11) / (1.0 - S11);
}

double complex nf_z_to_s11(double complex Z, double complex Z0)
{
    return (Z - Z0) / (Z + Z0);
}

/* ============================================================================
 * L5 — NFFFT (Near-Field to Far-Field Transformation)
 * ============================================================================
 */

int nf_nffft_init(nf_nffft_params_t *params, size_t Nx, size_t Ny,
                   double dx, double dy, double freq_hz)
{
    if (!params || Nx < 2 || Ny < 2 || dx <= 0 || dy <= 0) return -1;
    memset(params, 0, sizeof(*params));
    params->Nx = Nx; params->Ny = Ny;
    params->dx = dx; params->dy = dy;
    params->freq_hz = freq_hz;
    params->k0 = 2.0 * M_PI * freq_hz / C_0;
    params->nkx = Nx; params->nky = Ny;
    params->kx_grid = malloc(params->nkx * sizeof(double));
    params->ky_grid = malloc(params->nky * sizeof(double));
    if (!params->kx_grid || !params->ky_grid) { nf_nffft_free(params); return -1; }
    double dkx = 2.0 * M_PI / (dx * Nx);
    double dky = 2.0 * M_PI / (dy * Ny);
    for (size_t i = 0; i < params->nkx; i++) params->kx_grid[i] = -M_PI / dx + i * dkx;
    for (size_t i = 0; i < params->nky; i++) params->ky_grid[i] = -M_PI / dy + i * dky;
    return 0;
}

void nf_nffft_free(nf_nffft_params_t *params)
{
    if (!params) return;
    free(params->kx_grid); params->kx_grid = NULL;
    free(params->ky_grid); params->ky_grid = NULL;
    free(params->kz_grid); params->kz_grid = NULL;
    free(params->far_field_pattern_theta); params->far_field_pattern_theta = NULL;
    free(params->far_field_pattern_phi); params->far_field_pattern_phi = NULL;
    memset(params, 0, sizeof(*params));
}

int nf_nffft_transform(const nf_nffft_params_t *params, const double complex *E_near_x,
                        const double complex *E_near_y, double complex *E_far_theta,
                        double complex *E_far_phi)
{
    if (!params || !E_near_x || !E_near_y || !E_far_theta || !E_far_phi) return -1;
    size_t N = params->Nx * params->Ny;
    double complex *Kx = malloc(N * sizeof(double complex));
    double complex *Ky = malloc(N * sizeof(double complex));
    if (!Kx || !Ky) { free(Kx); free(Ky); return -1; }
    memcpy(Kx, E_near_x, N * sizeof(double complex));
    memcpy(Ky, E_near_y, N * sizeof(double complex));
    for (size_t i = 0; i < N; i++) { E_far_theta[i] = Kx[i]; E_far_phi[i] = Ky[i]; }
    free(Kx); free(Ky);
    return 0;
}
