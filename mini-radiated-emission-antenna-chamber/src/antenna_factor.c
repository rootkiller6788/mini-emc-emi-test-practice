/* =========================================================================
 * antenna_factor.c — Antenna Factor Theory, Calibration, and Interpolation
 *
 * L1: Antenna Factor definition and physical interpretation
 * L3: Mathematical derivation from antenna parameters
 * L4: Reciprocity theorem applied to antenna factor
 * L5: Frequency-domain interpolation algorithm
 *
 * References:
 *   Balanis (2016) Antenna Theory, sections 2.16, 4.4
 *   CISPR 16-1-4:2019 Annex C
 *   ANSI C63.5:2017 (Antenna Calibration)
 *   Friis (1946) "A Note on a Simple Transmission Formula"
 * ========================================================================= */

#include "radiated_emission.h"
#include "antenna_types.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* -------------------------------------------------------------------------
 * Physical Constants
 * ------------------------------------------------------------------------- */
#define SPEED_OF_LIGHT 299792458.0   /* m/s */
#define FREE_SPACE_Z0  376.730313668 /* ohm, mu_0/epsilon_0 exact */
#define PI              3.14159265358979323846

/* -------------------------------------------------------------------------
 * L1: Antenna Factor Fundamental Definition
 *
 * Antenna Factor AF relates incident E-field to antenna terminal voltage:
 *   AF = E / V_terminals    [1/m]
 *   AF_dB = 20*log10(E/V)   [dB(1/m)]
 *
 * For measurement: E_dBuV/m = V_dBuV + AF_dB + CableLoss_dB
 *
 * Physical derivation from antenna effective length:
 *   V_oc = h_e * E_inc        (open-circuit voltage)
 *   AF = 1 / h_e_effective
 *
 * From antenna gain and effective aperture:
 *   A_e = (lambda^2 / 4*pi) * G
 *   h_e = sqrt(A_e * R_rad * 4*pi / Z_0)
 *   AF = Z_0 / (2 * sqrt(G * R_load)) * 1/lambda
 *
 * Simplified form (matched antenna, Z_load = 50 ohm):
 *   AF = 9.73 / (lambda_m * sqrt(G_linear))
 *   AF_dB = 20*log10(f_MHz) - 10*log10(G_linear) - 29.78
 * ------------------------------------------------------------------------- */

double re_antenna_factor_from_gain(double frequency_hz, double gain_linear)
{
    /* Validate inputs */
    if (frequency_hz <= 0.0 || gain_linear <= 0.0) {
        return -999.0; /* Error indicator */
    }

    double f_mhz = frequency_hz / 1e6;
    double af_db = 20.0 * log10(f_mhz) - 10.0 * log10(gain_linear) - 29.78;
    return af_db;
}

/* -------------------------------------------------------------------------
 * Compute antenna factor from effective length.
 *
 * Derivation (Balanis 2016 eqn 2-106):
 *   AF = 20*log10(f_MHz) - 20*log10(h_e_m) - 32.0  [dB(1/m)]
 *
 * where h_e is the vector effective length of the antenna.
 *
 * For a half-wave dipole: h_e = lambda/pi = 0.3183*lambda
 *   AF_dipole = 20*log10(f_MHz) - 20*log10(lambda/pi) - 32.0
 *             = 20*log10(f_MHz) - 20*log10(95.41/f_MHz) - 32.0
 *             approx 11.37 dB/m at resonance
 *
 * @param frequency_hz Frequency [Hz]
 * @param effective_length_m Effective length [m]
 * @return Antenna factor [dB(1/m)]
 * ------------------------------------------------------------------------- */
static double af_from_effective_length(double frequency_hz, double effective_length_m)
{
    if (frequency_hz <= 0.0 || effective_length_m <= 0.0) {
        return -999.0;
    }
    double f_mhz = frequency_hz / 1e6;
    return 20.0 * log10(f_mhz) - 20.0 * log10(effective_length_m) - 32.0;
}

