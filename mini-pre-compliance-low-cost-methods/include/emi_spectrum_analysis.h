/**
 * emi_spectrum_analysis.h - Spectrum Analysis for Low-Cost EMC Pre-Compliance
 *
 * Spectrum analyzer configuration, detector models (peak, quasi-peak, average,
 * RMS), scan strategies (step, sweep, FFT-based), and signal identification.
 *
 * References:
 *   - CISPR 16-1-1: Specification for EMI measurement receivers
 *   - Agilent AN 150, "Spectrum Analysis Basics", Application Note
 *   - Rohde & Schwarz, "Fundamentals of Spectrum Analysis", 2014
 *   - Engelson, M., "Modern Spectrum Analyzer Theory and Applications", 1999
 *
 * Knowledge Coverage:
 *   L1: Detector types, RBW/VBW definitions, sweep parameters
 *   L2: Spectrum analyzer architecture, superheterodyne vs FFT
 *   L3: Resolution bandwidth / sweep time tradeoff, noise averaging
 *   L4: CISPR quasi-peak weighting function theory
 *   L5: Peak search algorithms, signal identification, ambient cancellation
 *   L6: Pre-compliance scan procedure (peak pre-scan, final QP measurement)
 */

#ifndef EMI_SPECTRUM_ANALYSIS_H
#define EMI_SPECTRUM_ANALYSIS_H

#include "emi_precompliance_core.h"
#include <stdint.h>

/* ─── Spectrum Analyzer Specification (L1, L2) ────────────────────────── */

/** Complete spectrum analyzer / EMI receiver specification.
 *  Models both traditional superheterodyne SA and modern FFT-based receivers.
 *  Low-cost USB SDR-based solutions (RTL-SDR, HackRF) can be modeled as
 *  spectrum analyzers with limited dynamic range and higher noise figure. */
typedef struct {
    double freq_min_hz;              /** Minimum frequency, Hz */
    double freq_max_hz;              /** Maximum frequency, Hz */
    double noise_figure_db;          /** Noise figure, dB */
    double danl_dbm_per_hz;          /** Displayed Average Noise Level, dBm/Hz */
    double max_safe_input_dbm;       /** Maximum safe input power, dBm */
    double rbw_min_hz;               /** Minimum resolution bandwidth, Hz */
    double rbw_max_hz;               /** Maximum resolution bandwidth, Hz */
    double vbw_min_hz;               /** Minimum video bandwidth, Hz */
    double vbw_max_hz;               /** Maximum video bandwidth, Hz */
    double phase_noise_dbc_hz_10k;   /** Phase noise at 10 kHz offset, dBc/Hz */
    double toi_dbm;                  /** Third-order intercept point, dBm */
    double ssfdr_db;                 /** Spurious-free dynamic range, dB */
    int    has_preamplifier;         /** Built-in preamplifier present */
    double preamp_gain_db;           /** Preamplifier gain, dB */
    int    has_tracking_generator;   /** Tracking generator present */
    double sweep_time_min_s;         /** Minimum sweep time, s */
    const char *receiver_type;       /** "superhet", "fft", "hybrid", "sdr" */
} emi_sa_spec_t;

/** RTL-SDR based low-cost spectrum analyzer model.
 *  Freq: ~500 kHz - 1.7 GHz, NF: ~20-30 dB, 8-bit ADC.
 *  DANL: ~-130 dBm/Hz (with LNA). No built-in tracking generator.
 *  Suitable for pre-compliance scans with reduced accuracy. */
void emi_sa_init_rtlsdr(emi_sa_spec_t *sa);

/** Entry-level benchtop SA (e.g., Rigol DSA815, Siglent SSA3021X).
 *  Freq: 9 kHz - 1.5/3.2 GHz, NF: ~15-20 dB.
 *  Has tracking generator, preamp option. */
void emi_sa_init_entry_benchtop(emi_sa_spec_t *sa);

/** Mid-range EMI receiver (e.g., R&S ESL, Keysight N9038A style).
 *  Freq: 20 Hz - 6/26 GHz, NF: ~10-15 dB, full CISPR detector suite.
 *  Includes built-in preamplifier and preselection filters. */
void emi_sa_init_emi_receiver(emi_sa_spec_t *sa);

/* ─── Detector Models (L3, L4) ───────────────────────────────────────── */

