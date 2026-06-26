/**
 * surge_waveform.c - Surge/EFT Waveform Generation Implementation
 *
 * L3 Mathematical Structures: Double-exponential model, Heidler function,
 * Fourier analysis, energy integrals.
 * L4 Fundamental Laws: IEC waveform constraints, spectral energy.
 *
 * Reference:
 *   IEC 61000-4-5, IEC 61000-4-4, IEEE C62.41
 *   Heidler, "Analytische Blitzstromfunktion," ETZ 1985
 *   Paul (2006) Introduction to EMC, Ch. 8
 */

#include "surge_waveform.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ==================================================================
 * L3: Double-Exponential Waveform (Standard Surge Model)
 *
 * Mathematical foundation:
 *   v(t) = Vp * k * (e^(-t/tau2) - e^(-t/tau1))
 *
 * This is the solution to the two-pole circuit:
 *   Thevenin source -> R_generator -> (L_series, C_shunt) -> load
 *
 * The waveform shape is determined by the ratio tau2/tau1.
 * For standard surge waveforms:
 *   1.2/50us: tau2/tau1 ~ 167  (heavily damped)
 *   8/20us:   tau2/tau1 ~ 9.05 (moderately damped)
 *   5/50ns:   tau2/tau1 ~ 40   (EFT)
 * ================================================================== */

double surge_double_exponential(double t, double vp, double tau1, double tau2)
{
    /* Guard against invalid parameters */
    if (tau1 <= 0.0 || tau2 <= 0.0 || tau2 <= tau1) return 0.0;
    if (t < 0.0) return 0.0;

    double k = surge_normalization_factor(tau1, tau2);
    if (k <= 0.0) return 0.0;

    double exp1 = exp(-t / tau2);
    double exp2 = exp(-t / tau1);

    /* v(t) = Vp * k * (e^(-t/tau2) - e^(-t/tau1)) */
    /* Note: exp1 > exp2 for tau2 > tau1, so v(t) is positive */
    return vp * k * (exp1 - exp2);
}

/* ==================================================================
 * L3: Heidler Function (Lightning Current Model)
 *
 * The Heidler function corrects the physically unrealistic non-zero
 * derivative at t=0 of the double-exponential model:
 *
 *   i(t) = (I_peak / eta) * (t/tau1)^n / (1 + (t/tau1)^n) * e^(-t/tau2)
 *
 * Properties:
 *   i(0) = 0        (zero current at t=0)
 *   di/dt(0) = 0    (zero derivative at t=0, physically correct)
 *   i(t_peak) ~ I_peak  (after eta correction)
 *
 * n controls the steepness of the rising edge:
 *   n=2:  moderate steepness (subsequent lightning strokes)
 *   n=10: high steepness (first lightning stroke)
 * ================================================================== */

double surge_heidler_function(double t, double Ip, double tau1,
                               double tau2, double n)
{
    if (tau1 <= 0.0 || tau2 <= 0.0 || n < 1.0) return 0.0;
    if (t < 0.0) return 0.0;

    double eta = surge_heidler_eta(tau1, tau2, n);
    if (eta <= 0.0) return 0.0;

    /* Avoid overflow for large t/tau1 ratios when n is large */
    double ratio = t / tau1;
    double ratio_n;

    if (ratio > 100.0 && n > 5.0) {
        /* For large t/tau1, (t/tau1)^n dominates and term ~ 1 */
        return (Ip / eta) * exp(-t / tau2);
    }

    ratio_n = pow(ratio, n);
    double front_factor = ratio_n / (1.0 + ratio_n);

    return (Ip / eta) * front_factor * exp(-t / tau2);
}

double surge_heidler_eta(double tau1, double tau2, double n)
{
    if (tau1 <= 0.0 || tau2 <= 0.0 || n < 1.0) return 0.0;

    /* eta = exp(-(tau1/tau2) * (n*tau2/tau1)^(1/n)) */
    double inner = n * tau2 / tau1;

    /* Guard against overflow for inner */
    if (inner <= 0.0) return 0.0;

    double inner_pow = pow(inner, 1.0 / n);
    double exponent = -(tau1 / tau2) * inner_pow;

    /* Guard against extreme exponents */
    if (exponent < -50.0) return 0.0;
    if (exponent > 50.0) return exp(50.0);

    return exp(exponent);
}

/* ==================================================================
 * L3: Waveform Parameter Computation
 * ================================================================== */

