/**
 * @file example_filter_network.c
 * @brief Network Analysis of Cascaded EMI Filters (L6 Canonical Problem)
 *
 * Demonstrates two-port network analysis of multi-stage EMI filters:
 *  1. Build ABCD matrix for a π-filter
 *  2. Convert to Z, Y, and S parameters
 *  3. Compute insertion loss from each representation
 *  4. Cascade two filter stages and analyze total response
 *  5. Compute mixed-mode S-parameters for CM/DM separation
 *  6. Analyze mode conversion from impedance imbalance
 *
 * This illustrates the mathematical framework used to analyze and
 * optimize filter performance using network theory.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "cm_dm_filter.h"
#include "cm_dm_network.h"
#include "dm_filter_model.h"

int main(void) {
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║  EMI Filter Network Analysis                             ║\n");
    printf("║  Two-Port Parameters and Cascading                       ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");

    /* Step 1: Design a π-filter stage */
    printf("─── Step 1: π-Filter Design ───\n");
    dm_lc_params_t params;
    int rc = dm_lc_params_calculate(FILTER_TOPOLOGY_PI, 10e3, 50.0, 50.0, &params);
    if (rc != 0) {
        printf("ERROR: Parameter calculation failed\n");
        return 1;
    }
    printf("  Cutoff frequency: 10 kHz\n");
    printf("  L_series:        %.2f µH\n", params.l_series * 1e6);
    printf("  C_shunt_input:   %.2f nF\n", params.c_shunt_input * 1e9);
    printf("  C_shunt_output:  %.2f nF\n", params.c_shunt_output * 1e9);
    printf("  R_damp (optimal): %.1f Ω\n\n", params.r_damp);

    /* Step 2: Build ABCD matrix at various frequencies */
    printf("─── Step 2: Frequency Response via ABCD Matrix ───\n");
    printf("  %-10s %-14s %-14s %-12s\n", "Freq", "|H(f)|", "IL [dB]", "Phase [°]");
    double freqs[] = {1e3, 5e3, 10e3, 50e3, 100e3, 500e3};
    for (int i = 0; i < 6; i++) {
        complex_t h = dm_transfer_function(FILTER_TOPOLOGY_PI,
            &params, freqs[i], 50.0, 50.0);
        double mag = complex_mag(h);
        double il = 20.0 * log10((50.0 + 50.0) / (50.0 * mag));
        double phase = complex_phase_deg(h);
        printf("  %-10.0f %-14.4f %-14.2f %-12.1f\n", freqs[i], mag, il, phase);
    }
    printf("\n");

    /* Step 3: Convert to S-parameters */
    printf("─── Step 3: S-Parameters at 100 kHz ───\n");
    abcd_matrix_t abcd = build_filter_abcd(FILTER_TOPOLOGY_PI,
        params.l_series, params.c_shunt_input,
        0.0, params.c_shunt_output, 100e3);

    s_matrix_t s = abcd_to_s_matrix(&abcd, 50.0);
    printf("  S11 = %.4f + j%.4f  (mag=%.4f)\n",
           s.s11.real, s.s11.imag, complex_mag(s.s11));
    printf("  S21 = %.4f + j%.4f  (mag=%.4f)\n",
           s.s21.real, s.s21.imag, complex_mag(s.s21));
    printf("  S12 = %.4f + j%.4f  (mag=%.4f)\n",
           s.s12.real, s.s12.imag, complex_mag(s.s12));
    printf("  S22 = %.4f + j%.4f  (mag=%.4f)\n",
           s.s22.real, s.s22.imag, complex_mag(s.s22));

    double il_s = s_to_insertion_loss(&s);
    double rl_in = s_to_return_loss(&s, 1);
    printf("  IL from S21: %.2f dB\n", il_s);
    printf("  Return Loss (input): %.2f dB\n\n", rl_in);

    /* Step 4: Cascade two π-filter stages */
    printf("─── Step 4: Two-Stage Cascade ───\n");
    abcd_matrix_t stage1 = build_filter_abcd(FILTER_TOPOLOGY_PI,
        params.l_series, params.c_shunt_input,
        0.0, params.c_shunt_output, 100e3);
    abcd_matrix_t stage2 = build_filter_abcd(FILTER_TOPOLOGY_PI,
        params.l_series * 0.7, params.c_shunt_input * 0.9,
        0.0, params.c_shunt_output * 0.9, 100e3);

    abcd_matrix_t cascade = abcd_cascade_two(&stage1, &stage2);
    s_matrix_t s_cascade = abcd_to_s_matrix(&cascade, 50.0);
    double il_cascade = s_to_insertion_loss(&s_cascade);
    printf("  Single-stage IL: %.2f dB\n", il_s);
    printf("  Two-stage IL:    %.2f dB\n", il_cascade);
    printf("  Improvement:     %.2f dB (expect ~2× single-stage)\n\n",
           il_cascade - il_s);

    /* Step 5: DM IL worst-case analysis per CISPR 17 */
    printf("─── Step 5: CISPR 17 Worst-Case DM IL at 100 kHz ───\n");
    cispr17_impedance_t worst_cond;
    double il_worst = dm_il_worst_case(&params, 100e3, &worst_cond);
    const char *cond_names[] = {"50Ω/50Ω", "0.1Ω/100Ω", "100Ω/0.1Ω", "Custom"};
    printf("  Worst-case IL: %.2f dB (%s)\n\n",
           il_worst, cond_names[(int)worst_cond]);

    /* Step 6: Mode conversion analysis from imbalance */
    printf("─── Step 6: Mode Conversion from Imbalance ───\n");
    double tol[] = {1.0, 5.0, 10.0, 20.0};
    for (int i = 0; i < 4; i++) {
        path_imbalance_t pi = path_imbalance_from_tolerance(
            50.0, 50.0, tol[i]);
        printf("  Tolerance ±%.0f%%: imbalance = %.4f (%.2f%%), est. LCL = %.1f dB\n",
               tol[i], pi.delta_z, pi.imbalance_ratio * 100.0, pi.expected_lcl_db);
    }
    printf("  (LCL > 40 dB is typically acceptable)\n\n");

    /* Step 7: Ideal theoretical maximum IL */
    printf("─── Step 7: Theoretical Maximum IL ───\n");
    for (int stages = 1; stages <= 4; stages++) {
        double il_ideal = il_ideal_theoretical_max(stages, 10e3, 150e3);
        printf("  %d-stage filter at 150 kHz (15× f_c): %.0f dB IL (ideal)\n",
               stages, il_ideal);
    }
    printf("  Note: Parasitics will reduce actual IL at high frequencies.\n\n");

    /* Step 8: High-frequency degradation estimate */
    printf("─── Step 8: HF Degradation Estimate ───\n");
    printf("  At frequencies > 10 MHz:\n");
    printf("    - CM choke inter-winding C bypasses filter\n");
    printf("    - Y-cap ESL limits high-freq attenuation\n");
    printf("    - PCB trace inductance adds ground impedance\n");
    printf("  Typical IL shelf at 30-100 MHz: 40-60 dB\n");
    printf("  This limits the useful frequency range of passive filters.\n");

    printf("\n╔══════════════════════════════════════════════════════════╗\n");
    printf("║  Network Analysis Complete                               ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n");
    return 0;
}