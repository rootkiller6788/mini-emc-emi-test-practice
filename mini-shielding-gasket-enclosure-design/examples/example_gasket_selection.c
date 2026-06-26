/* example_gasket_selection.c -- Gasket selection for given EMI requirements
 * L6: Select optimal gasket based on SE, cost, environment constraints.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "shielding_gasket.h"

static const char *material_name(gasket_material_t m) {
    switch (m) {
        case GASKET_CONDUCTIVE_ELASTOMER: return "Conductive Elastomer";
        case GASKET_WIRE_MESH: return "Wire Mesh";
        case GASKET_SPRING_FINGER: return "Spring Finger";
        case GASKET_CONDUCTIVE_FABRIC: return "Conductive Fabric";
        case GASKET_FORM_IN_PLACE: return "Form-In-Place";
        case GASKET_METAL_SPIRAL: return "Metal Spiral";
        case GASKET_KNITTED_WIRE: return "Knitted Wire";
        case GASKET_ORIENTED_WIRE: return "Oriented Wire";
        case GASKET_CONDUCTIVE_FOAM: return "Conductive Foam";
        case GASKET_BERYLLIUM_COPPER: return "Beryllium Copper";
        default: return "Unknown";
    }
}

static const char *compat_str(int c) {
    return c == 0 ? "Compatible" : c == 1 ? "Marginal" : "Incompatible";
}

int main(void) {
    printf("=== Gasket Selection and Comparison ===

");

    /* Environment: outdoor enclosure, aluminum flange, 50dB target at 1GHz */
    shielding_material_id_t flange = MATERIAL_ALUMINUM;
    double freq = 1.0e9;
    double seam_length = 1.0;
    double target_se = 50.0;

    /* Test all gasket types */
    gasket_material_t types[] = {
        GASKET_CONDUCTIVE_ELASTOMER, GASKET_WIRE_MESH, GASKET_SPRING_FINGER,
        GASKET_CONDUCTIVE_FABRIC, GASKET_FORM_IN_PLACE, GASKET_METAL_SPIRAL,
        GASKET_KNITTED_WIRE, GASKET_ORIENTED_WIRE, GASKET_CONDUCTIVE_FOAM,
        GASKET_BERYLLIUM_COPPER,
    };
    int n = sizeof(types) / sizeof(types[0]);

    printf("Flange: Aluminum, Target SE: %.0f dB @ %.0f MHz
", target_se, freq/1e6);
    printf("Seam Length: %.1f m

", seam_length);
    printf("%-25s | %6s | %8s | %10s | %s
",
           "Gasket Type", "SE(dB)", "ZT(ohm*m)", "Force(N/m)", "Galvanic");
    printf("--------------------------|--------|----------|------------|----------
");

    for (int i = 0; i < n; i++) {
        gasket_spec_t spec;
        if (gasket_spec_init(types[i], GASKET_PROFILE_D_SHAPE, 6.0, &spec) != 0) continue;

        gasket_compression_t state;
        gasket_compute_compression(&spec, COMPRESSION_MODE_FIXED_DEFLECTION,
            spec.height_uncompressed_mm * 0.25, &state);

        double SE = gasket_shielding_effectiveness(&state, freq, seam_length);
        double ZT = gasket_transfer_impedance(&state, freq);
        int compat = gasket_galvanic_compatibility(&spec, flange);

        printf("%-25s | %5.1f  | %8.2e | %8.1f   | %s
",
               material_name(types[i]), SE, ZT,
               state.applied_force_n_per_m, compat_str(compat));
    }

    /* Life estimate for top performer */
    printf("
=== Service Life Estimate (Indoor, 2 cycles/day) ===
");
    gasket_spec_t spec;
    gasket_spec_init(GASKET_CONDUCTIVE_ELASTOMER, GASKET_PROFILE_D_SHAPE, 6.0, &spec);
    double life25 = gasket_service_life_estimate(&spec, 2.0, 25.0, 1);
    double life50 = gasket_service_life_estimate(&spec, 2.0, 50.0, 1);
    double life85 = gasket_service_life_estimate(&spec, 2.0, 85.0, 1);
    printf("  Conductive Elastomer @ 25C: %.1f years
", life25);
    printf("  Conductive Elastomer @ 50C: %.1f years
", life50);
    printf("  Conductive Elastomer @ 85C: %.1f years
", life85);

    return 0;
}