double surge_peak_time(double tau1, double tau2)
{
    /* t_peak = tau1 * tau2 * ln(tau2/tau1) / (tau2 - tau1)
     *
     * Derivation:
     *   dv/dt = Vp*k*[(-1/tau2)*e^(-t/tau2) + (1/tau1)*e^(-t/tau1)]
     *   Set dv/dt = 0:
     *   (1/tau1)e^(-t/tau1) = (1/tau2)e^(-t/tau2)
     *   e^(t*(1/tau1 - 1/tau2)) = tau1/tau2
     *   t * (tau2 - tau1)/(tau1*tau2) = ln(tau1/tau2)
     *   t = tau1*tau2*ln(tau1/tau2)/(tau2-tau1)
     *   t = tau1*tau2*ln(tau2/tau1)/(tau2-tau1) [ln(a/b) = -ln(b/a)]
     */
    if (tau1 <= 0.0 || tau2 <= tau1) return 0.0;

    return tau1 * tau2 * log(tau2 / tau1) / (tau2 - tau1);
}

double surge_normalization_factor(double tau1, double tau2)
{
    /* k = 1 / (e^(-t_peak/tau2) - e^(-t_peak/tau1))
     *
     * This ensures v(t_peak) = Vp exactly.
     */
    double t_peak = surge_peak_time(tau1, tau2);
    if (t_peak <= 0.0) return 0.0;

    double exp_decay = exp(-t_peak / tau2);
    double exp_rise  = exp(-t_peak / tau1);
    double denom = exp_decay - exp_rise;

    if (fabs(denom) < 1e-15) return 0.0;

    return 1.0 / denom;
}

/* ==================================================================
 * L4: Rise Time and Pulse Width (IEC Standard Parameters)
 *
 * The IEC defines the waveform parameters as:
 * - Front time T1: 1.67 * (t_90% - t_10%) for surge voltage
 *   (approximated as 1.2 us for the standard combination wave)
 * - Duration T2: time from virtual origin to 50% of peak on tail
 *   (50 us for the standard combination wave)
 *
 * Virtual origin: intersection of the line through 30% and 90%
 * points with the time axis.
 * ================================================================== */

double surge_rise_time_10_90(double tau1, double tau2, double eps)
{
    if (tau1 <= 0.0 || tau2 <= tau1) return 0.0;
    if (eps <= 0.0) eps = 1e-6;

    double vp = 1.0;  /* Use normalized waveform */
    double t_peak = surge_peak_time(tau1, tau2);
    double v_peak = surge_double_exponential(t_peak, vp, tau1, tau2);

    if (v_peak <= 0.0) return 0.0;

    double target_10 = 0.10 * v_peak;
    double target_90 = 0.90 * v_peak;

    /* Binary search for 10% crossing time (before peak) */
    double t_lo = 0.0;
    double t_hi = t_peak;
    double t_10 = 0.0;
    int iter;
    for (iter = 0; iter < 100; iter++) {
        double t_mid = (t_lo + t_hi) / 2.0;
        double v_mid = surge_double_exponential(t_mid, vp, tau1, tau2);

        if (fabs(v_mid - target_10) < eps * v_peak) {
            t_10 = t_mid;
            break;
        }
        if (v_mid < target_10) {
            t_lo = t_mid;
        } else {
            t_hi = t_mid;
        }
        if (t_hi - t_lo < eps * t_peak) {
            t_10 = (t_lo + t_hi) / 2.0;
            break;
        }
    }

    /* Binary search for 90% crossing time (before peak) */
    t_lo = 0.0;
    t_hi = t_peak;
    double t_90 = 0.0;
    for (iter = 0; iter < 100; iter++) {
        double t_mid = (t_lo + t_hi) / 2.0;
        double v_mid = surge_double_exponential(t_mid, vp, tau1, tau2);

        if (fabs(v_mid - target_90) < eps * v_peak) {
            t_90 = t_mid;
            break;
        }
        if (v_mid < target_90) {
            t_lo = t_mid;
        } else {
            t_hi = t_mid;
        }
        if (t_hi - t_lo < eps * t_peak) {
            t_90 = (t_lo + t_hi) / 2.0;
            break;
        }
    }

    if (t_90 <= t_10) return 0.0;

    /* Front time = 1.67 * (t_90 - t_10) per IEC 60060-1 */
    return 1.67 * (t_90 - t_10);
}

