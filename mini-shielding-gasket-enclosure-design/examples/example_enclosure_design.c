/* example_enclosure_design.c -- End-to-end enclosure SE analysis
 * Demonstrates L6 canonical problem: design a shielded enclosure
 * with apertures and evaluate its shielding effectiveness.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "shielding_gasket.h"

int main(void) {
    printf("=== Enclosure Shielding Effectiveness Analysis ===

");

    /* Define a typical electronics enclosure: 300x200x100mm aluminum */
    enclosure_spec_t enc;
    memset(&enc, 0, sizeof(enc));
    enc.geometry = ENCLOSURE_RECTANGULAR;
    enc.dimension_x_m = 0.300;
    enc.dimension_y_m = 0.200;
    enc.dimension_z_m = 0.100;

    /* 2mm aluminum shield */
    shielding_material_t al;
    shielding_material_get(MATERIAL_ALUMINUM, &al);
    enc.shield.num_layers = 1;
    enc.shield.layers[0].material = al;
    enc.shield.layers[0].thickness_m = 0.002;
    enc.shield.layers[0].distance_from_source_m = 1.0;

    /* Seam: gasketed, 1m perimeter */
    enc.seam_total_length_m = 2.0 * (0.300 + 0.200);
    enc.seam_treatment = SEAM_TREATMENT_GASKET;
    enc.num_ground_points = 4;

    /* One ventilation slot: 100mm x 2mm */
    aperture_spec_t vent;
    memset(&vent, 0, sizeof(vent));
    vent.type = APERTURE_SLOT;
    vent.dimension1_m = 0.100;  /* length */
    vent.dimension2_m = 0.002;  /* width */
    vent.num_apertures = 1;
    enc.apertures = &vent;
    enc.num_apertures = 1;

    /* Compute cavity modes */
    cavity_analysis_t cav;
    enclosure_cavity_modes(&enc, 3.0e9, &cav);
    printf("Cavity Analysis:
");
    printf("  First resonance: %.1f MHz
", cav.first_resonance_hz / 1e6);
    printf("  Modes found below 3 GHz: %d

", cav.num_modes_found);

    /* Frequency sweep */
    printf("Frequency Sweep (SE in dB):
");
    printf("  Freq (MHz) | Wall SE | Aperture | Seam  | Net SE
");
    printf("  -----------|---------|----------|-------|-------
");

    double freqs[] = {10.0e6, 50.0e6, 100.0e6, 300.0e6, 500.0e6, 1000.0e6};
    int nfreq = sizeof(freqs) / sizeof(freqs[0]);

    for (int i = 0; i < nfreq; i++) {
        double f = freqs[i];
        double SE_wall = shielding_effectiveness_multilayer(
            &enc.shield, f, SE_FIELD_PLANE_WAVE, 1.0);
        double SE_ap = enclosure_aperture_leakage(&vent, f, SE_FIELD_PLANE_WAVE);
        double SE_seam = enclosure_seam_leakage(&enc, f, enc.seam_treatment);
        double SE_net = enclosure_se_with_apertures(&enc, f, SE_FIELD_PLANE_WAVE);

        printf("  %9.1f | %7.1f | %8.1f | %5.1f | %5.1f
",
               f/1e6, SE_wall, SE_ap, SE_seam, SE_net);
    }

    /* Recommend gasket */
    gasket_spec_t rec;
    enclosure_recommend_gasket(&enc, 40.0, 1000.0e6, &rec);
    printf("
Recommended gasket for 40dB @ 1GHz:
");
    printf("  Material ID: %d, Width: %.1f mm, Height: %.1f mm
",
           rec.material, rec.width_mm, rec.height_uncompressed_mm);

    return 0;
}
