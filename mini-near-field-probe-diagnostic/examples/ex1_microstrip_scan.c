/* Example 1: Microstrip Line Near-Field Scan
 * Demonstrates L6 canonical problem - microstrip line near-field mapping.
 * Simulates a PCB microstrip and computes the near-field distribution
 * scanned by an H-field loop probe.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "../include/nf_probe_core.h"
#include "../include/nf_scanning.h"

int main(void) {
    printf("=== Example 1: Microstrip Line Near-Field Scan ===\n");

    nf_microstrip_mapping_t ms = {
        .trace_width_m = 1e-3,
        .trace_length_m = 0.05,
        .substrate_h_m = 0.5e-3,
        .epsilon_r = 4.4,
        .char_impedance_ohm = 50,
        .freq_hz = 500e6,
        .input_power_dbm = 0,
        .termination_impedance_ohm = 50,
        .probe_height_m = 0.002
    };

    nf_scan_config_t cfg;
    nf_scan_config_init(&cfg, -0.005, 0.005, 21, -0.005, 0.055, 21, 0.002, NF_SCAN_RASTER);
    ms.scan_grid = cfg.grid;

    size_t n = cfg.grid.nx * cfg.grid.ny;
    nf_field_sample_t *field = calloc(n, sizeof(nf_field_sample_t));
    size_t map_size = 0;
    nf_microstrip_nearfield(&ms, field, &map_size);

    printf("Grid: %zu x %zu = %zu points\n", cfg.grid.nx, cfg.grid.ny, n);
    printf("Trace center at y = %.3f m\n", ms.trace_length_m / 2.0);

    double max_E = 0, max_H = 0;
    size_t max_idx = 0;
    for (size_t i = 0; i < map_size; i++) {
        double Em = cabs(field[i].Ez);
        double Hm = cabs(field[i].Hy);
        if (Em > max_E) { max_E = Em; max_idx = i; }
        if (Hm > max_H) max_H = Hm;
    }

    double x, y;
    nf_scan_grid_point_coords(&ms.scan_grid, max_idx, &x, &y);
    printf("Max E-field: %.3e V/m at (%.4f, %.4f)\n", max_E, x, y);
    printf("Max H-field: %.3e A/m\n", max_H);

    double resolution = nf_spatial_resolution(0.002, 0.002);
    printf("Spatial resolution: %.3f mm\n", resolution * 1000);

    /* Nyquist check */
    nf_nyquist_check_t nc;
    nf_nyquist_check(&cfg.grid, 500e6, &nc);
    printf("Nyquist spacing: %.3f mm, Satisfied: %s\n",
           nc.nyquist_spacing_m * 1000,
           nc.criterion_satisfied ? "YES" : "NO");

    free(field);
    printf("=== Example 1 Complete ===\n");
    return 0;
}
