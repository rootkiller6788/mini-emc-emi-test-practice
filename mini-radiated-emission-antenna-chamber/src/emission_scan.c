/* =========================================================================
 * emission_scan.c — Radiated Emission Scanning Algorithms
 *
 * L1: Pre-scan, final measurement, max-hold
 * L5: Max-hold algorithm, height scan optimization, turntable optimization
 * L6: CISPR 16-2-3 measurement procedure
 * L7: Application to CISPR 11, CISPR 32, FCC Part 15 testing
 *
 * References:
 *   CISPR 16-2-3:2019 section 7 (Radiated emission measurement)
 *   ANSI C63.4:2014 section 8 (Radiated emission measurements)
 *   Paul (2006) EMC chapter 5
 * ========================================================================= */

#include "radiated_emission.h"
#include "antenna_types.h"
#include "chamber_theory.h"
#include "field_propagation.h"
#include "emission_limits.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* -------------------------------------------------------------------------
 * L1: Initialize measurement configuration from standard.
 *
 * Each standard specifies default measurement parameters:
 * - CISPR 11: 10m/3m, 30-1000 MHz, QP detector, RBW=120 kHz
 * - CISPR 22/32: 3m/10m, 30-6000 MHz, QP+Avg, RBW=120 kHz/<1GHz, 1 MHz/>1GHz
 * - FCC Part 15: 3m, 30-40000 MHz, QP+Avg
 * - MIL-STD-461: 1m, 10 kHz-18 GHz, Peak, RBW varies
 * - CISPR 25: 1m, 150 kHz-2.5 GHz, Peak+Avg, RBW varies
 * ------------------------------------------------------------------------- */
void re_config_init_standard(re_measurement_config_t *config,
                              re_standard_t standard, re_site_type_t site)
{
    if (!config) return;
    memset(config, 0, sizeof(*config));

    config->site_type = site;
    config->antenna_height_min_m = 1.0;
    config->antenna_height_max_m = 4.0;
    config->turntable_angle_min_deg = 0.0;
    config->turntable_angle_max_deg = 360.0;
    config->ground_plane_reflection_coeff = 0.9;
    config->use_preamp = 0;
    config->preamp_gain_db = 0.0;
    config->cable_loss_db = 2.0; /* Typical cable loss */

    switch (standard) {
    case RE_STD_CISPR11:
    case RE_STD_EN55011:
        config->measurement_dist_m = (site == RE_SITE_SAC_10M) ? 10.0 : 3.0;
        config->rbw_hz = 120e3;  /* CISPR band C/D */
        config->measurement_time_ms = 100.0;
        config->detector = RE_DETECTOR_QUASI_PEAK;
        break;

    case RE_STD_CISPR22:
        config->measurement_dist_m = (site == RE_SITE_SAC_10M) ? 10.0 : 3.0;
        config->rbw_hz = 120e3;
        config->measurement_time_ms = 100.0;
        config->detector = RE_DETECTOR_QUASI_PEAK;
        break;

    case RE_STD_CISPR32:
    case RE_STD_EN55032:
        config->measurement_dist_m = 3.0;
        config->rbw_hz = 120e3;   /* Below 1 GHz */
        config->measurement_time_ms = 100.0;
        config->detector = RE_DETECTOR_QUASI_PEAK;
        break;

    case RE_STD_FCC_PART15:
        config->measurement_dist_m = 3.0;
        config->rbw_hz = 120e3;   /* Below 1 GHz */
        config->measurement_time_ms = 100.0;
        config->detector = RE_DETECTOR_QUASI_PEAK;
        break;

    case RE_STD_MIL_STD_461:
        config->measurement_dist_m = 1.0;
        config->rbw_hz = 1e3;     /* 10 kHz - 30 MHz */
        config->measurement_time_ms = 50.0;
        config->detector = RE_DETECTOR_PEAK;
        config->antenna_height_min_m = 1.0;
        config->antenna_height_max_m = 1.2; /* Fixed height for MIL-STD */
        config->use_preamp = 1;
        config->preamp_gain_db = 25.0;
        break;

    case RE_STD_CISPR25:
        config->measurement_dist_m = 1.0;
        config->rbw_hz = 120e3;
        config->measurement_time_ms = 50.0;
        config->detector = RE_DETECTOR_PEAK;
        break;

    default:
        config->measurement_dist_m = 3.0;
        config->rbw_hz = 120e3;
        config->measurement_time_ms = 100.0;
        config->detector = RE_DETECTOR_QUASI_PEAK;
        break;
    }
}

