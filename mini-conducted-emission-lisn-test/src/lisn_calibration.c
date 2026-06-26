/** @file lisn_calibration.c - LISN Calibration Procedures */
/** CISPR 16-1-2 Sec 5; ANSI C63.4 Annex B */
/** L1(defs), L2(cal concepts), L5(cal algorithms), L6(verification) */

#include "lisn_calibration.h"
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* L5: Measure Insertion Loss by comparing direct path to LISN path */
/* IL(dB) = 20*log10(V_direct / V_LISN) */
double lisn_cal_measure_insertion_loss(const lisn_network_model_t *m, double f) {
    if (!m || f <= 0.0) return 999.0;
    /* V_direct: signal generator -> 50 Ohm receiver (0 dB reference) */
    /* V_LISN: signal generator -> LISN -> 50 Ohm receiver */
    complex_z_t z = lisn_v_network_impedance(m, f);
    double z_mag = sqrt(z.real_ohm*z.real_ohm + z.imag_ohm*z.imag_ohm);
    /* V_LISN/V_direct = 50 / (Z_LISN + 50) [simplified model] */
    double ratio = 50.0 / (z_mag + 50.0);
    if (ratio <= 0.0) return 999.0;
    return -20.0 * log10(ratio);
}

/* L5: Verify LISN impedance against CISPR 16-1-2 tolerance */
/* Checks |Z| within +/-20% and phase within +/-11.5 degrees */
void lisn_cal_verify_impedance(const lisn_network_model_t *m, double f,
                                lisn_impedance_verification_t *result) {
    if (!m || !result || f <= 0.0) return;
    complex_z_t z = lisn_v_network_impedance(m, f);
    result->frequency_hz = f;
    result->measured_z_ohm = sqrt(z.real_ohm*z.real_ohm + z.imag_ohm*z.imag_ohm);
    result->nominal_z_ohm = 50.0;
    result->measured_phase_deg = atan2(z.imag_ohm, z.real_ohm) * 180.0 / M_PI;
    /* Nominal phase for 50Ohm||50uH at frequency f */
    double w = 2.0 * M_PI * f;
    double XL = w * 50.0e-6;
    result->nominal_phase_deg = atan(50.0 / XL) * 180.0 / M_PI;
    result->deviation_pct = (result->measured_z_ohm - 50.0) / 50.0 * 100.0;
    result->phase_deviation_deg = result->measured_phase_deg - result->nominal_phase_deg;
    result->impedance_pass = (fabs(result->deviation_pct) <= 20.0) ? 1 : 0;
    result->phase_pass = (fabs(result->phase_deviation_deg) <= 11.5) ? 1 : 0;
}

/* L5: Port-to-port isolation measurement */
/* Isolation > 30 dB is generally required between L and N measurement ports */
double lisn_cal_measure_isolation(const lisn_network_model_t *m, double f) {
    if (!m || f <= 0.0) return 0.0;
    /* Simplified: isolation degrades with frequency due to parasitic coupling */
    /* Real LISNs achieve 40-60 dB at low freq, dropping to 20-30 dB at 30 MHz */
    double base_iso = 60.0;
    double freq_mhz = f / 1.0e6;
    double degradation = 20.0 * log10(freq_mhz / 0.15);  /* ref 150 kHz */
    double iso = base_iso - degradation;
    return (iso > 10.0) ? iso : 10.0;  /* minimum 10 dB */
}

/* L5: Voltage Division Factor calibration */
/* VDF = V_RF_out / V_EUT_CM, ideally 1.0 (0 dB) */
double lisn_cal_voltage_division_factor(const lisn_network_model_t *m, double f) {
    if (!m || f <= 0.0) return 0.0;
    complex_z_t z = lisn_v_network_impedance(m, f);
    double z_mag = sqrt(z.real_ohm*z.real_ohm + z.imag_ohm*z.imag_ohm);
    /* VDF = 50 / (Z_LISN + 50), in dB: 20*log10(VDF) */
    double vdf_linear = 50.0 / (z_mag + 50.0);
    if (vdf_linear <= 0.0) return -999.0;
    return 20.0 * log10(vdf_linear);
}

/* L5: Generate full calibration table at N log-spaced frequencies */
void lisn_cal_generate_table(const lisn_network_model_t *m, double fs, double fe,
                              int n, lisn_calibration_t *cal) {
    if (!m || !cal || n <= 0 || fs <= 0.0 || fe <= fs) return;
    cal->num_points = n;
    cal->points = malloc(n * sizeof(lisn_cal_point_t));
    if (!cal->points) return;
    double ratio = pow(fe / fs, 1.0 / (double)(n - 1));
    double freq = fs;
    int i;
    for (i = 0; i < n; i++) {
        cal->points[i].frequency_hz = freq;
        cal->points[i].vdf_db = lisn_cal_voltage_division_factor(m, freq);
        cal->points[i].insertion_loss_db = lisn_cal_measure_insertion_loss(m, freq);
        cal->points[i].correction_factor_db = cal->points[i].vdf_db + cal->points[i].insertion_loss_db;
        freq *= ratio;
    }
    cal->overall_correction_db = cal->points[n/2].correction_factor_db;
    cal->calibration_uncertainty_db = 0.5;
}

/* L5: Longitudinal Conversion Loss - CM rejection of LISN */
/* LCL > 26 dB required per CISPR 16-1-2 */
double lisn_cal_lcl(const lisn_network_model_t *m, double f) {
    if (!m || f <= 0.0) return 0.0;
    /* LCL degrades with frequency due to asymmetry in LISN construction */
    double base_lcl = 46.0;  /* typical at 150 kHz */
    double freq_mhz = f / 1.0e6;
    double lcl = base_lcl - 20.0 * log10(freq_mhz / 0.15);
    return (lcl > 10.0) ? lcl : 10.0;
}

/* L5: Total correction factor = VDF + IL + cable_loss */
double lisn_cal_correction_factor(const lisn_calibration_t *cal, double f, double cable) {
    if (!cal || !cal->points || cal->num_points <= 0) return cable;
    /* Find nearest calibration frequency */
    double best_vdf = cal->points[0].vdf_db;
    double best_il = cal->points[0].insertion_loss_db;
    double min_diff = fabs(f - cal->points[0].frequency_hz);
    int i;
    for (i = 1; i < cal->num_points; i++) {
        double diff = fabs(f - cal->points[i].frequency_hz);
        if (diff < min_diff) {
            min_diff = diff;
            best_vdf = cal->points[i].vdf_db;
            best_il = cal->points[i].insertion_loss_db;
        }
    }
    return best_vdf + best_il + cable;
}

/* Free calibration data */
void lisn_calibration_free(lisn_calibration_t *cal) {
    if (!cal) return;
    if (cal->points) { free(cal->points); cal->points = NULL; }
    cal->num_points = 0;
}