double surge_pulse_width_fwhm(double tau1, double tau2, double eps)
{
    if (tau1 <= 0.0 || tau2 <= tau1) return 0.0;
    if (eps <= 0.0) eps = 1e-6;

    double vp = 1.0;
    double t_peak = surge_peak_time(tau1, tau2);
    double v_peak = surge_double_exponential(t_peak, vp, tau1, tau2);
    double target_half = 0.50 * v_peak;

    if (v_peak <= 0.0) return 0.0;

    /* Find first 50% crossing on the rising edge */
    double t_lo = 0.0;
    double t_hi = t_peak;
    double t_rise_50 = 0.0;
    int iter;
    for (iter = 0; iter < 100; iter++) {
        double t_mid = (t_lo + t_hi) / 2.0;
        double v_mid = surge_double_exponential(t_mid, vp, tau1, tau2);

        if (fabs(v_mid - target_half) < eps * v_peak) {
            t_rise_50 = t_mid;
            break;
        }
        if (v_mid < target_half) t_lo = t_mid;
        else t_hi = t_mid;

        if (t_hi - t_lo < eps * t_peak) {
            t_rise_50 = (t_lo + t_hi) / 2.0;
            break;
        }
    }

    /* Find second 50% crossing on the tail (after peak) */
    /* Use exponential extrapolation: search up to 5*tau2 */
    t_lo = t_peak;
    t_hi = 10.0 * tau2;  /* Go deep into the tail */
    double t_fall_50 = 0.0;

    /* First, verify that v(t_hi) < target_half */
    double v_hi = surge_double_exponential(t_hi, vp, tau1, tau2);
    if (v_hi >= target_half) {
        t_hi = 20.0 * tau2;
        v_hi = surge_double_exponential(t_hi, vp, tau1, tau2);
    }

    for (iter = 0; iter < 100; iter++) {
        double t_mid = (t_lo + t_hi) / 2.0;
        double v_mid = surge_double_exponential(t_mid, vp, tau1, tau2);

        if (fabs(v_mid - target_half) < eps * v_peak) {
            t_fall_50 = t_mid;
            break;
        }
        if (v_mid > target_half) t_lo = t_mid;
        else t_hi = t_mid;

        if (t_hi - t_lo < eps * t_peak) {
            t_fall_50 = (t_lo + t_hi) / 2.0;
            break;
        }
    }

    if (t_fall_50 <= t_rise_50) return 0.0;

    /* Full-width at half-maximum */
    return t_fall_50 - t_rise_50;
}

/* ==================================================================
 * L4: Fourier Spectrum Analysis
 * ================================================================== */

double surge_spectrum_magnitude(double f, double vp, double tau1, double tau2)
{
    /* Fourier transform of v(t) = Vp*k*(e^(-t/tau2) - e^(-t/tau1)):
     *
     * V(jw) = Vp*k * [tau2/(1+j*w*tau2) - tau1/(1+j*w*tau1)]
     *
     * Magnitude:
     * |V(f)| = Vp*k * sqrt( [tau2/(1+(w*tau2)^2) - tau1/(1+(w*tau1)^2)]^2
     *                      + [w*tau2^2/(1+(w*tau2)^2) - w*tau1^2/(1+(w*tau1)^2)]^2 )
     */
    if (f < 0.0 || tau1 <= 0.0 || tau2 <= tau1) return 0.0;

    double k = surge_normalization_factor(tau1, tau2);
    if (k <= 0.0) return 0.0;

    double w = 2.0 * M_PI * f;

    double w_tau1 = w * tau1;
    double w_tau2 = w * tau2;
    double den1 = 1.0 + w_tau1 * w_tau1;
    double den2 = 1.0 + w_tau2 * w_tau2;

    double real_part = tau2 / den2 - tau1 / den1;
    double imag_part = w_tau2 * tau2 / den2 - w_tau1 * tau1 / den1;

    return vp * k * sqrt(real_part * real_part + imag_part * imag_part);
}