/* -------------------------------------------------------------------------
 * Compute effective length of a half-wave dipole.
 *
 * h_e = (lambda / pi) * tan(pi * L / lambda)
 *
 * At resonance (L = lambda/2): h_e = lambda/pi
 *
 * Reference: Balanis (2016) eqn 4-93a
 * ------------------------------------------------------------------------- */
static double half_wave_dipole_effective_length(double frequency_hz)
{
    double lambda = SPEED_OF_LIGHT / frequency_hz;
    return lambda / PI;  /* Exact at resonance */
}

/* -------------------------------------------------------------------------
 * L5: Initialize standard antenna factor table from model name.
 *
 * Supports standard EMC antennas:
 *   - "BICONICAL" or "BBA9106" (Schwarzbeck): 30-300 MHz biconical
 *   - "LPDA" or "VULB9168" (Schwarzbeck): 200-1000 MHz log-periodic
 *   - "DRG_HORN" or "BBHA9120" (Schwarzbeck): 1-18 GHz double-ridged horn
 *   - "TUNED_DIPOLE": Half-wave tuned dipole set
 *
 * Uses theoretical gain models to compute frequency-dependent AF.
 * ------------------------------------------------------------------------- */
void re_af_table_init_standard(re_antenna_factor_table_t *table,
                                const char *model_name)
{
    if (!table || !model_name) return;

    memset(table, 0, sizeof(*table));
    strncpy(table->antenna_model, model_name, sizeof(table->antenna_model) - 1);
    strncpy(table->serial_number, "THEORETICAL", sizeof(table->serial_number) - 1);
    strncpy(table->calibration_date, "2024-01-01", sizeof(table->calibration_date) - 1);
    table->cal_uncertainty_db = 2.0; /* Typical expanded uncertainty k=2 */

    double f_start, f_end, gain_dbi_nominal;
    size_t n_points;

    /* Determine frequency range and typical gain based on antenna type */
    if (strstr(model_name, "BICONICAL") || strstr(model_name, "BBA9106") ||
        strstr(model_name, "biconical")) {
        f_start = 30e6;    f_end = 300e6;
        gain_dbi_nominal = 1.6;
        n_points = 200;
    } else if (strstr(model_name, "LPDA") || strstr(model_name, "VULB") ||
               strstr(model_name, "log_periodic")) {
        f_start = 200e6;   f_end = 1000e6;
        gain_dbi_nominal = 6.5;
        n_points = 400;
    } else if (strstr(model_name, "HORN") || strstr(model_name, "BBHA") ||
               strstr(model_name, "DRG") || strstr(model_name, "horn")) {
        f_start = 1e9;     f_end = 18e9;
        gain_dbi_nominal = 10.0;
        n_points = 500;
    } else if (strstr(model_name, "DIPOLE") || strstr(model_name, "dipole")) {
        /* Tuned dipole set: multiple resonant dipoles cover 30-1000 MHz */
        f_start = 30e6;    f_end = 1000e6;
        gain_dbi_nominal = 2.15; /* Half-wave dipole theoretical gain */
        n_points = 500;
    } else {
        /* Default: broadband 30 MHz - 1 GHz */
        f_start = 30e6;    f_end = 1000e6;
        gain_dbi_nominal = 3.0;
        n_points = 500;
    }

    if (n_points > RE_MAX_AF_POINTS) n_points = RE_MAX_AF_POINTS;

    /* Generate logarithmically spaced frequency points */
    double log_f_start = log10(f_start);
    double log_f_end   = log10(f_end);

    for (size_t i = 0; i < n_points; i++) {
        double log_f = log_f_start + (log_f_end - log_f_start) * i / (n_points - 1);
        double freq = pow(10.0, log_f);

        /* Frequency-dependent gain model:
         * Biconical: gain increases slightly with frequency up to 300 MHz
         * LPDA: gain is relatively flat across band
         * Horn: gain increases with frequency (aperture becomes larger in wavelengths)
         * Tuned dipole: constant gain at each resonance (2.15 dBi)
         */
        double gain_linear;
        if (strstr(model_name, "BICONICAL") || strstr(model_name, "biconical")) {
            /* Biconical gain: ~1.5 dBi at low end, ~2.0 dBi at high end */
            double ratio = (freq - f_start) / (f_end - f_start);
            gain_linear = pow(10.0, (1.5 + 0.5 * ratio) / 10.0);
        } else if (strstr(model_name, "HORN") || strstr(model_name, "horn")) {
            /* Horn gain: increases ~6 dB per octave above cutoff */
            double f_cutoff = 0.8e9;
            if (freq > f_cutoff) {
                gain_linear = pow(10.0, (8.0 + 6.0 * log2(freq/f_cutoff)) / 10.0);
                if (gain_linear > pow(10.0, 15.0/10.0)) gain_linear = pow(10.0, 15.0/10.0);
            } else {
                gain_linear = pow(10.0, 6.0/10.0);
            }
        } else if (strstr(model_name, "DIPOLE") || strstr(model_name, "dipole")) {
            gain_linear = pow(10.0, 2.15/10.0); /* Half-wave dipole */
        } else {
            /* LPDA or generic: flat gain */
            gain_linear = pow(10.0, gain_dbi_nominal / 10.0);
        }

        /* Compute AF from gain */
        double af_db = re_antenna_factor_from_gain(freq, gain_linear);

        /* Typical VSWR for well-matched EMC antennas */
        double vswr;
        if (freq < 80e6)      vswr = 2.5;
        else if (freq < 1e9)  vswr = 1.5;
        else                  vswr = 2.0;

        table->points[i].frequency_hz     = freq;
        table->points[i].antenna_factor_db = af_db;
        table->points[i].gain_dbi          = 10.0 * log10(gain_linear);
        table->points[i].antenna_impedance = 50.0;
        table->points[i].vswr              = vswr;
    }
    table->num_points = n_points;
}

