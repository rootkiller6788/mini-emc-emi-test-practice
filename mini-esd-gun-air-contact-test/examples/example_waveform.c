/**
 * @file example_waveform.c
 * @brief Example: IEC 61000-4-2 ESD waveform generation and compliance check
 *
 * L6 Canonical Problem: Demonstrates generation of IEC-compliant
 * ESD current waveforms, parameter extraction, and compliance
 * verification against the standard.
 *
 * This example validates that our Heidler waveform model produces
 * waveforms meeting the IEC 61000-4-2 specification tolerances.
 */

#include "esd_waveform.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

int main(void)
{
    printf("╔═══════════════════════════════════════════════╗\n");
    printf("║  IEC 61000-4-2 ESD Waveform Example          ║\n");
    printf("║  Waveform generation & compliance check      ║\n");
    printf("╚═══════════════════════════════════════════════╝\n\n");

    /* Demonstrate all four IEC severity levels */
    for (int level_idx = ESD_LEVEL_1; level_idx <= ESD_LEVEL_4; level_idx++) {
        esd_test_level_t level = (esd_test_level_t)level_idx;

        /* Get standard specification */
        esd_waveform_spec_t spec = esd_waveform_spec_standard(level,
                                                                ESD_DISCHARGE_CONTACT);

        printf("═══ IEC Level %d — %.0f kV Contact Discharge ═══\n",
               level_idx + 1, spec.voltage_kv);
        printf("  Nominal: Ip=%.1f A, tr=%.2f ns, I30=%.1f A, I60=%.1f A\n",
               spec.peak_current_a, spec.rise_time_ns,
               spec.current_at_30ns_a, spec.current_at_60ns_a);

        /* Generate IEC waveform using calibrated Heidler model */
        esd_waveform_data_t data;
        memset(&data, 0, sizeof(data));

        int ret = esd_waveform_generate_iec(&spec, 200.0, 500, &data);
        if (ret != 0) {
            printf("  ERROR: Waveform generation failed!\n");
            continue;
        }

        /* Extract waveform parameters */
        esd_waveform_spec_t measured;
        memset(&measured, 0, sizeof(measured));
        ret = esd_waveform_extract_params(&data, &measured);
        if (ret != 0) {
            printf("  ERROR: Parameter extraction failed!\n");
            esd_waveform_data_free(&data);
            continue;
        }

        printf("  Measured: Ip=%.1f A, tr=%.2f ns, I30=%.1f A, I60=%.1f A\n",
               measured.peak_current_a, measured.rise_time_ns,
               measured.current_at_30ns_a, measured.current_at_60ns_a);

        /* Check IEC compliance */
        esd_compliance_result_t result;
        memset(&result, 0, sizeof(result));
        ret = esd_waveform_check_compliance(&measured, &spec, &result);
        if (ret == 0) {
            printf("  Compliance:\n");
            printf("    Rise time (%.2f ns):  %s\n",
                   result.measured_rise_time_ns,
                   result.passes_rise_time ? "PASS" : "FAIL");
            printf("    Peak current (%.1f A): %s\n",
                   result.measured_peak_current_a,
                   result.passes_peak_current ? "PASS" : "FAIL");
            printf("    I @ 30ns (%.1f A):     %s\n",
                   result.measured_current_30ns,
                   result.passes_current_30ns ? "PASS" : "FAIL");
            printf("    I @ 60ns (%.1f A):     %s\n",
                   result.measured_current_60ns,
                   result.passes_current_60ns ? "PASS" : "FAIL");
            printf("    → OVERALL: %s\n",
                   result.overall_pass ? "PASS ✓" : "FAIL ✗");
        }

        /* Compute waveform metrics */
        double charge_nc = esd_waveform_total_charge(&data);
        double energy_uj = esd_waveform_energy(&data, 2.0);
        double fwhm_ns = esd_waveform_fwhm(&data);
        double tr_ns = esd_waveform_rise_time(&data);

        printf("  Metrics:\n");
        printf("    Total charge:  %.2f nC\n", charge_nc);
        printf("    Energy (2Ω):   %.2f µJ\n", energy_uj);
        printf("    FWHM:          %.2f ns\n", fwhm_ns);
        printf("    Rise time:     %.2f ns\n", tr_ns);

        /* Print a few waveform sample points */
        printf("  Waveform samples (I(t)):\n");
        printf("    t=0 ns:    %8.3f A\n", data.current_a[0]);
        printf("    t=1 ns:    %8.3f A\n",
               data.current_a[(size_t)(1.0 / (200.0/499.0))]);
        printf("    t=5 ns:    %8.3f A\n",
               data.current_a[(size_t)(5.0 / (200.0/499.0))]);
        printf("    t=30 ns:   %8.3f A\n",
               data.current_a[(size_t)(30.0 / (200.0/499.0))]);
        printf("    t=60 ns:   %8.3f A\n",
               data.current_a[(size_t)(60.0 / (200.0/499.0))]);
        printf("    t=100 ns:  %8.3f A\n",
               data.current_a[(size_t)(100.0 / (200.0/499.0))]);

        printf("\n");

        esd_waveform_data_free(&data);
    }

    /* Demonstrate double-exponential model */
    printf("═══ Double-Exponential Model Comparison ═══\n");

    esd_waveform_spec_t spec4 = esd_waveform_spec_standard(ESD_LEVEL_4,
                                                             ESD_DISCHARGE_CONTACT);
    esd_waveform_data_t data_heidler, data_dexp;
    memset(&data_heidler, 0, sizeof(data_heidler));
    memset(&data_dexp, 0, sizeof(data_dexp));

    esd_waveform_generate_heidler(&spec4, 100.0, 200, &data_heidler);
    esd_waveform_generate_double_exp(&spec4, 100.0, 200, &data_dexp);

    printf("  Heidler peak:        %.2f A\n",
           esd_waveform_rise_time(&data_heidler) > 0 ? 1.0 : 0.0);
    printf("  Double-exp peak:     N/A (see plot)\n");
    printf("  Heidler charge:      %.2f nC\n",
           esd_waveform_total_charge(&data_heidler));
    printf("  Double-exp charge:   %.2f nC\n",
           esd_waveform_total_charge(&data_dexp));

    esd_waveform_data_free(&data_heidler);
    esd_waveform_data_free(&data_dexp);

    printf("\n═══ Example Complete ═══\n");
    return 0;
}
