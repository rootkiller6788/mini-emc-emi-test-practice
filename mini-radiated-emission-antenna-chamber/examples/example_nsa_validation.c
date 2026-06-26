/* =========================================================================
 * example_nsa_validation.c - Chamber NSA Validation
 *
 * L6: Compare measured NSA vs theoretical NSA for chamber qualification.
 * L7: CISPR 16-1-4 Annex D chamber validation procedure.
 *
 * Reference: CISPR 16-1-4:2019 section 6, Annex D
 *            ANSI C63.4:2014 Annex A
 * ========================================================================= */

#include "radiated_emission.h"
#include "antenna_types.h"
#include "chamber_theory.h"
#include "field_propagation.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

int main(void)
{
    printf("========================================\n");
    printf("  Chamber NSA Validation\n");
    printf("  3m Semi-Anechoic Chamber Qualification\n");
    printf("========================================\n\n");

    double distance_m = 3.0;
    double h_tx_m = 2.0;

    double test_freqs[] = {
        30e6, 50e6, 80e6, 100e6, 150e6, 200e6,
        300e6, 500e6, 700e6, 1000e6
    };
    size_t n_freqs = sizeof(test_freqs) / sizeof(test_freqs[0]);

    printf("Test setup:\n");
    printf("  Distance: %.1f m\n", distance_m);
    printf("  TX antenna height: %.1f m\n", h_tx_m);
    printf("  RX height scan: 1.0-4.0 m\n");
    printf("  Frequencies: %zu points\n\n", n_freqs);

    printf("NSA Validation Results:\n");
    printf("%-14s %-8s %-10s %-10s %-10s %-8s\n",
           "Freq (MHz)", "Pol", "H_opt(m)", "NSA_meas", "NSA_theo", "Delta");
    printf("-------------------------------------------------------------\n");

    int total_pass = 0;
    int total_meas = 0;
    double max_deviation = 0.0;
    double worst_freq = 0.0;

    for (size_t f = 0; f < n_freqs; f++) {
        double freq = test_freqs[f];

        for (int pol = 0; pol < 2; pol++) {
            re_polarization_t polarization = (pol == 0) ?
                RE_POL_HORIZONTAL : RE_POL_VERTICAL;

            double h_opt, max_ratio;
            ct_nsa_height_scan_opt(distance_m, h_tx_m, freq,
                                    polarization, &h_opt, &max_ratio);

            double nsa_theory = ct_nsa_theoretical(distance_m, h_tx_m,
                                                    h_opt, freq, polarization);

            double measurement_error = 1.5 * sin((double)f * 0.7);
            double nsa_measured = nsa_theory + measurement_error;

            double deviation = nsa_measured - nsa_theory;
            int pass = (fabs(deviation) <= 4.0) ? 1 : 0;

            printf("%-14.3f %-8s %-10.2f %-10.2f %-10.2f %-+8.2f %s\n",
                   freq / 1e6,
                   (polarization == RE_POL_HORIZONTAL) ? "HORIZ" : "VERT",
                   h_opt, nsa_measured, nsa_theory, deviation,
                   pass ? "PASS" : "FAIL");

            total_meas++;
            if (pass) total_pass++;
            if (fabs(deviation) > max_deviation) {
                max_deviation = fabs(deviation);
                worst_freq = freq;
            }
        }
    }

    printf("\n========================================\n");
    printf("NSA Validation Summary:\n");
    printf("  %d/%d measurements within +/- 4 dB\n", total_pass, total_meas);
    printf("  Max deviation: %.2f dB at %.3f MHz\n", max_deviation, worst_freq / 1e6);
    printf("  Overall: %s\n", (total_pass == total_meas) ? "PASS" : "FAIL");
    printf("  CISPR 16-1-4 criterion: +/- 4 dB\n");
    printf("========================================\n");

    return (total_pass == total_meas) ? 0 : 1;
}