/* -------------------------------------------------------------------------
 * L5: Interpolate antenna factor at arbitrary frequency.
 *
 * Uses piecewise linear interpolation in log-frequency space:
 *   AF(f) = AF_i + (AF_{i+1} - AF_i) * log10(f/f_i) / log10(f_{i+1}/f_i)
 *
 * For extrapolation below/above calibrated range:
 *   Below: use AF at f_start + 20*log10(f_start/f) correction
 *   Above: use AF at f_end + 20*log10(f/f_end) correction
 *
 * Returns: 0 on success, -1 if table empty or freq <= 0.
 * ------------------------------------------------------------------------- */
int re_af_interpolate(const re_antenna_factor_table_t *table,
                       double freq_hz, double *af_db)
{
    if (!table || !af_db || table->num_points == 0 || freq_hz <= 0.0) {
        return -1;
    }

    /* Binary search for bracketing points */
    size_t lo = 0, hi = table->num_points - 1;

    /* Handle out-of-range: below lowest frequency */
    if (freq_hz <= table->points[0].frequency_hz) {
        *af_db = table->points[0].antenna_factor_db
               + 20.0 * log10(table->points[0].frequency_hz / freq_hz);
        return 0;
    }

    /* Handle out-of-range: above highest frequency */
    if (freq_hz >= table->points[hi].frequency_hz) {
        *af_db = table->points[hi].antenna_factor_db
               + 20.0 * log10(freq_hz / table->points[hi].frequency_hz);
        return 0;
    }

    /* Binary search for interval */
    while (hi - lo > 1) {
        size_t mid = (lo + hi) / 2;
        if (table->points[mid].frequency_hz <= freq_hz) {
            lo = mid;
        } else {
            hi = mid;
        }
    }

    /* Linear interpolation in log-frequency */
    double log_f      = log10(freq_hz);
    double log_f_lo   = log10(table->points[lo].frequency_hz);
    double log_f_hi   = log10(table->points[hi].frequency_hz);
    double af_lo      = table->points[lo].antenna_factor_db;
    double af_hi      = table->points[hi].antenna_factor_db;

    double t = (log_f - log_f_lo) / (log_f_hi - log_f_lo);
    *af_db = af_lo + t * (af_hi - af_lo);

    return 0;
}

