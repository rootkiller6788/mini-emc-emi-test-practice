/**
 * surge_waveform.h - Surge/EFT Waveform Generation and Analysis
 *
 * L3 Mathematical Structures: Double-exponential waveform model,
 * Heidler function, Fourier spectrum analysis, energy computation.
 * L4 Fundamental Laws: IEC waveform parameter constraints,
 * energy-time integral, spectral energy distribution.
 *
 * Reference:
 *   IEC 61000-4-5: Surge Immunity Test (waveform definitions)
 *   IEC 61000-4-4: EFT/Burst Immunity Test
 *   IEEE C62.41-1991: Guide on Surge Voltages
 *   Heidler, "Analytische Blitzstromfunktion," ETZ, 1985
 */

#ifndef SURGE_WAVEFORM_H
#define SURGE_WAVEFORM_H

#include "surge_defs.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================================================================
 * L3: Waveform Generation Functions
 * ================================================================== */

/**
 * Compute double-exponential waveform value at time t.
 *
 * The double-exponential function models standard surge pulses:
 *   v(t) = Vp * k * (exp(-t/tau2) - exp(-t/tau1))
 * where k is a normalization factor ensuring v(t_peak) = Vp.
 *
 * For the IEC 1.2/50us combination wave:
 *   tau1 = 0.4074 us, tau2 = 68.22 us
 *   Peak occurs at t_peak = tau1*tau2*ln(tau2/tau1)/(tau2-tau1)
 *
 * This is the standard model per IEEE C62.41 and IEC 61000-4-5.
 *
 * Complexity: O(1)
 * Reference: Paul (2006), Ch. 8
 */
double surge_double_exponential(double t, double vp, double tau1, double tau2);

/**
 * Compute Heidler function value at time t (for lightning current).
 *
 * The Heidler function provides a more physically accurate lightning
 * current waveform with zero derivative at t=0:
 *   i(t) = (Ip / eta) * (t/tau1)^n / (1 + (t/tau1)^n) * exp(-t/tau2)
 *
 * This corrects a limitation of the double-exponential model which
 * has non-zero derivative at t=0 (physically unrealistic).
 *
 * Parameters:
 *   Ip:    peak current (kA)
 *   tau1:  front time constant (us)
 *   tau2:  decay time constant (us)
 *   n:     steepness factor (typically 2-10, n=10 for first stroke)
 *   eta:   amplitude correction factor
 *
 * Complexity: O(1)
 * Reference: Heidler, ETZ vol. 106, 1985
 */
double surge_heidler_function(double t, double Ip, double tau1,
                              double tau2, double n);

/**
 * Compute the amplitude correction factor eta for Heidler function.
 *
 *   eta = exp(-(tau1/tau2) * (n*tau2/tau1)^(1/n))
 *
 * Complexity: O(1)
 */
double surge_heidler_eta(double tau1, double tau2, double n);

/**
 * Find the time of peak voltage for a double-exponential waveform.
 *
 *   t_peak = tau1 * tau2 * ln(tau2/tau1) / (tau2 - tau1)
 *
 * This is derived from dV/dt = 0 of v(t) = Vp*k*(e^(-t/tau2) - e^(-t/tau1))
 *
 * Complexity: O(1)
 */
double surge_peak_time(double tau1, double tau2);

/**
 * Compute normalization factor k for double-exponential.
 *
 *   k = 1 / (exp(-t_peak/tau2) - exp(-t_peak/tau1))
 *
 * Ensures the waveform peaks at exactly Vp.
 *
 * Complexity: O(1)
 */
double surge_normalization_factor(double tau1, double tau2);

/**
 * Compute the 10%-90% rise time from waveform parameters.
 *
 * For double-exponential waveforms, the rise time is approximately:
 *   t_rise = 2.2 * tau1  (for tau2 >> tau1)
 *
 * For EFT: tau1 = 1.70 ns -> t_rise ~ 3.74 ns, actual spec is 5 ns
 * For 1.2/50us: tau1 = 0.4074 us -> t_rise ~ 0.896 us, actual spec is 1.2 us
 *
 * This function computes the rise time numerically from 10% and 90%
 * crossing points.
 *
 * Complexity: O(log(1/eps)) using binary search
 */
double surge_rise_time_10_90(double tau1, double tau2, double eps);

/**
 * Compute the full-width at half-maximum (FWHM) pulse width.
 *
 * Finds the time interval [t1, t2] where v(t) = v_peak/2,
 * and returns t2 - t1.
 *
 * For 1.2/50us waveform, this should return ~50 us.
 * For EFT (5/50ns), this should return ~50 ns.
 *
 * Complexity: O(log(1/eps)) using binary search
 */
double surge_pulse_width_fwhm(double tau1, double tau2, double eps);

/* ==================================================================
 * L3: Waveform Parameter Generation
 * ================================================================== */

/**
 * Initialize a surge waveform params struct from standard type.
 *
 * Fills in all fields based on the standardized waveform type,
 * including the double-exponential time constants.
 *
 * Complexity: O(1)
 */
int surge_waveform_init(surge_waveform_params_t *wf,
                        surge_waveform_type_t type,
                        double v_peak,
                        double source_impedance);