/* -------------------------------------------------------------------------
 * L5: Pre-scan — coarse frequency sweep with peak detector.
 *
 * The pre-scan identifies emission peaks rapidly:
 * 1. Sweep frequency range with peak detector (fast)
 * 2. Record all peaks above (limit - margin_db_threshold)
 * 3. Sort peaks by margin (most critical first)
 * 4. Return list of critical frequencies for final measurement
 *
 * CISPR 16-2-3 recommends a 6 dB threshold for re-measurement.
 *
 * This function simulates a pre-scan by evaluating field strength
 * at discrete frequencies and identifying those near/above limits.
 *
 * @param scan        Output pre-scan result
 * @param eut         EUT descriptor
 * @param config      Measurement configuration
 * @param af_tbl      Antenna factor table
 * @param margin_db   Threshold margin (typically 6 dB below limit)
 * @return Number of critical frequencies found
 * ------------------------------------------------------------------------- */
static int pre_scan(re_emission_scan_t *scan,
                     const re_eut_descriptor_t *eut,
                     const re_measurement_config_t *config,
                     const re_antenna_factor_table_t *af_tbl,
                     double margin_db)
{
    if (!scan || !config || !af_tbl) return 0;

    /* Frequency range for pre-scan */
    double f_start = 30e6;
    double f_end   = 1000e6;
    double f_step_factor = 1.02; /* 2% frequency step (logarithmic) */

    size_t count = 0;
    double freq = f_start;

    while (freq <= f_end && count < RE_MAX_SCAN_POINTS) {
        /* Interpolate antenna factor at this frequency */
        double af_db = 0.0;
        if (re_af_interpolate(af_tbl, freq, &af_db) != 0) {
            af_db = 10.0; /* Default fallback */
        }

        /* Simulated emission level (in real measurement, this comes from receiver) */
        /* Model: EUT emission = base emission + frequency-dependent variation */
        double base_emission_dbuvm = 30.0; /* Example: 30 dBuV/m baseline */
        double freq_factor = 20.0 * log10(freq / 30e6); /* Increasing with freq */
        double simulated_emission = base_emission_dbuvm + freq_factor * 0.3;

        /* Add height-dependent variation (simulated two-ray interference) */
        double h_rx = config->antenna_height_min_m;
        double height_factor = 0.0;
        for (int h_step = 0; h_step < 10; h_step++) {
            double h = config->antenna_height_min_m +
                      (config->antenna_height_max_m - config->antenna_height_min_m)
                      * h_step / 9.0;
            double nsa_var = ct_nsa_theoretical(config->measurement_dist_m,
                                                 1.0, h, freq,
                                                 RE_POL_HORIZONTAL);
            /* Convert NSA variation to field variation */
            if (h_step == 0) height_factor = nsa_var;
            else {
                double diff = nsa_var - height_factor;
                if (diff > height_factor || h_step == 1) {
                    /* Track max variation */
                }
            }
        }

        /* Apply the height scan maximum (simplified) */
        double e_field_dbuvm = simulated_emission;

        /* Get limit at this frequency */
        double limit = el_get_standard_limit(config->site_type <= RE_SITE_OATS ?
                                              RE_STD_CISPR32 : RE_STD_FCC_PART15,
                                              freq, config->detector,
                                              config->measurement_dist_m, 1);

        /* Check if emission is near or above limit */
        double margin = e_field_dbuvm - limit;

        if (margin > -margin_db) {
            /* Critical emission: store for final measurement */
            scan->points[count].frequency_hz        = freq;
            scan->points[count].field_strength_dbuvm = e_field_dbuvm;
            scan->points[count].field_strength_vm    = pow(10.0, e_field_dbuvm/20.0) * 1e-6;
            scan->points[count].limit_dbuvm          = limit;
            scan->points[count].margin_db            = margin;
            scan->points[count].dominant_pol         = RE_POL_HORIZONTAL;
            scan->points[count].antenna_height_m     = 2.0;
            scan->points[count].turntable_angle_deg  = 0.0;
            scan->points[count].detector             = config->detector;
            count++;
        }

        /* Next frequency (logarithmic step) */
        freq *= f_step_factor;
    }

    scan->num_points = count;
    return (int)count;
}

