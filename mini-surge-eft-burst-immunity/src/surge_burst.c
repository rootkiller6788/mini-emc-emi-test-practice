/**
 * surge_burst.c - EFT/Burst Immunity Test Implementation
 *
 * L6 Canonical Problems: EFT burst generation, coupling clamp
 * modeling, filter design, statistical burst analysis.
 *
 * Reference:
 *   IEC 61000-4-4: EFT/Burst Immunity Test
 *   K. Hall, "EFT Testing Demystified", In Compliance, 2014
 *   W. Rhoades, "Avoiding EFT Test Failures", ITEM, 2018
 */

#include "surge_burst.h"
#include "surge_waveform.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ==================================================================
 * L6: EFT Individual Pulse Generation
 *
 * EFT pulse per IEC 61000-4-4:
 *   Rise time: 5 ns +/- 30%
 *   Pulse width: 50 ns +/- 30%
 *   Peak voltage: 0.5 - 4 kV depending on test level
 *
 * The pulse uses the double-exponential model:
 *   v(t) = Vp * k * (e^(-t/tau2) - e^(-t/tau1))
 * with tau1 = 1.70e-3 us, tau2 = 0.068 us
 * ================================================================== */

double eft_pulse(double t, double t0, double v_peak)
{
    /* Generate an individual EFT pulse at time offset t0.
     * Time t is in microseconds, t0 is in microseconds.
     */
    if (t < t0 || v_peak <= 0.0) return 0.0;

    double t_rel = t - t0;  /* Relative time from pulse onset */

    /* EFT time constants (in microseconds) */
    double tau1 = SURGE_TAU1_5_50_NS;  /* 0.0017 us = 1.7 ns */
    double tau2 = SURGE_TAU2_5_50_NS;  /* 0.068 us = 68 ns */

    return surge_double_exponential(t_rel, v_peak, tau1, tau2);
}

int eft_burst_generate(double *pulse_times, int *num_pulses,
                        const eft_burst_params_t *burst)
{
    if (!pulse_times || !num_pulses || !burst) return SURGE_ERR_NULL_PTR;

    int n = eft_pulses_per_burst(burst);
    if (n <= 0) return SURGE_ERR_INVALID_WAVEFORM;

    if (*num_pulses < n) {
        *num_pulses = n;
        return SURGE_ERR_ALLOC;
    }

    double pulse_period_us = 1000.0 / burst->repetition_khz;  /* in us */
    int i;

    for (i = 0; i < n; i++) {
        pulse_times[i] = (double)i * pulse_period_us;
    }

    *num_pulses = n;
    return SURGE_OK;
}

int eft_burst_amplitude_distribution(double *amplitudes,
                                      int num_pulses,
                                      double v_peak_nominal,
                                      double std_dev_pct)
{
    /* Generate Gaussian-distributed pulse amplitudes.
     *
     * In practice, EFT generators have some amplitude variation.
     * The standard allows +/- 10% variation.
     *
     * We use a simple Box-Muller-like approximation for the
     * Gaussian distribution within the truncation limits.
     */
    if (!amplitudes || num_pulses <= 0 || v_peak_nominal <= 0.0)
        return SURGE_ERR_NULL_PTR;

    double std_dev = v_peak_nominal * std_dev_pct / 100.0;
    double v_min = v_peak_nominal * 0.9;  /* -10% lower bound */
    double v_max = v_peak_nominal * 1.1;  /* +10% upper bound */

    /* Seed with a fixed value for reproducibility */
    unsigned int seed = 12345;
    int i;

    for (i = 0; i < num_pulses; i++) {
        /* Simple pseudo-random normal approximation using
         * uniform random numbers and the Central Limit Theorem.
         * Sum of 12 uniforms has variance = 1, mean = 6.
         * Subtract 6 to get mean 0, variance 1. */
        double u = 0.0;
        int j;
        for (j = 0; j < 12; j++) {
            seed = seed * 1103515245 + 12345;
            u += (double)((seed >> 16) & 0x7FFF) / 32768.0;
        }

        double z = u - 6.0;  /* Approximate N(0,1) */
        double amp = v_peak_nominal + z * std_dev;

        /* Truncate to allowed range */
        if (amp < v_min) amp = v_min;
        if (amp > v_max) amp = v_max;

        amplitudes[i] = amp;
    }

    return SURGE_OK;
}

/* ==================================================================
 * L6: EFT Coupling Clamp Model
 * ================================================================== */

