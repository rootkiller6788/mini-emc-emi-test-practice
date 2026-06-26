#ifndef NF_SIGNAL_PROCESSING_H
#define NF_SIGNAL_PROCESSING_H
/**
 * @file nf_signal_processing.h
 * @brief Near-Field Signal Processing (L5-L6)
 *
 * L5: Signal processing algorithms — FFT-based NFFFT, digital filtering,
 *     time-gating, correlation analysis, phase unwrapping
 * L6: Signal processing canonical problems — EMI receiver emulation,
 *     noise floor analysis, spurious detection
 *
 * Ref: Alan V. Oppenheim & Ronald W. Schafer, "Discrete-Time Signal Processing", 3rd ed, 2010
 *      IEEE Std 145-2013, "Definitions of Terms for Antennas"
 *      Thomas H. Lee, "Planar Microwave Engineering", 2004
 */

#include <complex.h>
#include <stddef.h>
#include "nf_probe_core.h"
#include "nf_field_theory.h"

/* ============================================================================
 * L5 — Digital Signal Processing for Near-Field
 * ============================================================================
 */

/* ---------- FFT Configuration ---------- */

typedef struct {
    size_t N;
    int    inverse;
    double complex *twiddle_factors;
    double complex *buffer;
} nf_fft_plan_t;

/* ---------- Time-Domain Gating ---------- */

typedef struct {
    double gate_start_s;
    double gate_stop_s;
    double gate_center_s;
    double gate_span_s;
    int    window_type;
    double *window_coeffs;
    size_t window_length;
} nf_time_gate_t;

/* ---------- Frequency-Domain Filter ---------- */

typedef enum {
    NF_FILTER_LOWPASS = 0,
    NF_FILTER_HIGHPASS = 1,
    NF_FILTER_BANDPASS = 2,
    NF_FILTER_BANDSTOP = 3
} nf_filter_type_t;

typedef enum {
    NF_WINDOW_RECTANGULAR = 0,
    NF_WINDOW_HANN = 1,
    NF_WINDOW_HAMMING = 2,
    NF_WINDOW_BLACKMAN = 3,
    NF_WINDOW_KAISER = 4,
    NF_WINDOW_FLATTOP = 5
} nf_window_type_t;

typedef struct {
    nf_filter_type_t type;
    double f_low_hz;
    double f_high_hz;
    size_t order;
    double *b_coeffs;
    double *a_coeffs;
    size_t num_b;
    size_t num_a;
} nf_iir_filter_t;

/* ---------- Correlation Analysis ---------- */

typedef struct {
    double *cross_correlation;
    double *auto_correlation_ref;
    double *auto_correlation_meas;
    size_t length;
    double max_correlation;
    size_t lag_at_max;
    double coherence_at_peak;
} nf_correlation_result_t;

/* ---------- Phase Unwrapping ---------- */

typedef struct {
    double *unwrapped_phase;
    size_t num_points;
    double total_phase_shift;
    int    discontinuities_found;
    size_t num_discontinuities;
} nf_phase_unwrap_t;

/* ---------- NFFFT Parameters (L5) ---------- */

typedef struct {
    size_t Nx, Ny;
    double dx, dy;
    double freq_hz;
    double k0;
    double *kx_grid;
    double *ky_grid;
    double *kz_grid;
    size_t nkx, nky;
    double *far_field_pattern_theta;
    double *far_field_pattern_phi;
    size_t ntheta, nphi;
    int    probe_corrected;
} nf_nffft_params_t;

/* ---------- EMI Receiver Model (L5) ---------- */

typedef struct {
    double freq_hz;
    double rbw_hz;
    double vbw_hz;
    int    detector;
    double charge_time_const_s;
    double discharge_time_const_s;
    double meter_time_const_s;
    double quasi_peak_output;
    double peak_output;
    double average_output;
    double rms_output;
} nf_emi_receiver_model_t;

/* ---------- Noise Analysis (L5) ---------- */

typedef struct {
    double noise_floor_dbm;
    double noise_figure_db;
    double snr_db;
    double signal_power_dbm;
    double noise_power_dbm;
    double minimum_detectable_signal_dbm;
    double dynamic_range_db;
} nf_noise_analysis_t;

/* ---------- Statistical Field Analysis (L5) ---------- */

typedef struct {
    double mean_value;
    double median_value;
    double std_deviation;
    double skewness;
    double kurtosis;
    double percentile_95;
    double percentile_99;
    size_t num_samples;
} nf_field_statistics_t;