/* -------------------------------------------------------------------------
 * L5: Max-hold emission search over antenna height.
 *
 * For each critical frequency identified in pre-scan, perform
 * a height scan to find the maximum emission.
 *
 * Physical model: two-ray interference creates standing wave pattern
 * in vertical plane. Maxima occur at heights where direct and
 * reflected paths add constructively.
 *
 * @param freq_hz     Frequency [Hz]
 * @param config      Measurement configuration
 * @param af_db       Antenna factor [dB(1/m)]
 * @param best_height Output: height for max emission [m]
 * @param max_field   Output: max field strength [dBuV/m]
 * ------------------------------------------------------------------------- */
static void height_scan_max_hold(double freq_hz,
                                  const re_measurement_config_t *config,
                                  double af_db,
                                  double *best_height,
                                  double *max_field)
{
    if (!best_height || !max_field) return;

    double max_found = -999.0;
    double best_h = 1.0;

    /* Scan heights from min to max in 50 steps */
    int n_steps = 50;
    for (int i = 0; i <= n_steps; i++) {
        double h = config->antenna_height_min_m +
                  (config->antenna_height_max_m - config->antenna_height_min_m)
                  * (double)i / (double)n_steps;

        /* Compute field at this height using two-ray model */
        double nsa = ct_nsa_theoretical(config->measurement_dist_m,
                                         1.0, h, freq_hz,
                                         RE_POL_HORIZONTAL);

        /* Convert NSA to field variation
         * (lower NSA means more signal, higher field) */
        double field = 0.0 - nsa; /* Simplified model */

        if (field > max_found) {
            max_found = field;
            best_h = h;
        }
    }

    *best_height = best_h;
    *max_field = max_found;
}

/* -------------------------------------------------------------------------
 * L5+L6: Full emission scan per CISPR 16-2-3 procedure.
 *
 * Complete measurement flow:
 *   1. Pre-scan with peak detector (all frequencies)
 *   2. Identify critical frequencies (<6 dB margin)
 *   3. For each critical frequency:
 *      a. Height scan to maximize emission
 *      b. Turntable rotation to find max azimuth
 *      c. Final measurement with appropriate detector
 *   4. Compute pass/fail verdict
 *
 * This implementation simulates the measurement using
 * theoretical models for demonstration.
 * ------------------------------------------------------------------------- */
int re_emission_scan_perform(re_emission_scan_t *scan,
                              const re_eut_descriptor_t *eut,
                              const re_measurement_config_t *config,
                              const re_antenna_factor_table_t *af_tbl)
{
    if (!scan || !eut || !config || !af_tbl) return -1;

    memset(scan, 0, sizeof(*scan));

    if (eut->name[0]) {
        strncpy(scan->eut_name, eut->name, sizeof(scan->eut_name) - 1);
    } else {
        strncpy(scan->eut_name, "UNKNOWN_EUT", sizeof(scan->eut_name) - 1);
    }

    snprintf(scan->test_standard, sizeof(scan->test_standard),
             "CISPR 16-2-3");
    memcpy(&scan->config, config, sizeof(*config));

    /* Phase 1: Pre-scan */
    int n_critical = pre_scan(scan, eut, config, af_tbl, 6.0);

    /* Phase 2: Final measurement at critical frequencies */
    scan->worst_case_margin_db = 999.0;

    for (size_t i = 0; i < scan->num_points && i < RE_MAX_SCAN_POINTS; i++) {
        double freq = scan->points[i].frequency_hz;

        /* Interpolate antenna factor */
        double af_db;
        if (re_af_interpolate(af_tbl, freq, &af_db) != 0) {
            af_db = 10.0;
        }

        /* Optimize antenna height */
        double best_h, max_field;
        height_scan_max_hold(freq, config, af_db, &best_h, &max_field);

        scan->points[i].antenna_height_m = best_h;

        /* Simulated final measurement with correct detector */
        /* QP detector reads ~2-4 dB below peak for impulsive noise */
        double detector_correction = 0.0;
        if (config->detector == RE_DETECTOR_QUASI_PEAK) {
            detector_correction = -3.0; /* Typical QP correction for broadband */
        } else if (config->detector == RE_DETECTOR_AVERAGE) {
            detector_correction = -8.0; /* Typical avg correction for pulses */
        }

        double final_field = scan->points[i].field_strength_dbuvm
                             + detector_correction;

        scan->points[i].field_strength_dbuvm = final_field;
        scan->points[i].field_strength_vm = pow(10.0, final_field/20.0) * 1e-6;

        /* Re-compute margin */
        double limit = el_get_standard_limit(RE_STD_CISPR32, freq,
                                              config->detector,
                                              config->measurement_dist_m, 1);
        scan->points[i].limit_dbuvm = limit;
        scan->points[i].margin_db    = final_field - limit;

        /* Track worst case */
        if (scan->points[i].margin_db < scan->worst_case_margin_db) {
            scan->worst_case_margin_db = scan->points[i].margin_db;
            scan->worst_case_freq_hz   = freq;
        }
    }

    /* Phase 3: Pass/fail evaluation */
    scan->pass_fail = re_emission_evaluate_pass_fail(scan);

    return (int)scan->num_points;
}