double surge_corner_frequency(double tau1, double tau2, double vp)
{
    /* Find frequency where |V(f)| = |V(0)|/sqrt(2) = -3dB point.
     *
     * |V(0)| = Vp * k * (tau2 - tau1)  (DC component)
     *
     * For standard surge waveforms:
     *   1.2/50us: fc ~ 8-12 kHz
     *   8/20us:   fc ~ 15-25 kHz
     *   5/50ns:   fc ~ 5-10 MHz
     */
    if (tau1 <= 0.0 || tau2 <= tau1) return 0.0;

    double k = surge_normalization_factor(tau1, tau2);
    if (k <= 0.0) return 0.0;

    double v_dc = vp * k * (tau2 - tau1);
    double target = v_dc / sqrt(2.0);

    /* Binary search in frequency domain */
    double f_lo = 0.0;
    double f_hi = 1.0 / (tau1 * 1e-6);  /* Start at ~1/tau1 Hz */
    if (f_hi < 1e3) f_hi = 1e9;  /* At least 1 GHz search range */

    int iter;
    for (iter = 0; iter < 100; iter++) {
        double f_mid = (f_lo + f_hi) / 2.0;
        double mag_mid = surge_spectrum_magnitude(f_mid, vp, tau1, tau2);

        if (fabs(mag_mid - target) < 1e-6 * v_dc) return f_mid;

        if (mag_mid > target) f_lo = f_mid;
        else f_hi = f_mid;

        if (f_hi - f_lo < 1.0) return (f_lo + f_hi) / 2.0;
    }

    return (f_lo + f_hi) / 2.0;
}

double surge_spectral_energy_density(double f, double vp, double tau1,
                                      double tau2, double z0)
{
    /* S(f) = |V(f)|^2 / (2 * Z0)  [W/Hz]
     *
     * This is the power spectral density delivered to a matched load.
     */
    if (f < 0.0 || z0 <= 0.0) return 0.0;

    double mag_v = surge_spectrum_magnitude(f, vp, tau1, tau2);
    return mag_v * mag_v / (2.0 * z0);
}

/* ==================================================================
 * L4: Energy Computation (Closed-Form Integrals)
 * ================================================================== */

double surge_energy_resistive(double vp, double tau1, double tau2,
                               double resistance)
{
    /* E = integral_0^inf [v(t)^2 / R] dt
     *
     * Closed form for double-exponential:
     * v(t)^2 = Vp^2 * k^2 * [e^(-2t/tau2) - 2*e^(-t*(1/tau1+1/tau2)) + e^(-2t/tau1)]
     *
     * Integral of each term:
     *   int_0^inf e^(-2t/tau2) dt = tau2/2
     *   int_0^inf e^(-2t/tau1) dt = tau1/2
     *   int_0^inf e^(-t*(1/tau1+1/tau2)) dt = tau1*tau2/(tau1+tau2)
     *
     * E = Vp^2 * k^2 / R * [tau2/2 + tau1/2 - 2*tau1*tau2/(tau1+tau2)]
     */
    if (tau1 <= 0.0 || tau2 <= tau1 || resistance <= 0.0) return 0.0;

    double k = surge_normalization_factor(tau1, tau2);
    if (k <= 0.0) return 0.0;

    double term1 = tau2 / 2.0;
    double term2 = tau1 / 2.0;
    double term3 = 2.0 * tau1 * tau2 / (tau1 + tau2);

    double energy = vp * vp * k * k / resistance * (term1 + term2 - term3);

    /* Energy must be non-negative */
    if (energy < 0.0) energy = 0.0;

    return energy;
}

double surge_i2t(double ip, double tau1, double tau2)
{
    /* I^2*t = integral_0^inf [i(t)^2] dt
     *
     * Same form as energy_resistive with R=1 and Vp->Ip.
     */
    return surge_energy_resistive(ip, tau1, tau2, 1.0);
}

double surge_charge_transfer(double ip, double tau1, double tau2)
{
    /* Q = integral_0^inf [i(t)] dt
     *
     * i(t) = Ip*k*[e^(-t/tau2) - e^(-t/tau1)]
     *
     * int_0^inf e^(-t/tau) dt = tau
     *
     * Q = Ip * k * (tau2 - tau1)
     */
    if (tau1 <= 0.0 || tau2 <= tau1) return 0.0;

    double k = surge_normalization_factor(tau1, tau2);
    if (k <= 0.0) return 0.0;

    return ip * k * (tau2 - tau1);
}

/* ==================================================================
 * L3: Ring Wave and Damped Sinusoidal Generation
 * ================================================================== */

