/* Example 3: Complete PCB EMC Diagnostic Workflow
 * Demonstrates L7 application - full PCB diagnostic from near-field scan
 * through emission ranking and regulatory compliance check.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../include/nf_probe_core.h"
#include "../include/nf_scanning.h"
#include "../include/nf_diagnostic.h"

int main(void) {
    printf("=== Example 3: PCB EMC Diagnostic Workflow ===\n");

    nf_pcb_diagnostic_t pcb;
    nf_pcb_diagnostic_init(&pcb, "IoT_Controller_v2", "Rev C", 0.08, 0.06);
    pcb.num_layers = 4;

    nf_scan_config_t cfg;
    nf_scan_config_init(&cfg, 0, 0.08, 41, 0, 0.06, 31, 0.003, NF_SCAN_RASTER);
    nf_scan_config_set_freq(&cfg, 30e6, 1e9, 10e6);

    nf_scan_dataset_t ds;
    if (nf_scan_dataset_allocate(&ds, &cfg) == 0) {
        /* Simulate scan data */
        for (size_t f = 0; f < ds.num_frames; f++) {
            for (size_t i = 0; i < ds.frames[f].num_samples; i++) {
                double x, y;
                nf_scan_grid_point_coords(&ds.frames[f].grid, i, &x, &y);
                double dist = sqrt(pow(x - 0.04, 2) + pow(y - 0.03, 2));
                ds.frames[f].field_samples[i].Ez =
                    1e-3 * exp(-dist / 0.005) * (1.0 + 0.1 * sin(2 * M_PI * f / 20));
            }
        }
        ds.valid = 1;

        /* Run PCB diagnostic */
        nf_pcb_diagnostic_evaluate(&pcb, &ds, 40.0, 40.0, 50.0);

        printf("PCB: %s %s (%.0f x %.0f mm, %d layers)\n",
               pcb.pcb_name, pcb.revision,
               pcb.board_width_m * 1000, pcb.board_length_m * 1000,
               pcb.num_layers);
        printf("FCC Class B: %s (margin: %.1f dB)\n",
               pcb.pass_fcc_class_b ? "PASS" : "FAIL",
               pcb.overall_margin_db);
        printf("CISPR 22 Class B: %s\n",
               pcb.pass_cispr_22_class_b ? "PASS" : "FAIL");
        printf("CISPR 25: %s\n",
               pcb.pass_cispr_25 ? "PASS" : "FAIL");

        /* Source catalog: identify and rank emission sources */
        nf_source_catalog_t cat;
        nf_source_catalog_init(&cat, 5);
        nf_source_catalog_add(&cat, NF_SOURCE_CLOCK_HARMONIC,
                               100e6, 35.0, 0.04, 0.03, 0.003, 0.9, "MCU clock harmonic");
        nf_source_catalog_add(&cat, NF_SOURCE_SWITCHING_NOISE,
                               50e6, 28.0, 0.02, 0.04, 0.003, 0.7, "DC-DC converter switching");
        nf_source_catalog_add(&cat, NF_SOURCE_CROSSTALK,
                               200e6, 22.0, 0.06, 0.02, 0.003, 0.5, "SPI bus crosstalk");
        nf_source_catalog_add(&cat, NF_SOURCE_PDN_RESONANCE,
                               80e6, 38.0, 0.04, 0.02, 0.003, 0.8, "PDN resonance at Vcore plane");
        nf_source_catalog_add(&cat, NF_SOURCE_GROUND_BOUNCE,
                               150e6, 32.0, 0.04, 0.03, 0.003, 0.6, "Ground bounce at IO bank");

        double limits[] = {40.0, 40.0, 40.0, 40.0, 40.0};
        nf_emission_rank_t ranking[5];
        size_t n_ranked;
        nf_source_catalog_rank(&cat, limits, ranking, &n_ranked);

        printf("\nEmission Source Ranking:\n");
        for (size_t i = 0; i < n_ranked; i++) {
            printf("  #%zu: %s\n", i + 1,
                   cat.sources[ranking[i].source_index].description);
            printf("       Level: %.1f dBuV, Margin: %.1f dB, Risk: %.1f\n",
                   cat.sources[ranking[i].source_index].magnitude_dbuV,
                   ranking[i].margin_db, ranking[i].risk_score);
        }

        /* Export scan data to CSV */
        if (nf_scan_dataset_export_csv(&ds, "scan_output.csv") == 0) {
            printf("\nScan data exported to scan_output.csv\n");
        }

        nf_source_catalog_free(&cat);
        nf_scan_dataset_free(&ds);
    }

    nf_pcb_diagnostic_free(&pcb);
    printf("=== Example 3 Complete ===\n");
    return 0;
}