/**
 * Initialize EFT burst parameters from standard test level.
 *
 * Sets all EFT burst characteristics per IEC 61000-4-4
 * for the specified severity level.
 *
 * Complexity: O(1)
 */
int eft_burst_init(eft_burst_params_t *burst, eft_test_level_t level);

/**
 * Compute the number of pulses per burst.
 *
 *   n_pulses = burst_duration * repetition_rate
 *
 * At 5 kHz rep rate with 15 ms burst: n = 75 pulses
 * At 100 kHz rep rate with 0.75 ms burst: n = 75 pulses
 *
 * Complexity: O(1)
 */
int eft_pulses_per_burst(const eft_burst_params_t *burst);

/* ==================================================================
 * L3: Fourier Spectrum Analysis
 * ================================================================== */

/**
 * Compute the Fourier transform magnitude of a double-exponential pulse
 * at a given frequency.
 *
 *   |V(f)| = Vp * k * |tau2/(1+j*2*pi*f*tau2) - tau1/(1+j*2*pi*f*tau1)|
 *
 * This allows frequency-domain analysis of surge energy distribution
 * and interaction with circuit impedances.
 *
 * Complexity: O(1)
 */
double surge_spectrum_magnitude(double f, double vp, double tau1, double tau2);

/**
 * Compute the -3dB corner frequency of a surge waveform spectrum.
 *
 * The corner frequency indicates the highest frequency component
 * with significant energy. For the 1.2/50us surge, fc ~ 10 kHz.
 * For the 5/50ns EFT pulse, fc ~ 10 MHz.
 *
 * This is critical for determining the required bandwidth of
 * protection devices and coupling networks.
 *
 * Complexity: O(log(f_max/f_min)) using binary search
 */
double surge_corner_frequency(double tau1, double tau2, double vp);

/**
 * Compute the spectral energy density at frequency f.
 *
 *   S(f) = |V(f)|^2 / (2 * Z0)  where Z0 = source impedance
 *
 * This gives the power spectral density in W/Hz.
 *
 * Complexity: O(1)
 */
double surge_spectral_energy_density(double f, double vp, double tau1,
                                      double tau2, double z0);

/* ==================================================================
 * L4: Energy Computation Theorems
 * ================================================================== */

/**
 * Compute total energy of a surge pulse into a resistive load.
 *
 *   E = integral_0^inf [v(t)^2 / R] dt
 *
 * For the double-exponential model, the integral has a closed form:
 *   E = Vp^2 * k^2 / R * [tau2/2 + tau1/2 - 2*tau1*tau2/(tau1+tau2)]
 *
 * This is the basis for all surge protection energy rating calculations.
 *
 * Complexity: O(1)
 * Reference: van der Laan & van Deursen, "Energy in Surge Waveforms", 1998
 */
double surge_energy_resistive(double vp, double tau1, double tau2,
                               double resistance);

/**
 * Compute the I^2*t (Joule integral) for a surge current waveform.
 *
 *   I^2*t = integral_0^inf [i(t)^2] dt
 *
 * This parameter is critical for fuse selection and device survivability.
 * MOV and TVS datasheets specify maximum I^2*t values.
 *
 * For double-exponential current:
 *   I^2*t = Ip^2 * k^2 * [tau2/2 + tau1/2 - 2*tau1*tau2/(tau1+tau2)]
 *
 * Complexity: O(1)
 */
double surge_i2t(double ip, double tau1, double tau2);

/**
 * Compute the charge delivered by a surge current waveform.
 *
 *   Q = integral_0^inf [i(t)] dt = Ip * k * (tau2 - tau1)
 *
 * This determines how much charge a protection device must conduct.
 * GDT and TSS devices have finite charge transfer capability.
 *
 * Complexity: O(1)
 */
double surge_charge_transfer(double ip, double tau1, double tau2);

/**
 * Generate a damped sinusoidal (ring wave) waveform value.
 *
 * Per IEEE C62.41, the ring wave is defined as:
 *   v(t) = Vp * (1 - exp(-t/tau_r)) * exp(-t/tau_d) * sin(2*pi*f*t)
 * where f = 100 kHz, tau_r ~ 0.5 us, tau_d ~ 10 us
 *
 * This models oscillatory transients from switching operations.
 *
 * Complexity: O(1)
 */
double surge_ring_wave(double t, double vp, double frequency,
                       double tau_rise, double tau_decay);

/**
 * Generate a complete surge waveform time series.
 *
 * Fills an array with v(t) samples at uniform time steps from t=0
 * to t=t_max. This is used for simulation and visualization.
 *
 * Complexity: O(n_points)
 */
int surge_waveform_series(double *v_out, int n_points,
                          double t_max, double vp,
                          double tau1, double tau2);

/**
 * Generate a complete EFT burst pattern time series.
 *
 * Produces a sequence of EFT pulses at the specified repetition rate
 * over the burst duration. Each individual pulse follows the 5/50ns
 * double-exponential model.
 *
 * Complexity: O(n_points * pulses_per_burst)
 */
int eft_burst_series(double *v_out, int n_points,
                      const eft_burst_params_t *burst,
                      double sample_period_ns);

#ifdef __cplusplus
}
#endif

#endif /* SURGE_WAVEFORM_H */