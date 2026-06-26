/* example_smps_prescan.c - SMPS Conducted Emissions Pre-Scan */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "emi_precompliance_core.h"
#include "emi_measurement_setup.h"
#include "emi_spectrum_analysis.h"
#include "emi_lowcost_methods.h"
#include "emi_correlation_uncertainty.h"

int main(void) {
    printf("=== SMPS Conducted Emissions Pre-Scan Example ===\n");
    printf("Standard: CISPR 32 Class B, 150 kHz - 30 MHz\n\n");

    emi_limit_line_t limit_qp, limit_avg;
    emi_limit_init_cispr32_conducted_classb(&limit_qp, EMI_DETECTOR_QUASIPEAK);
    emi_limit_init_cispr32_conducted_classb(&limit_avg, EMI_DETECTOR_AVERAGE);

    emi_scan_config_t config;
    emi_scan_config_init(&config, EMI_EMISSION_CONDUCTED, EMI_STD_CISPR32);
    config.detector = EMI_DETECTOR_PEAK;
    config.average_count = 1;

    printf("Scan Configuration:\n");
    printf("  Frequency range: %.0f kHz - %.0f MHz\n",
           config.start_freq_hz / 1e3, config.stop_freq_hz / 1e6);
    printf("  RBW: %.0f Hz, VBW: %.0f Hz\n", config.rbw_hz, config.vbw_hz);
    printf("  Sweep time: %.3f s, Points: %d\n\n",
           config.sweep_time_s, config.num_points);

    emi_sa_spec_t sa;
    emi_sa_init_entry_benchtop(&sa);

    emi_scan_result_t prescan;
    if (emi_peak_prescan(&config, &sa, &prescan) != 0) {
        printf("ERROR: Pre-scan failed!\n");
        return 1;
    }

    double f_sw = 100e3;
    double base_noise_dbuv = 10.0;

    for (int i = 0; i < prescan.num_points; i++) {
        double freq = prescan.points[i].frequency_hz;
        double amp_dbuv = base_noise_dbuv;
        int harmonic = (int)(freq / f_sw + 0.5);
        double detuning = (freq - harmonic * f_sw) / (config.rbw_hz);
        double harmonic_amp = 70.0 - 20.0 * log10((double)(harmonic > 0 ? harmonic : 1));
        amp_dbuv += harmonic_amp * exp(-0.5 * detuning * detuning);
        double bb_noise = 45.0 * exp(-freq / 5e6);
        amp_dbuv += bb_noise;
        if (amp_dbuv < base_noise_dbuv) amp_dbuv = base_noise_dbuv;
        prescan.points[i].amplitude_dbuv = amp_dbuv;
        prescan.points[i].amplitude_dbm = emi_dbuv_to_dbm(amp_dbuv, 50.0);
        prescan.points[i].detector = EMI_DETECTOR_PEAK;
    }

    emi_evaluate_all_margins(&prescan, &limit_qp);

    printf("Pre-Scan Results (Peak vs QP Limit):\n");
    printf("  Max emission: %.1f dBuV at %.3f MHz\n",
           prescan.max_emission_dbuv, prescan.max_emission_freq_hz / 1e6);
    printf("  Worst margin: %.1f dB\n", prescan.worst_margin_db);
    printf("  Failing points: %d, Marginal points: %d\n\n",
           prescan.num_failing_points, prescan.num_marginal_points);

    int peaks[50];
    int n_peaks = emi_find_peaks(&prescan, 6.0, peaks, 50);
    printf("Identified %d emission peaks above threshold\n", n_peaks);

    if (n_peaks > 0) {
        printf("\nTop Emission Peaks:\n");
        printf("%-6s %-14s %-12s %-12s %-10s\n",
               "Rank", "Frequency", "Amp(dBuV)", "Limit(dBuV)", "Margin(dB)");
        for (int i = 0; i < n_peaks && i < 10; i++) {
            int idx = peaks[i];
            double f = prescan.points[idx].frequency_hz;
            double a = prescan.points[idx].amplitude_dbuv;
            double lim = emi_limit_line_interpolate(&limit_qp, f);
            double marg = lim - a;
            const char *funit = "Hz";
            if (f >= 1e6) { f /= 1e6; funit = "MHz"; }
            else if (f >= 1e3) { f /= 1e3; funit = "kHz"; }
            printf("%-6d %-10.3f %-4s %-12.1f %-12.1f %-+10.1f\n",
                   i + 1, f, funit, a, lim, marg);
        }
    }

    emi_scan_result_t final;
    if (emi_final_qp_measurement(&config, &prescan, &final) == 0) {
        for (int i = 0; i < final.num_points; i++) {
            double qp_weight = emi_qp_weighting_db(200.0, EMI_STD_CISPR32);
            final.points[i].amplitude_dbuv += qp_weight;
            final.points[i].amplitude_dbm = emi_dbuv_to_dbm(
                final.points[i].amplitude_dbuv, 50.0);
        }
        emi_evaluate_all_margins(&final, &limit_qp);

        printf("\nFinal QP Results:\n");
        printf("  Max QP emission: %.1f dBuV\n", final.max_emission_dbuv);
        printf("  Worst margin: %.1f dB\n", final.worst_margin_db);

        emi_uncertainty_budget_t budget;
        emi_ucispr_conducted_init(&budget);
        emi_ucispr_finalize(&budget);

        printf("\nMeasurement Uncertainty (CISPR 16-4-2):\n");
        printf("  Combined uncertainty: %.2f dB\n", budget.combined_standard_uncertainty_dB);
        printf("  Expanded uncertainty: %.2f dB\n", budget.ucispr_dB);

        emi_margin_status_t decision = emi_ucispr_decision(
            final.max_emission_dbuv,
            emi_limit_line_interpolate(&limit_qp, final.max_emission_freq_hz),
            budget.ucispr_dB);

        printf("\n=== FINAL VERDICT ===\n");
        switch (decision) {
            case EMI_MARGIN_PASS:
                printf("PASS - Compliant with 95%% confidence\n");
                break;
            case EMI_MARGIN_FAIL:
                printf("FAIL - Non-compliant even considering uncertainty\n");
                break;
            case EMI_MARGIN_MARGINAL:
                printf("MARGINAL - Indeterminate; full compliance test recommended\n");
                break;
        }
        free(budget.items);
        free(final.points);
    }
    free(prescan.points);
    emi_limit_line_free(&limit_qp);
    emi_limit_line_free(&limit_avg);
    printf("\n--- SMPS pre-scan complete ---\n");
    return 0;
}
