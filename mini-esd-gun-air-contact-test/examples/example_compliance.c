/**
 * @file example_compliance.c
 * @brief Example: Full IEC 61000-4-2 compliance test simulation
 *
 * L6 Canonical Problem: Demonstrates a complete ESD compliance
 * test from plan creation through execution, statistical analysis,
 * and cross-standard equivalence mapping.
 *
 * L7 Application: Automotive and medical device compliance.
 */

#include "esd_waveform.h"
#include "esd_test_setup.h"
#include "esd_compliance.h"
#include "esd_tlp.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>

int main(void)
{
    printf("╔═══════════════════════════════════════════════╗\n");
    printf("║  ESD Compliance Test Simulation              ║\n");
    printf("║  IEC 61000-4-2 / ISO 10605 / HBM             ║\n");
    printf("╚═══════════════════════════════════════════════╝\n\n");

    /* ─── Part 1: IEC 61000-4-2 Compliance ────────────────── */
    printf("═══ Part 1: IEC 61000-4-2 Compliance ═══\n\n");

    /* Create a standard test plan */
    esd_test_plan_t plan;
    memset(&plan, 0, sizeof(plan));
    int ret = esd_test_plan_standard(ESD_DISCHARGE_CONTACT, ESD_LEVEL_4, &plan);
    if (ret != 0) {
        printf("ERROR: Failed to create test plan\n");
        return 1;
    }

    printf("Test Plan Summary:\n");
    printf("  Standard:            IEC 61000-4-2:2008\n");
    printf("  Discharge type:      Contact\n");
    printf("  Max severity:        Level 4 (8 kV)\n");
    printf("  Test points:         %zu\n", plan.num_test_points);
    printf("  Levels tested:       %zu\n", plan.num_levels);
    printf("  Discharges/point:    %d\n", plan.discharges_per_point);
    printf("  Both polarities:     %s\n",
           plan.test_both_polarities ? "Yes" : "No");

    size_t total = esd_test_plan_total_discharges(&plan);
    double duration = esd_test_plan_duration(&plan);
    printf("  Total discharges:    %zu\n", total);
    printf("  Est. duration:       %.1f minutes\n", duration);

    /* Verify plan compliance */
    int plan_ok = esd_compliance_verify_plan(&plan, ESD_LEVEL_4);
    printf("  IEC compliant:       %s\n", plan_ok ? "YES ✓" : "NO ✗");
    printf("\n");

    /* Test points listing */
    printf("Test Points:\n");
    for (size_t i = 0; i < plan.num_test_points; i++) {
        const char *type_str;
        switch (plan.test_points[i].type) {
        case ESD_POINT_EUT_DIRECT:   type_str = "EUT Direct"; break;
        case ESD_POINT_HCP_INDIRECT: type_str = "HCP Indirect"; break;
        case ESD_POINT_VCP_INDIRECT: type_str = "VCP Indirect"; break;
        default:                      type_str = "Other"; break;
        }
        printf("  [%d] %s — %s (%.2f, %.2f, %.2f)m\n",
               plan.test_points[i].point_id,
               type_str,
               plan.test_points[i].description,
               plan.test_points[i].x_m,
               plan.test_points[i].y_m,
               plan.test_points[i].z_m);
    }
    printf("\n");

    /* ─── Part 2: Simulate test execution ─────────────────── */
    printf("═══ Part 2: Simulated Test Execution ═══\n\n");

    /* Initialize test report */
    esd_test_report_t report;
    memset(&report, 0, sizeof(report));
    ret = esd_test_report_init(&plan, &report, "EUT-MODEL-X", "T. Engineer");
    if (ret != 0) {
        printf("ERROR: Failed to initialize test report\n");
        esd_test_plan_free(&plan);
        return 1;
    }

    /* Seed random for demonstration */
    srand(42);

    /* Simulate performing all discharges */
    int discharge_count = 0;
    for (size_t pt = 0; pt < plan.num_test_points; pt++) {
        for (size_t lvl = 0; lvl < plan.num_levels; lvl++) {
            for (int pol = 0; pol < (plan.test_both_polarities ? 2 : 1); pol++) {
                for (int d = 0; d < plan.discharges_per_point; d++) {
                    esd_discharge_record_t rec;
                    memset(&rec, 0, sizeof(rec));
                    rec.point_id = plan.test_points[pt].point_id;
                    rec.voltage_kv = esd_level_voltage_kv(plan.levels[lvl],
                                                           plan.discharge_type);
                    rec.type = plan.discharge_type;
                    rec.polarity_positive = pol;

                    /* Simulate realistic test outcome:
                     * At Level 4, higher probability of failure */
                    int r = rand() % 100;
                    if (plan.levels[lvl] == ESD_LEVEL_4 && r < 5) {
                        rec.result = ESD_RESULT_FAIL_D;
                        snprintf(rec.notes, sizeof(rec.notes),
                                 "System reset observed at 8 kV");
                    } else if (plan.levels[lvl] == ESD_LEVEL_4 && r < 15) {
                        rec.result = ESD_RESULT_PASS_C;
                        snprintf(rec.notes, sizeof(rec.notes),
                                 "Temporary display flicker, self-recovered");
                    } else if (r < 5) {
                        rec.result = ESD_RESULT_PASS_B;
                        snprintf(rec.notes, sizeof(rec.notes),
                                 "Minor glitch, self-recovered");
                    } else {
                        rec.result = ESD_RESULT_PASS_A;
                        snprintf(rec.notes, sizeof(rec.notes),
                                 "No effect observed");
                    }

                    rec.peak_current_a = rec.voltage_kv * 3.75 * (0.9 + 0.2 * ((double)rand() / RAND_MAX));
                    rec.rise_time_ns = 0.8 * (0.8 + 0.4 * ((double)rand() / RAND_MAX));

                    esd_test_report_record(&report, &rec);
                    discharge_count++;
                }
            }
        }
    }

    printf("Executed %d discharges.\n\n", discharge_count);

    /* Statistical analysis */
    size_t na, nb, nc, nd;
    esd_test_report_statistics(&report, &na, &nb, &nc, &nd);

    printf("Test Results:\n");
    printf("  Performance A (normal):     %zu (%.1f%%)\n",
           na, 100.0 * na / report.num_records);
    printf("  Performance B (temporary):  %zu (%.1f%%)\n",
           nb, 100.0 * nb / report.num_records);
    printf("  Performance C (intervention): %zu (%.1f%%)\n",
           nc, 100.0 * nc / report.num_records);
    printf("  Performance D (FAILURE):    %zu (%.1f%%)\n",
           nd, 100.0 * nd / report.num_records);
    printf("  Overall:                    %s\n",
           esd_test_report_overall_pass(&report) ? "PASS ✓" : "FAIL ✗");
    printf("\n");

    /* ─── Part 3: Compliance Margin Analysis ──────────────── */
    printf("═══ Part 3: Compliance Margin ═══\n\n");

    esd_compliance_margin_t margin;
    esd_compliance_margin_compute(12.0, 8.0, 1.5, &margin);

    printf("Failure threshold:   12.0 kV\n");
    printf("Required level:      8.0 kV (Level 4)\n");
    printf("Margin ratio:        %.2fx\n", margin.margin_ratio);
    printf("Margin (dB):         %.1f dB\n", margin.margin_db);
    printf("Passes (margin≥1.5x): %s\n", margin.passes ? "YES ✓" : "NO ✗");
    printf("\n");

    /* ─── Part 4: Cross-Standard Equivalence ──────────────── */
    printf("═══ Part 4: Cross-Standard Equivalence ═══\n\n");

    esd_standard_params_t iec  = esd_standard_get(ESD_STD_IEC_61000_4_2);
    esd_standard_params_t hbm  = esd_standard_get(ESD_STD_ANSI_ESDA_JS001);
    esd_standard_params_t iso  = esd_standard_get(ESD_STD_ISO_10605);
    esd_standard_params_t aec  = esd_standard_get(ESD_STD_AEC_Q100);
    esd_standard_params_t mil  = esd_standard_get(ESD_STD_MIL_STD_883);
    esd_standard_params_t tele = esd_standard_get(ESD_STD_GR_1089_CORE);

    printf("%-30s %6s %6s %6s\n", "Standard", "Cs(pF)", "Rd(Ω)", "Vmax");
    printf("──────────────────────────────────────────────────\n");
    printf("%-30s %6.0f %6.0f %5.0f kV\n",
           iec.name, iec.cs_pf, iec.rd_ohm, iec.max_voltage_kv);
    printf("%-30s %6.0f %6.0f %5.0f kV\n",
           iso.name, iso.cs_pf, iso.rd_ohm, iso.max_voltage_kv);
    printf("%-30s %6.0f %6.0f %5.0f V\n",
           hbm.name, hbm.cs_pf, hbm.rd_ohm, hbm.max_voltage_kv);
    printf("%-30s %6.0f %6.0f %5.0f V\n",
           aec.name, aec.cs_pf, aec.rd_ohm, aec.max_voltage_kv);
    printf("%-30s %6.0f %6.0f %5.0f V\n",
           mil.name, mil.cs_pf, mil.rd_ohm, mil.max_voltage_kv);
    printf("%-30s %6.0f %6.0f %5.0f kV\n",
           tele.name, tele.cs_pf, tele.rd_ohm, tele.max_voltage_kv);
    printf("\n");

    /* Equivalence mapping for IEC Level 4 */
    esd_cross_standard_map_t map;
    esd_cross_standard_map(8.0, &map);
    printf("IEC 8 kV contact equivalent to:\n");
    printf("  HBM (JS-001):   ≈ %.0f V  %s\n",
           map.hbm_voltage_v,
           map.equivalent_valid ? "(approximate)" : "(not valid)");
    printf("  ISO 10605:      ≈ %.1f kV\n", map.iso_10605_kv);
    printf("  CAVEAT: %s\n", map.caveat);
    printf("\n");

    /* ─── Part 5: HBM Classification ──────────────────────── */
    printf("═══ Part 5: HBM Device Classification ═══\n\n");

    double hbm_test_voltages[] = {100, 300, 750, 1500, 3000, 6000, 10000};
    const char *class_names[] = {
        "Class 0", "Class 1A", "Class 1B", "Class 1C",
        "Class 2", "Class 3A", "Class 3B"
    };

    printf("  HBM Voltage  →  Classification\n");
    printf("  ─────────────────────────────\n");
    for (int i = 0; i < 7; i++) {
        esd_hbm_class_t cls = esd_hbm_classify(hbm_test_voltages[i]);
        double vmin, vmax;
        esd_hbm_class_range(cls, &vmin, &vmax);
        printf("  %6.0f V     →  %-10s [%.0f - %.0f V)\n",
               hbm_test_voltages[i], class_names[(int)cls], vmin, vmax);
    }
    printf("\n");

    /* ─── Part 6: Application-Specific Requirements ──────── */
    printf("═══ Part 6: Application-Specific Requirements ═══\n\n");

    printf("Medical Device (IEC 60601-1-2):\n");
    double med_cont, med_air;
    char med_reqs[256];
    esd_medical_requirements(0, &med_cont, &med_air, med_reqs);
    printf("  Standard: %.0f kV contact, %.0f kV air\n", med_cont, med_air);
    printf("  %s\n", med_reqs);

    printf("\nLife-Support Medical:\n");
    char life_reqs[256];
    esd_medical_requirements(1, &med_cont, &med_air, life_reqs);
    printf("  %s\n", life_reqs);

    printf("\nAutomotive (ISO 10605):\n");
    esd_auto_test_params_t auto_p;
    esd_auto_test_params(15.0, 0, &auto_p);
    printf("  15 kV air discharge\n");
    printf("  Network: %.0f pF / %.0f Ω\n", auto_p.cs_pf, auto_p.rd_ohm);
    printf("  Powered test: %s\n", auto_p.powered_test ? "Yes" : "No");
    printf("  Connector pins: %s\n",
           auto_p.esd_into_connector_pins ? "Yes" : "No");
    printf("\n");

    /* ─── Test point estimation ───────────────────────────── */
    printf("═══ Test Point Estimation ═══\n\n");

    double eut_dims[][3] = {
        {0.30, 0.20, 0.15},  /* Smartphone */
        {0.35, 0.25, 0.05},  /* Tablet */
        {0.15, 0.10, 0.05},  /* Wearable */
        {0.48, 0.13, 0.35},  /* Desktop PC */
    };
    const char *eut_names[] = {"Smartphone", "Tablet", "Wearable", "Desktop PC"};

    for (int i = 0; i < 4; i++) {
        int n = esd_compliance_num_test_points(eut_dims[i][0],
                                                eut_dims[i][1],
                                                eut_dims[i][2]);
        printf("  %-15s (%.2f×%.2f×%.2f)m → %d test points\n",
               eut_names[i],
               eut_dims[i][0], eut_dims[i][1], eut_dims[i][2], n);
    }

    /* ─── Cleanup ─────────────────────────────────────────── */
    esd_test_report_free(&report);
    esd_test_plan_free(&plan);

    printf("\n═══ Example Complete ═══\n");
    return 0;
}