double eft_coupling_clamp_response(double f, double clamp_capacitance_pf,
                                    double z_source)
{
    /* Capacitive coupling clamp transfer function:
     *
     * |H(f)| = 1 / sqrt(1 + (fc/f)^2)
     *
     * where fc = 1/(2*pi*Z_source*C_clamp)
     *
     * For standard values (Z=50 ohm, C=33 nF):
     *   fc = 1/(2*pi*50*33e-9) ~ 96.5 kHz
     *
     * At f = 10 MHz (typical EFT spectrum peak):
     *   |H| = 1 / sqrt(1 + (0.0965/10)^2) ~ 1.0
     * The clamp transmits EFT pulses with minimal attenuation.
     */
    if (f < 0.0 || clamp_capacitance_pf <= 0.0 || z_source <= 0.0)
        return 0.0;

    if (f == 0.0) return 0.0;  /* DC blocked by capacitor */

    double c_farad = clamp_capacitance_pf * 1e-12;
    double fc = 1.0 / (2.0 * M_PI * z_source * c_farad);

    if (fc <= 0.0) return 0.0;

    double ratio = fc / f;
    return 1.0 / sqrt(1.0 + ratio * ratio);
}

double eft_delivered_voltage(double v_generator, double z_eut,
                              double clamp_capacitance_pf,
                              double pulse_rise_ns)
{
    /* Estimate peak voltage delivered to EUT.
     *
     * The coupling clamp + EUT input impedance forms a capacitive
     * voltage divider.
     *
     * For high-impedance EUT (>1 kohm):
     *   V_EUT ~ V_generator (nearly full voltage)
     *
     * For low-impedance EUT (50 ohm):
     *   V_EUT is limited by the clamp's coupling capacitance.
     *
     * The critical parameter is the RC time constant of the
     * coupling capacitor + EUT impedance relative to the
     * pulse rise time.
     */
    if (v_generator <= 0.0 || clamp_capacitance_pf <= 0.0) return 0.0;
    if (z_eut <= 0.0) return v_generator;  /* Short circuit: full current */

    double c_farad = clamp_capacitance_pf * 1e-12;
    double tau_rc = z_eut * c_farad;  /* Seconds */
    double t_rise = pulse_rise_ns * 1e-9;  /* Seconds */

    /* If RC >> t_rise: capacitor acts nearly as short circuit */
    if (tau_rc > t_rise * 10.0) {
        return v_generator;
    }

    /* If RC << t_rise: capacitor charges, reduces delivered V */
    /* Simplified exponential charging model:
     * V_EUT = V_generator * (1 - exp(-t_rise/tau_rc))
     */
    double ratio = t_rise / tau_rc;
    if (ratio > 50.0) ratio = 50.0;  /* Prevent underflow */

    return v_generator * (1.0 - exp(-ratio));
}

/* ==================================================================
 * L6: EFT Filter Design
 * ================================================================== */