/* ---------- Coherence Analysis (L5) ---------- */

typedef struct {
    double *magnitude_squared_coherence;
    double *phase_coherence;
    size_t num_freq_bins;
    double df_hz;
    double mean_coherence;
    double *freqs;
} nf_coherence_t;

/* ============================================================================
 * API — Signal Processing Functions
 * ============================================================================
 */

/* FFT operations */
int  nf_fft_plan_create(nf_fft_plan_t *plan, size_t N, int inverse);
void nf_fft_plan_destroy(nf_fft_plan_t *plan);
int  nf_fft_execute(nf_fft_plan_t *plan, double complex *data);
int  nf_fft_real(const double *real_data, size_t N,
                  double complex *spectrum);

/* Windowing */
int  nf_window_generate(double *window, size_t N, nf_window_type_t type,
                         double param);
int  nf_window_apply(double *data, const double *window, size_t N);
int  nf_window_apply_complex(double complex *data, const double *window,
                              size_t N);

/* IIR filter design (Bilinear transform from analog prototype) */
int  nf_iir_butterworth_design(nf_iir_filter_t *filter,
                                nf_filter_type_t type,
                                double f_low, double f_high,
                                double fs, size_t order);
int  nf_iir_filter_apply(const nf_iir_filter_t *filter,
                          const double *input, double *output, size_t N);
void nf_iir_filter_free(nf_iir_filter_t *filter);

/* Time-domain gating */
int  nf_time_gate_apply(const nf_time_gate_t *gate,
                         const double complex *freq_data,
                         double complex *gated_data, size_t N,
                         double df);

/* Correlation analysis */
int  nf_correlation_analyze(const double *x, const double *y, size_t N,
                             nf_correlation_result_t *result);
int  nf_correlation_analyze_complex(const double complex *x,
                                     const double complex *y, size_t N,
                                     nf_correlation_result_t *result);
void nf_correlation_result_free(nf_correlation_result_t *result);

/* Phase unwrapping (Itoh algorithm) */
int  nf_phase_unwrap(const double *wrapped_phase, size_t N,
                      nf_phase_unwrap_t *result);
void nf_phase_unwrap_free(nf_phase_unwrap_t *result);

/* NFFFT — Near-Field to Far-Field Transformation */
int  nf_nffft_init(nf_nffft_params_t *params,
                    size_t Nx, size_t Ny, double dx, double dy,
                    double freq_hz);
void nf_nffft_free(nf_nffft_params_t *params);
int  nf_nffft_transform(const nf_nffft_params_t *params,
                         const double complex *E_near_x,
                         const double complex *E_near_y,
                         double complex *E_far_theta,
                         double complex *E_far_phi);

/* EMI receiver simulation */
int  nf_emi_receiver_process(nf_emi_receiver_model_t *rx,
                              const double *time_signal, size_t N,
                              double fs);

/* Noise analysis */
int  nf_noise_analyze(const double *signal, size_t N,
                       double rbw_hz, double gain_db, double nf_db,
                       nf_noise_analysis_t *result);

/* Field statistics */
int  nf_field_statistics_compute(const double *field_magnitudes,
                                  size_t N,
                                  nf_field_statistics_t *stats);

/* Coherence estimation (Welch method) */
int  nf_coherence_estimate(const double *x, const double *y, size_t N,
                            double fs, size_t nfft, size_t noverlap,
                            nf_coherence_t *coh);
void nf_coherence_free(nf_coherence_t *coh);

/* Power spectral density (Welch periodogram) */
int  nf_psd_welch(const double *x, size_t N, double fs,
                   size_t nfft, size_t noverlap,
                   nf_window_type_t window_type,
                   double *freqs, double *psd, size_t *num_bins);

/* Moving average filter */
int  nf_moving_average(const double *x, double *y, size_t N,
                        size_t window_size);

/* Median filter for impulsive noise removal */
int  nf_median_filter(const double *x, double *y, size_t N,
                       size_t window_size);

/* Peak detection in spectrum */
int  nf_peak_detect(const double *spectrum, size_t N,
                     double threshold, double min_separation_hz,
                     double df,
                     double *peak_freqs, double *peak_values,
                     size_t *num_peaks, size_t max_peaks);

/* S-parameter to impedance conversion */
double complex nf_s11_to_impedance(double complex S11, double complex Z0);
double complex nf_z_to_s11(double complex Z, double complex Z0);

#endif /* NF_SIGNAL_PROCESSING_H */