double surge_ring_wave(double t, double vp, double frequency,
                       double tau_rise, double tau_decay)
{
    /* Ring wave (IEEE C62.41):
     *   v(t) = Vp * (1 - e^(-t/tau_rise)) * e^(-t/tau_decay) * sin(2*pi*f*t)
     *
     * Standard parameters: f = 100 kHz, tau_rise = 0.5 us, tau_decay = 10 us
     */
    if (t < 0.0 || frequency <= 0.0 || tau_rise <= 0.0 || tau_decay <= 0.0)
        return 0.0;

    double envelope_rise = 1.0 - exp(-t / tau_rise);
    double envelope_decay = exp(-t / tau_decay);
    double oscillation = sin(2.0 * M_PI * frequency * t);

    return vp * envelope_rise * envelope_decay * oscillation;
}

/* ==================================================================
 * L5: Waveform Series Generation
 * ================================================================== */

int surge_waveform_series(double *v_out, int n_points,
                          double t_max, double vp,
                          double tau1, double tau2)
{
    if (!v_out || n_points < 2 || t_max <= 0.0) return SURGE_ERR_NULL_PTR;
    if (tau1 <= 0.0 || tau2 <= tau1) return SURGE_ERR_INVALID_WAVEFORM;

    double dt = t_max / (double)(n_points - 1);
    int i;

    for (i = 0; i < n_points; i++) {
        double t = (double)i * dt;
        v_out[i] = surge_double_exponential(t, vp, tau1, tau2);
    }

    return SURGE_OK;
}

int eft_burst_series(double *v_out, int n_points,
                      const eft_burst_params_t *burst,
                      double sample_period_ns)
{
    if (!v_out || !burst || n_points < 2) return SURGE_ERR_NULL_PTR;
    if (sample_period_ns <= 0.0) return SURGE_ERR_INVALID_WAVEFORM;

    /* Generate EFT burst as a series of pulses */
    int pulses = eft_pulses_per_burst(burst);
    if (pulses <= 0) return SURGE_ERR_INVALID_WAVEFORM;

    double pulse_period_ns = 1e6 / burst->repetition_khz; /* Period in ns */
    double tau1 = SURGE_TAU1_5_50_NS;
    double tau2 = SURGE_TAU2_5_50_NS;

    int i, p;
    /* Clear output */
    for (i = 0; i < n_points; i++) v_out[i] = 0.0;

    for (p = 0; p < pulses; p++) {
        double t0_ns = (double)p * pulse_period_ns;

        for (i = 0; i < n_points; i++) {
            double t_ns = (double)i * sample_period_ns;
            double t_rel = t_ns - t0_ns;

            if (t_rel >= 0.0) {
                v_out[i] += surge_double_exponential(t_rel * 1e-3,
                            burst->v_peak, tau1, tau2);
            }
        }
    }

    return SURGE_OK;
}

/* ==================================================================
 * L1: Waveform Parameter Initialization
 * ================================================================== */

