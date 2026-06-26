/* =========================================================================
 * field_strength.c — Electric Field Strength Computation from EMC Measurements
 *
 * L1: Field strength definition (V/m, dBuV/m)
 * L3: dB conversions, impedance relationships
 * L4: Friis transmission equation for field computation
 * L5: Cable loss compensation, preamplifier correction
 *
 * References:
 *   CISPR 16-2-3:2019 section 8.3 (Radiated emission measurement)
 *   ANSI C63.4:2014 section 8.2 (Field strength measurement)
 *   Paul (2006) EMC chapter 5
 * ========================================================================= */

#include "radiated_emission.h"
#include "antenna_types.h"
#include "field_propagation.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------
 * L1: Convert dBm to dBuV.
 *
 * Power in dBm: P_dBm = 10*log10(P / 1mW), P in watts
 * Voltage in dBuV: V_dBuV = 20*log10(V / 1uV), V in volts
 *
 * Given P = V^2 / R (RMS values):
 *   V = sqrt(P * R)
 *   V_dBuV = 20*log10(sqrt(P * R) / 1e-6)
 *          = 10*log10(P * R) + 120
 *          = 10*log10(P) + 10*log10(R) + 120
 *          = (P_dBm - 30) + 10*log10(R) + 120
 *          = P_dBm + 90 + 10*log10(R)
 *
 * For R = 50 ohm: V_dBuV = P_dBm + 90 + 16.9897 = P_dBm + 107.0
 * For R = 75 ohm: V_dBuV = P_dBm + 90 + 18.7506 = P_dBm + 108.8
 *
 * Reference: Agilent Application Note 57-2
 * ------------------------------------------------------------------------- */
double re_dbm_to_dbuv(double dbm, double impedance_ohm)
{
    if (impedance_ohm <= 0.0) {
        return -9999.0;
    }
    return dbm + 90.0 + 10.0 * log10(impedance_ohm);
}

/* -------------------------------------------------------------------------
 * Convert dBuV to dBm (inverse of above).
 *
 * P_dBm = V_dBuV - 90 - 10*log10(R)
 * ------------------------------------------------------------------------- */
static double dbuv_to_dbm(double dbuv, double impedance_ohm)
{
    if (impedance_ohm <= 0.0) {
        return -9999.0;
    }
    return dbuv - 90.0 - 10.0 * log10(impedance_ohm);
}

/* -------------------------------------------------------------------------
 * L3: Compute electric field strength from receiver reading.
 *
 * Complete measurement equation (CISPR 16-2-3 eqn 1):
 *
 *   E_dBuV/m = V_r_dBuV + AF_dB + C_loss_dB - G_preamp_dB + Delta_dB
 *
 * Where:
 *   V_r_dBuV  = receiver voltage reading in dBuV
 *   AF_dB     = antenna factor in dB(1/m)
 *   C_loss_dB = total cable and connector loss in dB
 *   G_preamp_dB = preamplifier gain in dB
 *   Delta_dB  = correction factors (mismatch, height, etc.)
 *
 * Derivation from first principles:
 *   E [V/m] = AF [1/m] * V_r [V] * 10^(C_loss/10) / 10^(G_preamp/10)
 *   In dB: E_dBuV/m = V_r_dBuV + AF_dB + C_loss_dB - G_preamp_dB
 * ------------------------------------------------------------------------- */
double re_field_strength_compute(double receiver_dbm, double af_db,
                                  double cable_loss_db, double preamp_gain_db,
                                  double impedance_ohm)
{
    /* Step 1: Convert receiver reading from dBm to dBuV */
    double v_dbuv = re_dbm_to_dbuv(receiver_dbm, impedance_ohm);

    /* Step 2: Apply measurement equation */
    double e_dbuvm = v_dbuv + af_db + cable_loss_db - preamp_gain_db;

    return e_dbuvm;
}

/* -------------------------------------------------------------------------
 * Convert field strength from dBuV/m to V/m.
 *
 * E_V/m = 10^(E_dBuV/m / 20) * 1e-6
 * ------------------------------------------------------------------------- */
static double dbuvm_to_vm(double e_dbuvm)
{
    return pow(10.0, e_dbuvm / 20.0) * 1e-6;
}

/* -------------------------------------------------------------------------
 * Convert field strength from V/m to dBuV/m.
 *
 * E_dBuV/m = 20*log10(E_V/m / 1e-6)
 * ------------------------------------------------------------------------- */
