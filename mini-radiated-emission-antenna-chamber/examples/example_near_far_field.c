/* =========================================================================
 * example_near_far_field.c — Near-Field / Far-Field Transition Analysis
 *
 * L6: Explore EMC measurement challenges at the near/far boundary.
 * L3: Demonstrate field behavior across different regions.
 *
 * This example:
 *   1. Computes field regions for common EMC measurement distances
 *   2. Shows wave impedance transition from near to far field
 *   3. Compares 3m vs 10m measurement distance field behavior
 *   4. Demonstrates distance extrapolation validity
 *
 * Reference: Balanis (2016) ch 2, Paul (2006) EMC ch 3
 * ========================================================================= */

#include "radiated_emission.h"
#include "antenna_types.h"
#include "chamber_theory.h"
#include "field_propagation.h"
#include <stdio.h>
#include <math.h>

static const char *region_name(fp_field_region_t r)
{
    switch (r) {
    case FP_STATIC_FIELD:         return "STATIC";
    case FP_REACTIVE_NEAR_FIELD:  return "REACTIVE_NEAR";
    case FP_RADIATING_NEAR_FIELD: return "RADIATING_NEAR";
    case FP_FAR_FIELD:            return "FAR_FIELD";
    default:                      return "UNKNOWN";
    }
}

int main(void)
{
    printf("==============================================\n");
    printf("  Near-Field / Far-Field Transition Analysis\n");
    printf("  EMC Antenna Measurement Considerations\n");
    printf("==============================================\n\n");

    /* ---- Part 1: Field Region Classification ---- */
    printf("Part 1: Field Region Classification\n");
    printf("Antenna size D = 1.0 m (typical LPDA)\n\n");

    double D = 1.0;
    double freqs[] = {30e6, 100e6, 300e6, 500e6, 1000e6, 3000e6, 10000e6};
    size_t n_freqs = sizeof(freqs) / sizeof(freqs[0]);

    printf("%-14s %-10s %-10s %-10s %-16s %-16s\n",
           "Freq", "Lambda(m)", "R_nf(m)", "R_ff(m)", "3m Region", "10m Region");
    printf("-----------------------------------------------------------------\n");

    for (size_t i = 0; i < n_freqs; i++) {
        double freq = freqs[i];
        double lambda = at_wavelength(freq);

        double r_nf, r_ff;
        at_far_field_boundary(D, freq, &r_nf, &r_ff);

        fp_field_region_t r3  = fp_classify_region(3.0, D, freq);
        fp_field_region_t r10 = fp_classify_region(10.0, D, freq);

        printf("%-14.3f %-10.3f %-10.3f %-10.3f %-16s %-16s\n",
               freq / 1e6, lambda, r_nf, r_ff,
               region_name(r3), region_name(r10));
    }

    /* ---- Part 2: Wave Impedance vs Distance ---- */
    printf("\nPart 2: Wave Impedance Transition (100 MHz)\n");
    printf("Distance from source vs wave impedance\n\n");
    printf("%-14s %-14s %-18s\n", "Distance(m)", "kr", "Zw (ohm)");
    printf("-------------------------------------------\n");

    double distances[] = {0.05, 0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0, 30.0};
    size_t n_dist = sizeof(distances) / sizeof(distances[0]);
    double freq_zw = 100e6;
    double eta_0 = fp_free_space_impedance();

    for (size_t i = 0; i < n_dist; i++) {
        double k = fp_wavenumber(freq_zw);
        double kr = k * distances[i];
        double zw = fp_wave_impedance_magnitude(distances[i], freq_zw);

        printf("%-14.2f %-14.3f %-18.1f", distances[i], kr, zw);

        if (fabs(zw - eta_0) / eta_0 < 0.05) {
            printf(" (far-field)");
        } else if (zw > eta_0 * 2) {
            printf(" (high-Z near-field)");
        }
        printf("\n");
    }

    /* ---- Part 3: Distance Extrapolation Validity ---- */
    printf("\nPart 3: Distance Extrapolation (1/r rule validity)\n\n");

    printf("%-14s %-10s %-16s %-16s %-10s\n",
           "Freq", "Lambda(m)", "E_3m(V/m)", "E_10m(V/m)", "1/r ratio");
    printf("--------------------------------------------------------\n");

    double P_tx = 10e-3; /* 10 mW */
    double G_tx = 1.64;   /* Half-wave dipole */

    for (size_t i = 0; i < 5; i++) {
        double freq_test = 30e6 * pow(2.0, (double)i); /* 30, 60, 120, 240, 480 MHz */

        double e_3m  = fp_field_from_power(P_tx, G_tx, 3.0);
        double e_10m = fp_field_from_power(P_tx, G_tx, 10.0);
        double ratio = e_3m / e_10m;
        double expected_ratio = 10.0 / 3.0; /* 1/r -> 3.33 */
        double error_pct = fabs(ratio - expected_ratio) / expected_ratio * 100.0;

        printf("%-14.3f %-10.3f %-16.6f %-16.6f %-10.1f%%\n",
               freq_test / 1e6, at_wavelength(freq_test),
               e_3m, e_10m, error_pct);
    }

    /* ---- Part 4: EMC Measurement Recommendation ---- */
    printf("\nPart 4: Measurement Distance Recommendations\n");
    printf("-------------------------------------------\n");

    struct { double f; const char *band; } bands[] = {
        {30e6, "C (Biconical)"},
        {100e6, "C"},
        {300e6, "D (LPDA)"},
        {500e6, "D"},
        {1000e6, "E (Horn)"},
        {3000e6, "E"},
        {10000e6, "F (Horn)"}
    };
    size_t n_bands = sizeof(bands) / sizeof(bands[0]);

    printf("%-12s %-8s %-18s %-20s\n",
           "Band", "Freq", "Min far-field", "3m OK?");
    printf("------------------------------------------------------\n");
    for (size_t i = 0; i < n_bands; i++) {
        double r_nf, r_ff;
        double D_ef;
        if (bands[i].f < 1e9) D_ef = 1.37;       /* Biconical/LPDA */
        else D_ef = 0.15;                          /* Horn */

        at_far_field_boundary(D_ef, bands[i].f, &r_nf, &r_ff);
        const char *ok = (r_ff < 3.0) ? "YES (far-field)" : "NO (near-field)";

        printf("%-12s %-8.0f %-18.2f %-20s\n",
               bands[i].band, bands[i].f/1e6, r_ff, ok);
    }

    printf("\n==============================================\n");
    printf("Note: For LPDA below 200 MHz at 3m, near-field\n");
    printf("corrections may be needed (CISPR 16-1-4 Annex D).\n");
    printf("==============================================\n");

    return 0;
}