/**
 * Quasi-Peak Detector implementation per CISPR 16-1-1.
 *
 * The QP detector is an envelope-weighted detector with specific
 * charge and discharge time constants designed to correlate with
 * the subjective annoyance of impulsive interference to analog
 * broadcast reception (AM radio, TV).
 *
 * For CISPR Band B (150 kHz - 30 MHz):
 *   Charge time constant:   1 ms
 *   Discharge time constant: 550 ms
 *   Meter mechanical time constant: 160 ms
 *   IF bandwidth: 9 kHz
 *
 * For CISPR Band C/D (30 - 1000 MHz):
 *   Charge:   1 ms
 *   Discharge: 550 ms
 *   Meter: 100 ms
 *   IF BW: 120 kHz
 *
 * The QP response to a repetitive impulsive signal of amplitude A
 * and repetition rate f_prf is:
 *   V_qp = A * (1 - exp(-1/(f_prf * tau_charge)))
 *          / (1 - exp(-1/(f_prf * tau_discharge)))
 *
 * Ref: CISPR 16-1-1, Annex B; Engelson, Ch. 8.
 *
 * @param peak_amplitude    Peak amplitude of the impulsive signal
 * @param prf_hz            Pulse repetition frequency, Hz
 * @param tau_charge_s      Charge time constant, seconds
 * @param tau_discharge_s   Discharge time constant, seconds
 * @param tau_meter_s       Meter mechanical time constant, seconds
 * @return Quasi-peak detector response (normalized to peak = 1.0)
 */
double emi_qp_detector_response(double peak_amplitude, double prf_hz,
                                 double tau_charge_s, double tau_discharge_s,
                                 double tau_meter_s);

/**
 * Compute QP weighting factor relative to peak for a given PRF.
 * QP_weighting_dB = 20*log10(V_qp / V_peak)
 *
 * For high PRF (> 1 kHz), QP ≈ peak (0 dB difference).
 * For low PRF (< 10 Hz), QP can be 30-40 dB below peak.
 */
double emi_qp_weighting_db(double prf_hz, emi_standard_t standard);

/**
 * RMS-Average detector response.
 * For an impulsive signal: V_rms = V_peak * sqrt(PRF * tau_RBW)
 * where tau_RBW ≈ 1/RBW is the IF filter time constant.
 *
 * Ref: CISPR 16-1-1, Section 6.
 */
double emi_rms_detector_response(double peak_amplitude, double prf_hz,
                                  double rbw_hz);

/**
 * Average detector response (CISPR Average).
 * V_avg = V_peak * PRF / RBW  (for PRF << RBW)
 *
 * For continuous signals: V_avg = V_peak.
 */
double emi_average_detector_response(double peak_amplitude, double prf_hz,
                                      double rbw_hz);

/* ─── Scan Automation (L5 Algorithms) ────────────────────────────────── */

/**
 * Execute a peak pre-scan over the configured frequency range.
 *
 * Peak pre-scan strategy:
 *   1. Set max hold mode
 *   2. Sweep entire frequency range with peak detector
 *   3. Capture all peaks above (noise floor + threshold)
 *   4. Record peak frequencies and amplitudes for final measurement
 *
 * The pre-scan is typically much faster than final QP measurement
 * because peak detection doesn't require dwell time per frequency.
 *
 * @param config   Scan configuration
 * @param sa       Spectrum analyzer specification (for noise floor calc)
 * @param result   Output: scan result with all detected emission points
 * @return 0 on success, -1 on configuration error
 */
int emi_peak_prescan(const emi_scan_config_t *config,
                      const emi_sa_spec_t *sa,
                      emi_scan_result_t *result);

/**
 * Execute final quasi-peak measurement on identified peaks.
 *
 * For each peak identified in the pre-scan:
 *   1. Tune to peak frequency
 *   2. Set appropriate RBW (9 kHz < 30 MHz, 120 kHz > 30 MHz)
 *   3. Dwell for QP detector settling time (typ. 1 second)
 *   4. Record QP amplitude
 *
 * The dwell time must be sufficient for the QP detector to reach
 * steady state, typically 1-5 seconds per frequency.
 */
int emi_final_qp_measurement(const emi_scan_config_t *config,
                              emi_scan_result_t *prescan_result,
                              emi_scan_result_t *final_result);

/**
 * Find emission peaks in a scan result above a threshold.
 * Peak criterion: local maximum within +/- half_RBW frequency span
 * and amplitude > noise_floor + threshold_db.
 *
 * @param scan_data     Raw scan data
 * @param threshold_db  Threshold above noise floor, dB (typ. 6 dB)
 * @param peaks_out     Output array of peak indices (caller allocates)
 * @param max_peaks     Maximum number of peaks to find
 * @return Number of peaks found
 */