int eft_filter_design(eft_filter_params_t *filter,
                      eft_filter_topology_t topology,
                      double fc_hz, int max_current_a,
                      double voltage_rating_v)
{
    if (!filter) return SURGE_ERR_NULL_PTR;
    if (fc_hz <= 0.0 || max_current_a <= 0 || voltage_rating_v <= 0.0)
        return SURGE_ERR_INVALID_WAVEFORM;

    memset(filter, 0, sizeof(*filter));
    filter->topology = topology;
    filter->cutoff_freq_hz = fc_hz;
    filter->max_current_a = max_current_a;
    filter->voltage_rating_v = voltage_rating_v;

    /* Design based on topology.
     *
     * For an LC filter (single-stage):
     *   fc = 1/(2*pi*sqrt(L*C))
     *   L = Z_char / (2*pi*fc)
     *   C = 1 / (2*pi*fc*Z_char)
     *
     * Characteristic impedance Z_char sets L/C ratio.
     * For power lines: Z_char ~ 5-20 ohm
     * For signal lines: Z_char ~ 50-100 ohm
     */
    double z_char;
    if (max_current_a > 5.0) {
        z_char = 5.0;  /* Low impedance for high current */
    } else {
        z_char = 50.0; /* Standard impedance for signal lines */
    }

    switch (topology) {
        case EFT_FILTER_LC:
            filter->inductance_uh = z_char / (2.0 * M_PI * fc_hz) * 1e6;
            filter->capacitance_input_pf = 1.0 / (2.0 * M_PI * fc_hz *
                                            z_char) * 1e12;
            filter->capacitance_output_pf = filter->capacitance_input_pf;
            /* -40 dB/decade rolloff above fc */
            filter->attenuation_at_100mhz_db =
                40.0 * log10(100e6 / fc_hz);
            break;

        case EFT_FILTER_PI:
            /* Pi-section: C_in - L - C_out
             * More capacitance at input/output for better HF performance
             */
            filter->inductance_uh = z_char / (2.0 * M_PI * fc_hz) * 1e6;
            filter->capacitance_input_pf = 0.5 / (2.0 * M_PI * fc_hz *
                                            z_char) * 1e12;
            filter->capacitance_output_pf = filter->capacitance_input_pf;
            /* -60 dB/decade rolloff */
            filter->attenuation_at_100mhz_db =
                60.0 * log10(100e6 / fc_hz);
            break;

        case EFT_FILTER_T:
            /* T-section: L_in - C - L_out */
            filter->inductance_uh = z_char / (4.0 * M_PI * fc_hz) * 1e6;
            filter->capacitance_input_pf = 0.0;
            filter->capacitance_output_pf = 1.0 / (2.0 * M_PI * fc_hz *
                                             z_char) * 1e12;
            filter->attenuation_at_100mhz_db =
                60.0 * log10(100e6 / fc_hz);
            break;

        case EFT_FILTER_CM_CHOKE:
            /* Common-mode choke: high CM inductance, low DM inductance
             * CM_L >> DM_L because of coupled windings */
            filter->inductance_uh = 1000.0 / (2.0 * M_PI * fc_hz) * 1e6;
            filter->capacitance_input_pf = 1.0 / (2.0 * M_PI * fc_hz *
                                            100.0) * 1e12;
            filter->capacitance_output_pf = filter->capacitance_input_pf;
            filter->cmrr_db = 30.0;  /* Typical CM choke CMRR */
            filter->attenuation_at_100mhz_db =
                40.0 * log10(100e6 / fc_hz) + filter->cmrr_db;
            break;

        case EFT_FILTER_FERRITE:
            /* Ferrite bead: broadband lossy impedance */
            filter->inductance_uh = 0.0;
            filter->capacitance_input_pf = 0.0;
            filter->capacitance_output_pf = 0.0;
            /* Ferrite beads have ~100-1000 ohm at 100 MHz */
            filter->attenuation_at_100mhz_db = 20.0;
            break;

        default:
            return SURGE_ERR_INVALID_WAVEFORM;
    }

    /* Clamp attenuation to realistic maximum */
    if (filter->attenuation_at_100mhz_db > 120.0)
        filter->attenuation_at_100mhz_db = 120.0;

    return SURGE_OK;
}

double eft_filter_attenuation(const eft_filter_params_t *filter,
                               double frequency_hz, double r_load)
{
    if (!filter || frequency_hz < 0.0) return 0.0;
    if (r_load <= 0.0) r_load = 50.0;

    double f = frequency_hz;
    double fc = filter->cutoff_freq_hz;
    double L = filter->inductance_uh * 1e-6;  /* uH -> H */
    double C = (filter->capacitance_output_pf > 0.0 ?
                filter->capacitance_output_pf :
                filter->capacitance_input_pf) * 1e-12;  /* pF -> F */

    if (fc <= 0.0) {
        /* For ferrite bead: estimate from typical impedance */
        if (filter->topology == EFT_FILTER_FERRITE) {
            /* Ferrite bead attenuation increases with frequency */
            if (f < 1e6) return 3.0;   /* <1 MHz: ~3dB */
            if (f < 10e6) return 6.0;  /* 1-10 MHz: ~6dB */
            if (f < 100e6) return 12.0; /* 10-100 MHz: ~12dB */
            return 20.0;                /* >100 MHz: ~20dB */
        }
        return 0.0;
    }

    /* LC filter transfer function magnitude:
     * |H(f)| = 1 / sqrt((1 - (f/fc)^2)^2 + (f/(fc*Q))^2)
     * where Q = R_load / sqrt(L/C) is the quality factor
     */
    double ratio = f / fc;

    double q;
    if (L > 0.0 && C > 0.0) {
        double z0 = sqrt(L / C);
        q = r_load / z0;
        if (q < 0.1) q = 0.1;  /* Prevent singularity */
    } else {
        q = 1.0;
    }

    double real_part = 1.0 - ratio * ratio;
    double imag_part = ratio / q;
    double mag_sq = real_part * real_part + imag_part * imag_part;

    if (mag_sq <= 0.0) return 120.0;  /* Very high attenuation */

    double attenuation = -10.0 * log10(mag_sq);
    if (attenuation < 0.0) attenuation = 0.0;

    return attenuation;
}