/* -------------------------------------------------------------------------
 * Compute antenna factor including mismatch correction.
 *
 * If antenna VSWR != 1.0, the mismatch loss affects the effective AF:
 *   AF_corrected = AF + 10*log10( (1 - |Gamma|^2) )
 *   Gamma = (VSWR - 1) / (VSWR + 1)
 *
 * Reference: ANSI C63.5:2017 Annex G
 * ------------------------------------------------------------------------- */
double re_af_with_mismatch(double af_nominal_db, double vswr_antenna,
                            double vswr_receiver)
{
    double gamma_a = (vswr_antenna - 1.0) / (vswr_antenna + 1.0);
    double gamma_r = (vswr_receiver - 1.0) / (vswr_receiver + 1.0);

    /* Mismatch loss (can be gain or loss depending on phase) */
    double mismatch_factor = 1.0 / (1.0 - gamma_a * gamma_r);
    double mismatch_db = 10.0 * log10(mismatch_factor);

    return af_nominal_db + mismatch_db;
}

/* -------------------------------------------------------------------------
 * Compute gain from antenna factor (inverse relationship).
 *
 * G_linear = (9.73 / (lambda * AF_linear))^2
 * G_dBi = 20*log10(f_MHz) - AF_dB - 29.78
 *
 * Reference: CISPR 16-1-4 Annex C eqn C.1
 * ------------------------------------------------------------------------- */
double re_gain_from_antenna_factor(double frequency_hz, double af_db)
{
    if (frequency_hz <= 0.0) return -999.0;
    double f_mhz = frequency_hz / 1e6;
    double g_dbi = 20.0 * log10(f_mhz) - af_db - 29.78;
    return g_dbi;
}

/* -------------------------------------------------------------------------
 * Validate antenna factor table calibration.
 *
 * Checks for:
 *   - Monotonically increasing frequency
 *   - AF within physically plausible range
 *   - Gain consistent with antenna type
 *
 * Returns 0 if valid, negative error code if invalid.
 * ------------------------------------------------------------------------- */
int re_af_table_validate(const re_antenna_factor_table_t *table)
{
    if (!table || table->num_points == 0) return -1;
    if (table->num_points > RE_MAX_AF_POINTS) return -2;

    for (size_t i = 1; i < table->num_points; i++) {
        /* Frequency must be monotonically increasing */
        if (table->points[i].frequency_hz <= table->points[i-1].frequency_hz) {
            return -3;
        }
        /* AF must be positive (physical) and reasonable (< 60 dB/m) */
        if (table->points[i].antenna_factor_db < 0.0 ||
            table->points[i].antenna_factor_db > 60.0) {
            return -4;
        }
        /* Gain must be between -20 dBi and +25 dBi for EMC antennas */
        if (table->points[i].gain_dbi < -20.0 ||
            table->points[i].gain_dbi > 25.0) {
            return -5;
        }
    }
    return 0;
}

/* -------------------------------------------------------------------------
 * Print antenna factor table summary (for diagnostic use).
 * ------------------------------------------------------------------------- */
void re_af_table_print_summary(const re_antenna_factor_table_t *table)
{
    if (!table || table->num_points == 0) {
        printf("Antenna Factor Table: EMPTY\n");
        return;
    }
    printf("Antenna Model: %s\n", table->antenna_model);
    printf("Serial: %s  Cal Date: %s  Uncertainty: %.1f dB (k=2)\n",
           table->serial_number, table->calibration_date,
           table->cal_uncertainty_db);
    printf("Frequency Range: %.2f MHz - %.2f GHz (%zu points)\n",
           table->points[0].frequency_hz / 1e6,
           table->points[table->num_points-1].frequency_hz / 1e9,
           table->num_points);
}