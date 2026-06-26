/* example_multilayer_shield.c -- Multi-layer shield optimization
 * L6-L8: Compare single vs multi-layer shields, explore tradeoffs
 * between absorption, reflection, weight, and cost.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "shielding_gasket.h"

int main(void) {
    printf("=== Multi-Layer Shield Optimization ===

");

    shielding_material_t copper, steel, mu_metal;
    shielding_material_get(MATERIAL_COPPER, &copper);
    shielding_material_get(MATERIAL_STEEL_1008, &steel);
    shielding_material_get(MATERIAL_MU_METAL, &mu_metal);

    /* Design #1: Single layer 0.5mm copper */
    shield_stack_t s1;
    s1.num_layers = 1;
    s1.layers[0].material = copper;
    s1.layers[0].thickness_m = 0.0005;
    s1.layers[0].distance_from_source_m = 1.0;

    /* Design #2: Single layer 1mm steel */
    shield_stack_t s2;
    s2.num_layers = 1;
    s2.layers[0].material = steel;
    s2.layers[0].thickness_m = 0.001;
    s2.layers[0].distance_from_source_m = 1.0;

    /* Design #3: Two-layer: 0.5mm copper + 0.5mm mu-metal with 2mm gap */
    shield_stack_t s3;
    s3.num_layers = 2;
    s3.layers[0].material = copper;
    s3.layers[0].thickness_m = 0.0005;
    s3.layers[0].distance_from_source_m = 1.0;
    s3.layers[1].material = mu_metal;
    s3.layers[1].thickness_m = 0.0005;
    s3.layers[1].distance_from_source_m = 1.0;
    s3.air_gap_m[0] = 0.002;

    /* Design #4: Three-layer: Cu + air + Steel + air + Cu */
    shield_stack_t s4;
    s4.num_layers = 3;
    s4.layers[0].material = copper;
    s4.layers[0].thickness_m = 0.0003;
    s4.layers[0].distance_from_source_m = 1.0;
    s4.layers[1].material = steel;
    s4.layers[1].thickness_m = 0.0005;
    s4.layers[1].distance_from_source_m = 1.0;
    s4.layers[2].material = copper;
    s4.layers[2].thickness_m = 0.0003;
    s4.layers[2].distance_from_source_m = 1.0;
    s4.air_gap_m[0] = 0.001;
    s4.air_gap_m[1] = 0.001;

    shield_stack_t *designs[] = {&s1, &s2, &s3, &s4};
    const char *names[] = {"1-layer Cu 0.5mm", "1-layer Steel 1mm",
                           "2-layer Cu+MuMetal", "3-layer Cu+Steel+Cu"};
    int nd = 4;

    /* Frequency sweep: 10 kHz to 10 GHz (magnetic, plane wave, electric) */
    double freqs[] = {1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10};
    int nf = sizeof(freqs) / sizeof(freqs[0]);

    printf("Low-frequency magnetic shielding (60 Hz equivalent):
");
    for (int d = 0; d < nd; d++) {
        double SE_H = shielding_effectiveness_multilayer(
            designs[d], 60.0, SE_FIELD_NEAR_H, 0.5);
        printf("  %-22s: %.1f dB
", names[d], SE_H);
    }

    printf("
Mid-frequency plane wave (1-1000 MHz):
");
    printf("  %-20s", "Frequency");
    for (int d = 0; d < nd; d++) printf(" | %-12s", names[d]);
    printf("
");
    for (int i = 0; i < nf; i++) {
        printf("  %9.1f MHz", freqs[i]/1e6);
        for (int d = 0; d < nd; d++) {
            double SE = shielding_effectiveness_multilayer(
                designs[d], freqs[i], SE_FIELD_PLANE_WAVE, 1.0);
            printf(" | %8.1f dB", SE);
        }
        printf("
");
    }

    /* Weight comparison */
    printf("
Weight comparison (per m2):
");
    for (int d = 0; d < nd; d++) {
        double weight = 0.0;
        for (int l = 0; l < designs[d]->num_layers; l++) {
            weight += designs[d]->layers[l].material.density
                    * designs[d]->layers[l].thickness_m;
        }
        printf("  %-22s: %.2f kg/m2
", names[d], weight);
    }

    return 0;
}