double eft_optimal_cutoff(double signal_bandwidth_hz,
                           double eft_peak_freq_hz,
                           double min_attenuation_db)
{
    /* Determine optimal filter cutoff frequency:
     *
     * Constraints:
     * 1. fc > signal_bandwidth (pass signal without distortion)
     * 2. fc < eft_peak_freq / 10^(attenuation/40) for LC filter
     *    (provide sufficient attenuation at EFT frequencies)
     *
     * For an LC filter (-40 dB/decade):
     *   attenuation = 40 * log10(f_eft / fc)
     *   fc = f_eft / 10^(attenuation/40)
     *
     * Example: signal_bw = 10 kHz (audio), f_eft = 10 MHz,
     *          min_attenuation = 40 dB
     *   fc_from_signal > 10 kHz
     *   fc_from_attenuation = 10e6 / 10^(40/40) = 10e6 / 10 = 1 MHz
     *
     * Since 1 MHz > 10 kHz, fc = 1 MHz satisfies both constraints.
     */
    if (signal_bandwidth_hz <= 0.0 || eft_peak_freq_hz <= 0.0)
        return signal_bandwidth_hz;

    double fc_from_attenuation = eft_peak_freq_hz /
                                  pow(10.0, min_attenuation_db / 40.0);

    double fc_optimal = fc_from_attenuation;
    if (signal_bandwidth_hz > fc_optimal)
        fc_optimal = signal_bandwidth_hz * 1.5;  /* 50% margin */

    /* If signal bandwidth exceeds attenuation requirement,
     * we need a higher-order filter or ferrite-based solution */
    if (fc_optimal > eft_peak_freq_hz / 10.0) {
        /* Cannot achieve required attenuation with single-stage LC */
        fc_optimal = eft_peak_freq_hz / 10.0;
    }

    return fc_optimal;
}

/* ==================================================================
 * L6: Statistical Burst Analysis
 * ================================================================== */

double eft_burst_total_energy(const eft_burst_params_t *burst,
                               double v_peak_per_pulse,
                               double load_impedance)
{
    if (!burst || load_impedance <= 0.0) return 0.0;

    int n_pulses = burst->pulses_per_burst;
    if (n_pulses <= 0) n_pulses = 75;  /* Default for 15ms @ 5kHz */

    /* Energy per individual EFT pulse into resistive load */
    double tau1 = SURGE_TAU1_5_50_NS;
    double tau2 = SURGE_TAU2_5_50_NS;
    double energy_per_pulse = surge_energy_resistive(v_peak_per_pulse,
                                                      tau1, tau2, load_impedance);

    return energy_per_pulse * (double)n_pulses;
}

double eft_effective_pulse_rate(const eft_burst_params_t *burst)
{
    if (!burst) return 0.0;

    if (burst->burst_period_ms <= 0.0) return burst->repetition_khz * 1000.0;

    /* Effective average rate = pulses_per_burst / burst_period
     * Example: 75 pulses / 0.3 s = 250 pulses/s average
     */
    int n_pulses = burst->pulses_per_burst;
    if (n_pulses <= 0) n_pulses = (int)(burst->burst_duration_ms *
                                         burst->repetition_khz);

    return (double)n_pulses / (burst->burst_period_ms * 1e-3);
}

int eft_worst_case_stress(const eft_burst_params_t *burst,
                          double *worst_v_peak,
                          double *worst_dv_dt,
                          double *worst_energy_per_burst)
{
    if (!burst || !worst_v_peak || !worst_dv_dt || !worst_energy_per_burst)
        return SURGE_ERR_NULL_PTR;

    /* Worst-case analysis uses maximum tolerances:
     *   - Peak voltage: nominal + 10%
     *   - Rise time: nominal - 30% = 3.5 ns (steepest edge)
     *   - Pulse width: nominal + 30% = 65 ns (most energy)
     */
    *worst_v_peak = burst->v_peak * 1.10;

    /* Worst-case dV/dt = (0.9*V_peak - 0.1*V_peak) / (0.8*t_rise_min)
     *   = 0.8 * V_peak / (0.8 * t_rise_nom * 0.7)
     *   = V_peak / (t_rise_nom * 0.7)
     */
    double t_rise_min = burst->pulse_rise_ns * 0.7;  /* -30% tolerance */
    if (t_rise_min <= 0.0) t_rise_min = 3.5;  /* 3.5 ns minimum */
    *worst_dv_dt = *worst_v_peak * 1000.0 / t_rise_min;  /* kV/us */

    /* Worst-case energy: longest pulse width, highest voltage */
    double z_load = 50.0;  /* Assume 50 ohm EUT input */
    /* Note: eft_burst_total_energy uses standard EFT tau constants internally */
    *worst_energy_per_burst = eft_burst_total_energy(burst,
                                                      *worst_v_peak, z_load);

    return SURGE_OK;
}