int surge_waveform_init(surge_waveform_params_t *wf,
                        surge_waveform_type_t type,
                        double v_peak,
                        double source_impedance)
{
    if (!wf) return SURGE_ERR_NULL_PTR;
    if (v_peak <= 0.0) return SURGE_ERR_INVALID_WAVEFORM;

    memset(wf, 0, sizeof(*wf));
    wf->type = type;
    wf->v_peak = v_peak;
    wf->source_impedance = source_impedance;

    switch (type) {
        case SURGE_WAVE_1_2_50_US:
            wf->tau1 = SURGE_TAU1_1_2_50;
            wf->tau2 = SURGE_TAU2_1_2_50;
            wf->t_front = 1.2;
            wf->t_duration = 50.0;
            wf->i_peak = v_peak / source_impedance;
            snprintf(wf->description, sizeof(wf->description),
                     "1.2/50us Combination Wave Voltage");
            break;

        case SURGE_WAVE_8_20_US:
            wf->tau1 = SURGE_TAU1_8_20;
            wf->tau2 = SURGE_TAU2_8_20;
            wf->t_front = 8.0;
            wf->t_duration = 20.0;
            wf->i_peak = v_peak; /* v_peak treated as current for this type */
            wf->v_peak = v_peak * source_impedance;
            snprintf(wf->description, sizeof(wf->description),
                     "8/20us Combination Wave Current");
            break;

        case SURGE_WAVE_10_700_US:
            wf->tau1 = SURGE_TAU1_10_700;
            wf->tau2 = SURGE_TAU2_10_700;
            wf->t_front = 10.0;
            wf->t_duration = 700.0;
            wf->i_peak = v_peak / source_impedance;
            snprintf(wf->description, sizeof(wf->description),
                     "10/700us Telecom Surge");
            break;

        case SURGE_WAVE_10_350_US:
            wf->tau1 = 3.5;
            wf->tau2 = 480.0;
            wf->t_front = 10.0;
            wf->t_duration = 350.0;
            wf->i_peak = v_peak / source_impedance;
            snprintf(wf->description, sizeof(wf->description),
                     "10/350us Direct Lightning");
            break;

        case SURGE_WAVE_10_1000_US:
            wf->tau1 = 3.8;
            wf->tau2 = 1400.0;
            wf->t_front = 10.0;
            wf->t_duration = 1000.0;
            wf->i_peak = v_peak / source_impedance;
            snprintf(wf->description, sizeof(wf->description),
                     "10/1000us Telecom Long Wave");
            break;

        case SURGE_WAVE_5_50_NS:
            wf->tau1 = SURGE_TAU1_5_50_NS;
            wf->tau2 = SURGE_TAU2_5_50_NS;
            wf->t_front = 0.005; /* 5ns in us */
            wf->t_duration = 0.050; /* 50ns in us */
            wf->i_peak = v_peak / source_impedance;
            snprintf(wf->description, sizeof(wf->description),
                     "5/50ns EFT Pulse");
            break;

        case SURGE_WAVE_RING_0_5_US:
            wf->tau1 = 0.5;
            wf->tau2 = 10.0;
            wf->t_front = 0.5;
            wf->t_duration = 10.0;
            wf->i_peak = v_peak / source_impedance;
            snprintf(wf->description, sizeof(wf->description),
                     "0.5us 100kHz Ring Wave");
            break;

        case SURGE_WAVE_1MHZ_DAMPED:
            wf->tau1 = 0.1;
            wf->tau2 = 5.0;
            wf->t_front = 0.1;
            wf->t_duration = 5.0;
            wf->i_peak = v_peak / source_impedance;
            snprintf(wf->description, sizeof(wf->description),
                     "1MHz Damped Sinusoid");
            break;

        default:
            return SURGE_ERR_INVALID_WAVEFORM;
    }

    /* Compute energy for a typical 2-ohm load (IEC standard) */
    if (wf->source_impedance > 0.0) {
        wf->energy_per_pulse = surge_energy_resistive(wf->v_peak,
                                wf->tau1, wf->tau2, wf->source_impedance);
    }

    return SURGE_OK;
}

int eft_burst_init(eft_burst_params_t *burst, eft_test_level_t level)
{
    if (!burst) return SURGE_ERR_NULL_PTR;
    if (level > EFT_LEVEL_X) return SURGE_ERR_INVALID_LEVEL;

    memset(burst, 0, sizeof(*burst));
    burst->level = level;
    burst->pulse_rise_ns = EFT_PULSE_RISE_NS;
    burst->pulse_width_ns = EFT_PULSE_WIDTH_NS;
    burst->burst_duration_ms = EFT_BURST_DURATION_MS;
    burst->burst_period_ms = EFT_BURST_PERIOD_MS;
    burst->coupling_cap_pf = EFT_COUPLING_CAP_PF;
    burst->test_duration_sec = 60.0;  /* 1 minute minimum per IEC */

    switch (level) {
        case EFT_LEVEL_1:
            burst->v_peak = 0.5;
            burst->repetition_khz = 5.0;
            break;
        case EFT_LEVEL_2:
            burst->v_peak = 1.0;
            burst->repetition_khz = 5.0;
            break;
        case EFT_LEVEL_3:
            burst->v_peak = 2.0;
            burst->repetition_khz = 5.0;
            break;
        case EFT_LEVEL_4:
            burst->v_peak = 4.0;
            burst->repetition_khz = 2.5;
            break;
        default:
            burst->v_peak = 0.5;
            burst->repetition_khz = 5.0;
            break;
    }

    burst->pulses_per_burst = eft_pulses_per_burst(burst);
    return SURGE_OK;
}

int eft_pulses_per_burst(const eft_burst_params_t *burst)
{
    if (!burst) return 0;
    /* n_pulses = burst_duration_ms * repetition_khz */
    return (int)(burst->burst_duration_ms * burst->repetition_khz);
}