int emi_find_peaks(const emi_scan_result_t *scan_data,
                    double threshold_db,
                    int *peaks_out, int max_peaks);

/* ─── Signal Classification (L5, L6) ──────────────────────────────────── */

/** Emission signal classification. */
typedef enum {
    EMI_SIGNAL_NARROWBAND = 0,   /** Narrowband: clock harmonics, CW */
    EMI_SIGNAL_BROADBAND = 1,    /** Broadband: switching noise, arcs */
    EMI_SIGNAL_IMPULSIVE  = 2,   /** Impulsive: ESD, burst events */
    EMI_SIGNAL_AMBIENT    = 3    /** Ambient: broadcast, cellular */
} emi_signal_class_t;

/**
 * Classify an emission signal as narrowband or broadband.
 *
 * Criterion: if the signal amplitude changes < 3 dB when RBW is
 * changed by a factor of 10, it is narrowband. Otherwise, broadband.
 *
 * This is the standard two-RBW method per CISPR 16-2-3.
 *
 * @param amp_rbw1    Amplitude with RBW1
 * @param amp_rbw2    Amplitude with RBW2
 * @param rbw1_hz     First RBW, Hz
 * @param rbw2_hz     Second RBW, Hz (must differ by factor >= 3)
 * @return Signal classification
 */
emi_signal_class_t emi_classify_nb_bb(double amp_rbw1, double amp_rbw2,
                                       double rbw1_hz, double rbw2_hz);

/**
 * Ambient noise cancellation — subtract ambient from DUT measurement.
 *
 * Procedure:
 *   1. Measure ambient (DUT off) → ambient_scan
 *   2. Measure DUT on → dut_scan
 *   3. For each frequency, if dut > ambient + threshold, keep dut
 *      else discard (ambient-dominated)
 *   4. Output cleaned scan
 *
 * Note: simple subtraction in dB is not correct; must convert to
 * linear (voltage), subtract, and convert back to dB.
 *
 * @param dut_scan       Scan with DUT on
 * @param ambient_scan   Scan with DUT off (ambient only)
 * @param threshold_db   Minimum DUT-above-ambient to keep, dB (typ. 6)
 * @param cleaned_out    Output: ambient-subtracted scan
 * @return Number of valid DUT emission points found
 */
int emi_ambient_cancellation(const emi_scan_result_t *dut_scan,
                              const emi_scan_result_t *ambient_scan,
                              double threshold_db,
                              emi_scan_result_t *cleaned_out);

/**
 * Compute the signal-to-ambient ratio across a scan.
 * SAR(f) = DUT_amplitude(f) - Ambient_amplitude(f)
 *
 * @param sar_out   Output array of SAR values in dB (caller allocates)
 */
void emi_signal_ambient_ratio(const emi_scan_result_t *dut_scan,
                               const emi_scan_result_t *ambient_scan,
                               double *sar_out);

/* ─── Resolution Bandwidth Optimization (L3, L5) ──────────────────────── */

/**
 * Optimize RBW for a given span and sweep time constraint.
 * RBW_opt = sqrt(Span / (SweepTime * k))
 *
 * Smaller RBW gives better frequency resolution but longer sweep time.
 * Larger RBW gives faster sweeps but may merge adjacent signals.
 */
double emi_optimize_rbw(double span_hz, double max_sweep_time_s, double k_factor);

/**
 * Compute the noise floor improvement due to video bandwidth reduction.
 * Noise reduction = 10*log10(VBW_old / VBW_new)
 *
 * Reducing VBW by factor 10 reduces displayed noise by 10 dB
 * but increases sweep time proportionally.
 */
static inline double emi_vbw_noise_reduction_db(double vbw_old_hz,
                                                 double vbw_new_hz) {
    if (vbw_new_hz <= 0.0 || vbw_old_hz <= vbw_new_hz) return 0.0;
    return 10.0 * log10(vbw_old_hz / vbw_new_hz);
}

/**
 * Compute trace averaging noise reduction.
 * N averages reduce noise by 10*log10(N) dB (for power averaging).
 * For voltage averaging (default on most SAs): 5.0*log10(N) dB reduction.
 */
static inline double emi_averaging_noise_reduction_db(int num_averages,
                                                       int is_power_averaging) {
    if (num_averages <= 1) return 0.0;
    double factor = is_power_averaging ? 10.0 : 5.0;
    return factor * log10((double)num_averages);
}

#endif /* EMI_SPECTRUM_ANALYSIS_H */