static double vm_to_dbuvm(double e_vm)
{
    if (e_vm <= 0.0) return -9999.0;
    return 20.0 * log10(e_vm / 1e-6);
}

/* -------------------------------------------------------------------------
 * Compute margin against limit.
 *
 * Positive margin = OVER LIMIT (FAIL)
 * Negative margin = UNDER LIMIT (PASS)
 * Zero margin = AT LIMIT (borderline)
 *
 * CISPR convention: margin = measured - limit
 * ------------------------------------------------------------------------- */
double re_compute_margin(double field_dbuvm, double limit_dbuvm)
{
    return field_dbuvm - limit_dbuvm;
}

/* -------------------------------------------------------------------------
 * Compute required receiver sensitivity for a given limit.
 *
 * To measure down to limit - 6 dB (pre-scan margin):
 *   Sensitivity = Limit - 6 dB - AF - CableLoss + PreampGain
 *
 * For 50 ohm system, noise figure NF:
 *   MDS_dBm = -174 + 10*log10(RBW) + NF
 *
 * Reference: Agilent Spectrum Analyzer Basics (AN 150)
 * ------------------------------------------------------------------------- */
double re_receiver_sensitivity_needed(double limit_dbuvm, double af_db,
                                       double cable_loss_db, double preamp_gain_db,
                                       double margin_db)
{
    return limit_dbuvm - margin_db - af_db - cable_loss_db + preamp_gain_db;
}

/* -------------------------------------------------------------------------
 * Compute minimum detectable field strength.
 *
 * Given receiver noise floor at RBW:
 *   E_min = DANL_dBm + AF_dB + CableLoss_dB - PreampGain_dB + 107 + SNR_dB
 *
 * where DANL = Displayed Average Noise Level in dBm/Hz
 *
 * Reference: Paul (2006) EMC section 5.6
 * ------------------------------------------------------------------------- */
double re_min_detectable_field(double danl_dbm_per_hz, double rbw_hz,
                                double af_db, double cable_loss_db,
                                double preamp_gain_db, double snr_required_db,
                                double impedance_ohm)
{
    /* Noise power in RBW bandwidth */
    double noise_dbm = danl_dbm_per_hz + 10.0 * log10(rbw_hz);

    /* Convert to dBuV */
    double noise_dbuv = re_dbm_to_dbuv(noise_dbm, impedance_ohm);

    /* Minimum field = noise floor + AF + cable - gain + SNR */
    double e_min_dbuvm = noise_dbuv + af_db + cable_loss_db
                        - preamp_gain_db + snr_required_db;

    return e_min_dbuvm;
}

/* -------------------------------------------------------------------------
 * Compute total measurement uncertainty budget.
 *
 * CISPR 16-4-2 identifies uncertainty contributions:
 *   u_AF      = antenna factor calibration uncertainty
 *   u_cable   = cable loss uncertainty
 *   u_receiver = receiver uncertainty
 *   u_mismatch = impedance mismatch uncertainty
 *   u_site    = site imperfection uncertainty
 *
 * Combined standard uncertainty (RSS):
 *   u_c = sqrt(sum(u_i^2))
 *
 * Expanded uncertainty (k=2, 95% confidence):
 *   U = 2 * u_c
 *
 * Typical U for 30-1000 MHz SAC measurement: 5.2 dB (CISPR 16-4-2)
 * ------------------------------------------------------------------------- */
double re_measurement_uncertainty(double u_af_db, double u_cable_db,
                                   double u_receiver_db, double u_mismatch_db,
                                   double u_site_db)
{
    double u_c_squared = u_af_db * u_af_db
                        + u_cable_db * u_cable_db
                        + u_receiver_db * u_receiver_db
                        + u_mismatch_db * u_mismatch_db
                        + u_site_db * u_site_db;

    double u_c = sqrt(u_c_squared);

    /* Expanded uncertainty k=2 */
    return 2.0 * u_c;
}

/* -------------------------------------------------------------------------
 * Apply measurement uncertainty to determine compliance.
 *
 * Per CISPR 16-4-2 decision rules:
 *   If E_measured + U_lab < Limit: PASS
 *   If E_measured - U_lab > Limit: FAIL
 *   Otherwise: INDETERMINATE (in the grey zone)
 *
 * Returns: -1 = PASS with confidence, 0 = INDETERMINATE, 1 = FAIL with confidence
 * ------------------------------------------------------------------------- */
