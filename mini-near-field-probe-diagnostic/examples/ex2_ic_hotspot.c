/* Example 2: IC Radiated Emission Hot-Spot Detection
 * Demonstrates L6 canonical problem - locating emission sources on an IC
 * using near-field scan data.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../include/nf_probe_core.h"
#include "../include/nf_scanning.h"
#include "../include/nf_diagnostic.h"

int main(void) {
    printf("=== Example 2: IC Emission Hot-Spot Detection ===\n");

    nf_ic_emission_t ic = {
        .ic_width_m = 0.01,
        .ic_length_m = 0.01,
        .ic_height_m = 0.001,
        .die_position_x_m = 0.005,
        .die_position_y_m = 0.005,
        .clock_freq_hz = 100e6
    };

    size_t rows = 20, cols = 20;
    double *emission_map = calloc(rows * cols, sizeof(double));

    /* Simulate emission map with a hot spot at die position */
    for (size_t iy = 0; iy < rows; iy++) {
        for (size_t ix = 0; ix < cols; ix++) {
            double x = ic.ic_width_m * (double)ix / cols;
            double y = ic.ic_length_m * (double)iy / rows;
            double dx = x - ic.die_position_x_m;
            double dy = y - ic.die_position_y_m;
            double dist = sqrt(dx * dx + dy * dy);
            emission_map[iy * cols + ix] = 60.0 * exp(-dist / 0.001);
        }
    }

    nf_ic_hotspot_detect(&ic, emission_map, rows, cols, 40.0);

    printf("IC: %.0f x %.0f mm\n", ic.ic_width_m * 1000, ic.ic_length_m * 1000);
    printf("Clock: %.1f MHz\n", ic.clock_freq_hz / 1e6);
    printf("Hot spot: (%.3f, %.3f) mm, level = %.1f dBuV\n",
           ic.hot_spot_x_m * 1000, ic.hot_spot_y_m * 1000,
           ic.hot_spot_level_dbuV);

    /* Classify emission mechanism */
    nf_field_sample_t sample;
    memset(&sample, 0, sizeof(sample));
    sample.Ez = pow(10, ic.hot_spot_level_dbuV / 20.0) * 1e-6;
    sample.Hy = sample.Ez / ETA_0;
    nf_emission_mechanism_t mech;
    nf_emission_mechanism_classify(&sample, ic.clock_freq_hz, &mech);
    printf("Emission type: %s\n", mech.classification);
    printf("Wave impedance: %.1f Ohm (eta0=%.1f)\n",
           mech.wave_impedance_ohm, ETA_0);

    free(emission_map);
    if (ic.emission_map_dbuV) free(ic.emission_map_dbuV);
    printf("=== Example 2 Complete ===\n");
    return 0;
}
