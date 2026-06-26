/**
 * @file example_cm_choke_analysis.c
 * @brief CM Choke Performance Analysis (L6 Canonical Problem)
 *
 * Performs a complete analysis of a CM choke for a motor drive:
 *  1. Model core material and geometry
 *  2. Compute CM and DM impedances vs frequency
 *  3. Check saturation under worst-case conditions
 *  4. Calculate core loss and temperature rise
 *  5. Analyze CMRR degradation at high frequency
 *
 * This demonstrates the full physical analysis of a common-mode
 * choke as used in a 1 HP variable-frequency motor drive.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "cm_dm_filter.h"
#include "cm_choke_model.h"

int main(void) {
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║  CM Choke Performance Analysis                           ║\n");
    printf("║  1 HP Motor Drive — Common-Mode Choke                    ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");

    /* Look up N87 ferrite material */
    printf("─── Core Material: TDK/Epcos N87 MnZn Ferrite ───\n");
    core_material_t *mat = core_material_lookup(CORE_MAT_N87);
    if (mat == NULL) {
        printf("ERROR: Material lookup failed\n");
        return 1;
    }
    printf("  Initial μ_r:      %.0f\n", mat->mu_i);
    printf("  Saturation B_sat: %.2f T\n", mat->b_sat);
    printf("  Curie Temperature: %.0f °C\n", mat->curie_temp);
    printf("  Resistivity:      %.1f Ω·m\n", mat->resistivity);

    /* Define core geometry (25 mm OD toroid) */
    printf("\n─── Core Geometry: 25 mm Toroid ───\n");
    core_geometry_t geom = core_geom_calculate(
        CORE_SHAPE_TOROID, 0.025, 0.015, 0.010, mat->mu_i);
    printf("  Outer diameter:   %.1f mm\n", geom.od * 1000.0);
    printf("  Inner diameter:   %.1f mm\n", geom.id * 1000.0);
    printf("  Height:           %.1f mm\n", geom.height * 1000.0);
    printf("  Effective A_e:    %.1f mm²\n", geom.ae * 1e6);
    printf("  Effective l_e:    %.1f mm\n", geom.le * 1000.0);
    printf("  AL value:         %.0f nH/turn²\n", geom.al_value);

    /* Create CM choke model (20 turns, bifilar winding) */
    printf("\n─── CM Choke: 20 Turns, Bifilar Winding ───\n");
    cm_choke_model_t *choke = cm_choke_create(mat, &geom, 20.0, 0.99);
    if (choke == NULL) {
        printf("ERROR: Choke creation failed\n");
        core_material_free(mat);
        return 1;
    }
    printf("  CM inductance:    %.3f mH\n", choke->l_cm * 1000.0);
    printf("  DM leakage L:     %.3f µH\n", choke->l_dm_leakage * 1e6);
    printf("  Winding R:        %.2f Ω\n", choke->winding_resistance);
    printf("  Winding C:        %.1f pF\n", choke->winding_capacitance * 1e12);
    printf("  Rated DM I:       %.2f A\n", choke->rated_current_dm);
    printf("  Coupling coeff:   %.3f\n", choke->coupling_coef);

    /* Frequency sweep: CM and DM impedance */
    printf("\n─── Frequency Sweep: CM and DM Impedance ───\n");
    printf("  %-12s %-16s %-16s %-12s\n", "Freq", "|Z_cm| [Ω]", "|Z_dm| [Ω]", "CMRR [dB]");
    double freqs[] = {1e3, 10e3, 50e3, 100e3, 500e3, 1e6, 5e6, 10e6};
    for (int i = 0; i < 8; i++) {
        complex_t z_cm = cm_choke_cm_impedance(choke, freqs[i]);
        complex_t z_dm = cm_choke_dm_impedance(choke, freqs[i]);
        double mag_cm = complex_mag(z_cm);
        double mag_dm = complex_mag(z_dm);
        double cmrr = (mag_dm > 1e-30) ? 20.0 * log10(mag_cm / mag_dm) : 120.0;
        printf("  %-12.0f %-16.1f %-16.3f %-12.1f\n",
               freqs[i], mag_cm, mag_dm, cmrr);
    }

    /* Saturation check at maximum CM voltage */
    printf("\n─── Saturation Analysis ───\n");
    double v_cm_test[] = {1.0, 5.0, 20.0, 50.0, 100.0};
    for (int i = 0; i < 5; i++) {
        int sat_10k = cm_choke_check_saturation(choke, v_cm_test[i], 10e3);
        int sat_100k = cm_choke_check_saturation(choke, v_cm_test[i], 100e3);
        printf("  V_cm=%.0f V: %s @10kHz, %s @100kHz\n",
               v_cm_test[i],
               sat_10k ? "SATURATED" : "OK",
               sat_100k ? "SATURATED" : "OK");
    }

    /* Core loss at 100 kHz */
    printf("\n─── Core Loss at 100 kHz ───\n");
    double b_peak = 0.05;  /* 50 mT typical */
    double p_loss = cm_choke_core_loss(choke, b_peak, 100e3);
    printf("  B_peak = %.0f mT → Core loss = %.3f W\n", b_peak * 1000.0, p_loss);
    printf("  Core volume: %.1f cm³\n", geom.ve * 1e6);

    /* CMRR estimate */
    printf("\n─── CMRR at 100 kHz ───\n");
    complex_t zc = cm_choke_cm_impedance(choke, 100e3);
    complex_t zd = cm_choke_dm_impedance(choke, 100e3);
    cmrr_result_t *cr = cmrr_calculate(complex_mag(zc), complex_mag(zd), 100e3);
    if (cr) {
        printf("  Z_cm = %.1f Ω, Z_dm = %.3f Ω\n",
               cr->cm_impedance, cr->dm_impedance);
        printf("  CMRR = %.1f dB\n\n", cr->cmrr_db);
        cmrr_free(cr);
    }

    /* DC bias effect */
    printf("─── DC Bias Effect (at rated 2.5 A) ───\n");
    double l_biased = inductance_under_dc_bias(
        choke->l_cm, 2.5, choke->rated_current_dm);
    printf("  CM inductance at 2.5 A DC: %.3f mH (%.1f%% of zero-bias)\n",
           l_biased * 1000.0, (l_biased / choke->l_cm) * 100.0);

    /* Temperature derating */
    temp_derating_t td = temperature_derating(mat, 85.0);
    printf("\n─── Temperature Derating at 85°C ───\n");
    printf("  μ_r ratio:   %.2f×\n", td.mu_ratio_to_25c);
    printf("  B_sat ratio: %.2f×\n", td.b_sat_ratio_to_25c);
    printf("  Flux derating factor: %.2f\n", td.recommended_derating);

    cm_choke_free(choke);
    core_material_free(mat);

    printf("\n╔══════════════════════════════════════════════════════════╗\n");
    printf("║  CM Choke Analysis Complete                              ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n");
    return 0;
}