/* -------------------------------------------------------------------------
 * L1: Evaluate pass/fail from scan.
 *
 * Per CISPR 16-2-3:
 *   PASS if all emissions < limit (considering measurement uncertainty)
 *   FAIL if any emission > limit
 *
 * MIL-STD-461 allows up to 3 non-critical narrowband exceedances.
 * ------------------------------------------------------------------------- */
int re_emission_evaluate_pass_fail(const re_emission_scan_t *scan)
{
    if (!scan || scan->num_points == 0) return 1; /* No emissions = pass */

    int fail_count = 0;

    for (size_t i = 0; i < scan->num_points; i++) {
        if (scan->points[i].margin_db > 0.0) {
            fail_count++;
        }
    }

    return (fail_count == 0) ? 1 : 0;
}

/* -------------------------------------------------------------------------
 * L5: Sort emission points by margin (worst first).
 *
 * Implements quicksort to identify most critical emissions.
 * ------------------------------------------------------------------------- */
static int compare_margin(const void *a, const void *b)
{
    const re_emission_point_t *pa = (const re_emission_point_t *)a;
    const re_emission_point_t *pb = (const re_emission_point_t *)b;

    /* Sort by margin descending (worst first) */
    if (pa->margin_db > pb->margin_db) return -1;
    if (pa->margin_db < pb->margin_db) return 1;
    return 0;
}

void re_emission_sort_by_margin(re_emission_scan_t *scan)
{
    if (!scan || scan->num_points <= 1) return;

    qsort(scan->points, scan->num_points, sizeof(re_emission_point_t),
          compare_margin);

    /* Update worst case after sort */
    if (scan->num_points > 0) {
        scan->worst_case_margin_db = scan->points[0].margin_db;
        scan->worst_case_freq_hz   = scan->points[0].frequency_hz;
    }
}

/* -------------------------------------------------------------------------
 * L7: Generate test report summary.
 *
 * Output format per ISO/IEC 17025 test report requirements.
 * ------------------------------------------------------------------------- */
void re_emission_report_summary(const re_emission_scan_t *scan, FILE *out)
{
    if (!scan || !out) return;

    fprintf(out, "===========================================\n");
    fprintf(out, "RADIATED EMISSION TEST REPORT\n");
    fprintf(out, "===========================================\n");
    fprintf(out, "EUT: %s\n", scan->eut_name);
    fprintf(out, "Standard: %s\n", scan->test_standard);
    fprintf(out, "Measurement distance: %.1f m\n", scan->config.measurement_dist_m);
    fprintf(out, "Detector: %d\n", (int)scan->config.detector);
    fprintf(out, "RBW: %.1f kHz\n", scan->config.rbw_hz / 1000.0);
    fprintf(out, "-------------------------------------------\n");

    if (scan->num_points == 0) {
        fprintf(out, "No emissions detected above noise floor.\n");
    } else {
        fprintf(out, "Frequency (MHz)  | Level (dBuV/m) | Limit | Margin | Result\n");
        fprintf(out, "-----------------|----------------|-------|--------|-------\n");
        for (size_t i = 0; i < scan->num_points && i < 20; i++) {
            fprintf(out, "%14.3f  | %12.1f  | %5.1f | %+5.1f | %s\n",
                    scan->points[i].frequency_hz / 1e6,
                    scan->points[i].field_strength_dbuvm,
                    scan->points[i].limit_dbuvm,
                    scan->points[i].margin_db,
                    scan->points[i].margin_db <= 0.0 ? "PASS" : "FAIL");
        }
        if (scan->num_points > 20) {
            fprintf(out, "... (%zu more emissions)\n",
                    scan->num_points - 20);
        }
    }

    fprintf(out, "-------------------------------------------\n");
    fprintf(out, "Worst margin: %+.1f dB at %.3f MHz\n",
            scan->worst_case_margin_db,
            scan->worst_case_freq_hz / 1e6);
    fprintf(out, "Overall verdict: %s\n",
            scan->pass_fail ? "PASS" : "FAIL");
    fprintf(out, "===========================================\n");
}