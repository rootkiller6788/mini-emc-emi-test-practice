/**
 * surge_burst.h - EFT/Burst Immunity Analysis
 *
 * L6 Canonical Problems: EFT burst generation, coupling clamp
 * modeling, filter design for EFT suppression, statistical
 * burst analysis.
 *
 * EFT (Electrical Fast Transient) per IEC 61000-4-4 is characterized
 * by repetitive high-voltage, short-duration pulses that simulate
 * switching transients from inductive loads (relays, motors).
 *
 * Reference:
 *   IEC 61000-4-4: EFT/Burst Immunity Test
 *   K. Hall, "EFT Testing Demystified", In Compliance Magazine, 2014
 *   W. Rhoades, "Avoiding EFT Test Failures", ITEM, 2018
 */

#ifndef SURGE_BURST_H
#define SURGE_BURST_H

#include "surge_defs.h"
#include "surge_waveform.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================================================================
 * L6: EFT Burst Pattern Generation
 * ================================================================== */

/**
 * Generate an individual EFT pulse per IEC 61000-4-4.
 *
 * The EFT pulse is defined as a double-exponential with:
 *   Rise time: 5 ns +/- 30% (10%-90%)
 *   Pulse width: 50 ns +/- 30% (50%-50%)
 *   tau1 = 1.70 ns, tau2 = 68.0 ns
 *
 * This function generates one pulse at time offset t0.
 *
 * Complexity: O(1)
 */
double eft_pulse(double t, double t0, double v_peak);

/**
 * Generate a complete EFT burst envelope with multiple pulses.
 *
 * A burst is a train of individual pulses:
 *   - Pulse spacing: 1/f_rep (e.g., 200 us for 5 kHz)
 *   - Burst duration: 15 ms (standard) or 0.75 ms (optional)
 *   - Burst repeats every 300 ms
 *
 * The envelope follows a trapezoidal shape:
 *   - Rise to full amplitude over first 1-2 pulses
 *   - Maintain full amplitude for burst duration
 *   - No explicit fall time (pulses simply stop)
 *
 * Complexity: O(num_pulses)
 */
int eft_burst_generate(double *pulse_times, int *num_pulses,
                        const eft_burst_params_t *burst);

/**
 * Compute the statistical distribution of EFT pulse amplitudes
 * within a burst.
 *
 * In practice, individual EFT pulses within a burst have some
 * amplitude variation due to generator limitations. The standard
 * allows +/-10% variation.
 *
 * This function models the amplitude as a truncated Gaussian
 * distribution around the nominal V_peak.
 *
 * Complexity: O(num_pulses)
 */
int eft_burst_amplitude_distribution(double *amplitudes,
                                      int num_pulses,
                                      double v_peak_nominal,
                                      double std_dev_pct);

/* ==================================================================
 * L6: EFT Coupling Clamp Model
 * ================================================================== */

/**
 * Model the capacitive coupling clamp used for EFT injection.
 *
 * The IEC 61000-4-4 coupling clamp consists of:
 *   - A metal clamp around the cable under test
 *   - Capacitive coupling plates (typically 100-200 pF per cm)
 *   - Total coupling capacitance: 33 nF (standard)
 *   - Source impedance: 50 ohm
 *
 * The coupling clamp acts as a high-pass filter:
 *   fc = 1/(2*pi*50*33e-9) ~ 96 kHz
 * The EFT spectrum (~100 kHz-100 MHz) is well above fc.
 *
 * Transfer function: V_EUT / V_generator = 1 / sqrt(1 + (fc/f)^2)
 *
 * Complexity: O(1)
 */
double eft_coupling_clamp_response(double f, double clamp_capacitance_pf,
                                    double z_source);

/**
 * Compute the peak voltage delivered to the EUT through the
 * coupling clamp.
 *
 * The coupling clamp forms a capacitive voltage divider with
 * the EUT input impedance. For high-impedance EUT inputs,
 * nearly the full generator voltage appears across the input.
 *
 * Complexity: O(1)
 */
double eft_delivered_voltage(double v_generator, double z_eut,
                              double clamp_capacitance_pf,
                              double pulse_rise_ns);

/* ==================================================================
 * L6: EFT Filter Design
 * ================================================================== */

/**
 * Design a low-pass filter for EFT suppression.
 *
 * EFT pulses have significant frequency content up to ~100 MHz.
 * A low-pass filter with fc < 100 kHz can provide substantial
 * attenuation of EFT energy while passing power/signal frequencies.
 *
 * Filter topologies for EFT suppression:
 *   1. Single-stage LC: -40 dB/decade, simple, effective
 *   2. Pi-filter (C-L-C): -60 dB/decade, better HF performance
 *   3. T-filter (L-C-L): -60 dB/decade, better impedance matching
 *   4. Common-mode choke: Differential mode only
 *
 * The EFT filter must also consider:
 *   - Ferrite saturation at high current
 *   - Parasitic capacitance across inductors
 *   - Voltage rating of capacitors (must withstand EFT peaks)
 *   - Safety (Y-caps for line-to-ground)
 *
 * Complexity: O(1)
 */