int re_compliance_with_uncertainty(double e_dbuvm, double limit_dbuvm,
                                    double uncertainty_db)
{
    double upper = e_dbuvm + uncertainty_db;
    double lower = e_dbuvm - uncertainty_db;

    if (upper < limit_dbuvm) return -1;   /* Pass with 95% confidence */
    if (lower > limit_dbuvm) return 1;     /* Fail with 95% confidence */
    return 0;                               /* Indeterminate */
}

/* -------------------------------------------------------------------------
 * Compute the E-field from transmitter power using Friis formula.
 *
 * For far-field free-space conditions:
 *   E = sqrt(30 * P_tx * G_tx) / d    [V/m]
 *
 * In dB:
 *   E_dBuV/m = P_tx_dBm + G_tx_dBi - 20*log10(d_m) + 104.77
 *
 * Derivation:
 *   P_rx = P_tx * G_tx * G_rx * (lambda/(4*pi*d))^2
 *   E = sqrt(480*pi^2 * P_tx * G_tx) / (4*pi*d)
 *     = sqrt(30 * P_tx * G_tx) / d
 *
 * Reference: Balanis (2016) eqn 2-117
 * ------------------------------------------------------------------------- */
double re_field_from_transmitter(double power_tx_w, double gain_tx_linear,
                                  double distance_m)
{
    if (distance_m <= 0.0 || power_tx_w < 0.0 || gain_tx_linear <= 0.0) {
        return -9999.0;
    }

    double e_vm = sqrt(30.0 * power_tx_w * gain_tx_linear) / distance_m;

    return vm_to_dbuvm(e_vm);
}

/* -------------------------------------------------------------------------
 * Compute the required preamplifier gain for adequate measurement SNR.
 *
 * Desired SNR margin: typically 20 dB for reliable detection
 *
 * G_preamp_needed = Noise_dBuV + AF_dB + CableLoss_dB
 *                   - Limit_dBuV/m + Margin_dB + SNR_desired_dB
 * ------------------------------------------------------------------------- */
double re_preamp_gain_needed(double receiver_noise_dbuv, double af_db,
                              double cable_loss_db, double limit_dbuvm,
                              double margin_db, double snr_db)
{
    double v_required = limit_dbuvm - margin_db - af_db - cable_loss_db;
    return receiver_noise_dbuv - v_required + snr_db;
}

/* -------------------------------------------------------------------------
 * Compute spatial averaging of field strength over antenna height scan.
 *
 * Per CISPR 16-2-3, the max-hold value from height scan is the
 * reported emission level. This function simulates the scan by
 * computing field at multiple heights and returning statistics.
 *
 * @param e_readings Array of field readings at different heights
 * @param n_readings Number of readings
 * @param e_max      Output: maximum field
 * @param e_avg      Output: average field
 * @param e_min      Output: minimum field
 * ------------------------------------------------------------------------- */
void re_height_scan_statistics(const double *e_readings, size_t n_readings,
                                double *e_max, double *e_avg, double *e_min)
{
    if (!e_readings || n_readings == 0) {
        if (e_max) *e_max = 0.0;
        if (e_avg) *e_avg = 0.0;
        if (e_min) *e_min = 0.0;
        return;
    }

    double max_val = e_readings[0];
    double min_val = e_readings[0];
    double sum = e_readings[0];

    for (size_t i = 1; i < n_readings; i++) {
        if (e_readings[i] > max_val) max_val = e_readings[i];
        if (e_readings[i] < min_val) min_val = e_readings[i];
        sum += e_readings[i];
    }

    if (e_max) *e_max = max_val;
    if (e_min) *e_min = min_val;
    if (e_avg) *e_avg = sum / (double)n_readings;
}

/* -------------------------------------------------------------------------
 * Compute measurement dynamic range.
 *
 * Dynamic Range = E_max_measurable - E_min_detectable
 *
 * E_max is limited by receiver compression/overload
 * E_min is limited by noise floor + required SNR
 * ------------------------------------------------------------------------- */
double re_dynamic_range(double receiver_max_input_dbm,
                         double receiver_noise_floor_dbm,
                         double af_db, double cable_loss_db,
                         double preamp_gain_db, double impedance_ohm)
{
    double v_max_dbuv = re_dbm_to_dbuv(receiver_max_input_dbm, impedance_ohm);
    double v_min_dbuv = re_dbm_to_dbuv(receiver_noise_floor_dbm, impedance_ohm);

    double e_max = v_max_dbuv + af_db + cable_loss_db - preamp_gain_db;
    double e_min = v_min_dbuv + af_db + cable_loss_db - preamp_gain_db;

    return e_max - e_min;
}