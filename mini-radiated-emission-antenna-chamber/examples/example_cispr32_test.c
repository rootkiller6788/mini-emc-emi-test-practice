/* =========================================================================
 * example_cispr32_test.c — CISPR 32 Radiated Emission Pre-Compliance Test
 *
 * L6: Complete end-to-end radiated emission measurement simulation.
 * L7: CISPR 32 Class B compliance testing for multimedia equipment.
 *
 * This example demonstrates:
 *   1. Setting up measurement configuration per CISPR 32
 *   2. Loading antenna factor tables for biconical and LPDA antennas
 *   3. Performing pre-scan and final measurement
 *   4. Evaluating pass/fail against Class B limits
 *   5. Generating a test report summary
 *
 * Reference: CISPR 32:2015, CISPR 16-2-3:2019
 * ========================================================================= */

#include "radiated_emission.h"
#include "antenna_types.h"
#include "chamber_theory.h"
#include "field_propagation.h"
#include "emission_limits.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

int main(void)
{
    printf("================================================\n");
    printf("  CISPR 32 Class B Radiated Emission Test\n");
    printf("  Multimedia Equipment - 3m Semi-Anechoic Chamber\n");
    printf("================================================\n\n");

    /* ---- Setup EUT ---- */
    re_eut_descriptor_t eut;
    memset(&eut, 0, sizeof(eut));
    strncpy(eut.name, "Example LED Display Monitor", sizeof(eut.name) - 1);
    eut.config_type = RE_EUT_TABLE_TOP;
    eut.op_state = RE_EUT_STATE_ACTIVE_TX;
    eut.base_height_m = 0.8;
    eut.cable_length_m = 2.0;
    eut.aux_equipment_present = 0;

    printf("EUT: %s\n", eut.name);
    printf("Configuration: Table-top at %.1fm height\n\n", eut.base_height_m);

    /* ---- Configure measurement per CISPR 32 ---- */
    re_measurement_config_t config;
    re_config_init_standard(&config, RE_STD_CISPR32, RE_SITE_SAC_3M);

    printf("Measurement Setup:\n");
    printf("  Distance: %.1f m\n", config.measurement_dist_m);
    printf("  RBW: %.1f kHz\n", config.rbw_hz / 1000.0);
    printf("  Detector: Quasi-Peak (below 1 GHz)\n");
    printf("  Antenna height scan: %.1f - %.1f m\n",
           config.antenna_height_min_m, config.antenna_height_max_m);
    printf("  Turntable: %.0f - %.0f degrees\n\n",
           config.turntable_angle_min_deg, config.turntable_angle_max_deg);

    /* ---- Load antenna factor table (biconical 30-300 MHz) ---- */
    re_antenna_factor_table_t af_biconical;
    re_af_table_init_standard(&af_biconical, "BICONICAL");
    printf("Antenna: %s\n", af_biconical.antenna_model);
    printf("  Frequency range: %.0f - %.0f MHz\n",
           af_biconical.points[0].frequency_hz / 1e6,
           af_biconical.points[af_biconical.num_points-1].frequency_hz / 1e6);
    printf("  Calibration points: %zu\n\n", af_biconical.num_points);

    /* ---- Perform emission scan (simulated) ---- */
    re_emission_scan_t scan;
    int n_emissions = re_emission_scan_perform(&scan, &eut, &config, &af_biconical);

    printf("Scan complete: %d critical frequencies identified\n\n", n_emissions);

    /* ---- Sort emissions by margin (worst first) ---- */
    re_emission_sort_by_margin(&scan);

    /* ---- Display top emissions ---- */
    printf("Top 5 Worst-Case Emissions:\n");
    printf("%-15s %-12s %-8s %-8s %-8s\n",
           "Freq (MHz)", "Level (dBuV/m)", "Limit", "Margin", "Result");
    printf("----------------------------------------------------\n");

    size_t n_show = scan.num_points < 5 ? scan.num_points : 5;
    for (size_t i = 0; i < n_show; i++) {
        const char *result_str = (scan.points[i].margin_db <= 0.0) ? "PASS" : "FAIL";
        printf("%-15.3f %-12.1f %-8.1f %-+8.1f %-8s\n",
               scan.points[i].frequency_hz / 1e6,
               scan.points[i].field_strength_dbuvm,
               scan.points[i].limit_dbuvm,
               scan.points[i].margin_db,
               result_str);
    }
    printf("\n");

    /* ---- Overall verdict ---- */
    printf("================================================\n");
    printf("OVERALL VERDICT: %s\n", scan.pass_fail ? "PASS" : "FAIL");
    printf("Worst margin: %+.1f dB at %.3f MHz\n",
           scan.worst_case_margin_db,
           scan.worst_case_freq_hz / 1e6);
    printf("================================================\n");

    return scan.pass_fail ? 0 : 1;
}