typedef enum {
    EFT_FILTER_LC      = 0,  /* Single-stage LC low-pass */
    EFT_FILTER_PI      = 1,  /* Pi-section (C-L-C) */
    EFT_FILTER_T       = 2,  /* T-section (L-C-L) */
    EFT_FILTER_CM_CHOKE = 3, /* Common-mode choke + caps */
    EFT_FILTER_FERRITE = 4   /* Ferrite bead on cable */
} eft_filter_topology_t;

typedef struct {
    eft_filter_topology_t topology;
    double cutoff_freq_hz;          /* -3dB cutoff frequency (Hz) */
    double inductance_uh;           /* Series inductance (uH) */
    double capacitance_input_pf;    /* Input shunt capacitance (pF) */
    double capacitance_output_pf;   /* Output shunt capacitance (pF) */
    double attenuation_at_100mhz_db; /* Predicted attenuation at 100 MHz */
    double cmrr_db;                 /* Common-mode rejection ratio (dB) */
    int    max_current_a;           /* Maximum DC current rating (A) */
    double voltage_rating_v;        /* Capacitor voltage rating (V) */
} eft_filter_params_t;

/**
 * Design EFT filter parameters based on requirements.
 *
 * Input: desired cutoff frequency, topology, max current
 * Output: complete filter component values
 *
 * The filter is designed so that the EFT spectrum is attenuated
 * by at least 30 dB (factor of ~30 voltage reduction).
 *
 * Complexity: O(1)
 */
int eft_filter_design(eft_filter_params_t *filter,
                      eft_filter_topology_t topology,
                      double fc_hz, int max_current_a,
                      double voltage_rating_v);

/**
 * Compute the EFT attenuation provided by a filter.
 *
 * Attenuation = 20 * log10(|V_out|/|V_in|)
 *
 * For an LC filter:
 *   |H(f)| = 1 / sqrt((1 - (2*pi*f)^2*L*C)^2 + (2*pi*f*L/R_load)^2)
 *
 * Complexity: O(1)
 */
double eft_filter_attenuation(const eft_filter_params_t *filter,
                               double frequency_hz, double r_load);

/**
 * Compute the optimal corner frequency for EFT filter design.
 *
 * The filter must balance:
 *   - Lower fc: better EFT attenuation
 *   - Higher fc: less impact on signal integrity
 *
 * For power lines (DC/50-60Hz): fc = 1-10 kHz
 * For signal lines (e.g., RS-232): fc = 10-100 kHz
 * For high-speed data (e.g., Ethernet): fc >= 100 MHz (use CM choke)
 *
 * Complexity: O(1)
 */
double eft_optimal_cutoff(double signal_bandwidth_hz,
                           double eft_peak_freq_hz,
                           double min_attenuation_db);

/* ==================================================================
 * L6: Statistical Burst Analysis
 * ================================================================== */

/**
 * Compute the cumulative energy deposited by an EFT burst.
 *
 *   E_total = sum_{i=1}^{N} E_pulse_i
 *
 * where E_pulse_i is the energy of each individual pulse.
 * For a standard burst: 75 pulses * ~1 mJ/pulse = ~75 mJ total.
 *
 * While individual pulse energy is low, the cumulative effect
 * can cause thermal buildup in protection devices.
 *
 * Complexity: O(num_pulses)
 */
double eft_burst_total_energy(const eft_burst_params_t *burst,
                               double v_peak_per_pulse,
                               double load_impedance);

/**
 * Compute the effective repetition rate for thermal analysis.
 *
 * During a burst: pulses arrive at f_rep (5 kHz or 100 kHz)
 * Between bursts: 300 ms silence
 * Effective average rate = pulses_per_burst / burst_period
 *   = 75 / 0.3 = 250 pulses/second average
 *
 * This determines if the protection device has time to cool
 * between bursts.
 *
 * Complexity: O(1)
 */
double eft_effective_pulse_rate(const eft_burst_params_t *burst);

/**
 * Evaluate worst-case EFT scenario for equipment testing.
 *
 * Considers:
 *   - Maximum pulse amplitude
 *   - Minimum pulse spacing (worst-case thermal)
 *   - Coupling clamp efficiency
 *   - Cable resonance effects
 *
 * Returns the worst-case stress on the EUT.
 *
 * Complexity: O(1)
 */
int eft_worst_case_stress(const eft_burst_params_t *burst,
                          double *worst_v_peak,
                          double *worst_dv_dt,
                          double *worst_energy_per_burst);

#ifdef __cplusplus
}
#endif

#endif /* SURGE_BURST_H */