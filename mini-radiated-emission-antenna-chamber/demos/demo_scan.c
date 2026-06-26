/* =========================================================================
 * demo_scan.c - Interactive emission scan visualization demo
 * ========================================================================= */
#include "radiated_emission.h"
#include "antenna_types.h"
#include "chamber_theory.h"
#include "field_propagation.h"
#include "emission_limits.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

int main(void) {
    printf("
");
    printf("  ╔══════════════════════════════════════════╗
");
    printf("  ║  RADIATED EMISSION MEASUREMENT DEMO      ║
");
    printf("  ║  Antenna Chamber Theory Visualization    ║
");
    printf("  ╚══════════════════════════════════════════╝

");

    printf("--- CISPR 32 Class B Limit Line (3m) ---
");
    printf("  Frequency        QP Limit    Avg Limit
");
    printf("  ----------------------------------------
");
    double freqs[] = {30e6, 100e6, 230e6, 500e6, 1000e6, 2000e6, 4000e6};
    const char *labels[] = {"30 MHz", "100 MHz", "230 MHz", "500 MHz",
                            "1 GHz", "2 GHz", "4 GHz"};
    for (int i = 0; i < 7; i++) {
        double qp = el_get_standard_limit(RE_STD_CISPR32, freqs[i],
                                           RE_DETECTOR_QUASI_PEAK, 3.0, 1);
        double avg = el_get_standard_limit(RE_STD_CISPR32, freqs[i],
                                            RE_DETECTOR_AVERAGE, 3.0, 1);
        printf("  %-12s    %6.1f dBuV/m  %6.1f dBuV/m
", labels[i], qp, avg);
    }

    printf("
--- Biconical Antenna Factor Table ---
");
    re_antenna_factor_table_t af;
    re_af_table_init_standard(&af, "BICONICAL");
    re_af_table_print_summary(&af);
    printf("
  Sample AF values:
");
    for (int i = 0; i < 5; i++) {
        size_t idx = i * af.num_points / 5;
        if (idx >= af.num_points) idx = af.num_points - 1;
        printf("    %.1f MHz: AF = %.2f dB/m, Gain = %.1f dBi
",
               af.points[idx].frequency_hz / 1e6,
               af.points[idx].antenna_factor_db,
               af.points[idx].gain_dbi);
    }

    printf("
--- Far-Field Boundaries ---
");
    struct { double f; const char *n; double D; } ants[] = {
        {100e6, "Biconical", 1.37}, {300e6, "Biconical", 1.37},
        {300e6, "LPDA", 1.5}, {1000e6, "LPDA", 1.5},
        {3000e6, "DRG Horn", 0.1}, {10000e6, "DRG Horn", 0.1}
    };
    for (int i = 0; i < 6; i++) {
        double r_nf, r_ff;
        at_far_field_boundary(ants[i].D, ants[i].f, &r_nf, &r_ff);
        printf("  %-12s @ %6.0f MHz: R_ff = %.2f m (%.1f lambda)
",
               ants[i].n, ants[i].f/1e6, r_ff, r_ff / at_wavelength(ants[i].f));
    }
    printf("
  ╔══════════════════════════════════════════╗
");
    printf("  ║  Demo complete. Run examples for full    ║
");
    printf("  ║  emission test simulations.              ║
");
    printf("  ╚══════════════════════════════════════════╝

");
    return 0;
}
