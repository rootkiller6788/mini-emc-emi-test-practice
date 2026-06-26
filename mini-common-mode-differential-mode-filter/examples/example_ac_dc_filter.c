/**
 * @file example_ac_dc_filter.c
 * @brief Complete AC-DC SMPS EMI filter design example (L6 Canonical Problem)
 *
 * Demonstrates end-to-end EMI filter design for a 65W laptop adapter:
 *  1. Define the system specification (230V, 65W, 65 kHz flyback)
 *  2. Estimate typical conducted EMI noise spectrum
 *  3. Compute required insertion loss to meet CISPR 32 Class B
 *  4. Design multi-stage CM/DM filter
 *  5. Verify compliance margin
 *  6. Report final filter parameters
 *
 * This is the standard workflow for an EMC engineer designing a
 * conducted emission filter for an AC-DC power supply.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "cm_dm_filter.h"

int main(void) {
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║  AC-DC SMPS EMI Filter Design Example                    ║\n");
    printf("║  65W Laptop Adapter — CISPR 32 Class B Compliance       ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");

    /* Step 1: System Specification */
    printf("─── Step 1: System Specification ───\n");
    printf("  Input:   230 V_rms, 50 Hz\n");
    printf("  Output:  19.5 V DC, 3.33 A (65 W)\n");
    printf("  Topology: Flyback converter @ 65 kHz\n");
    printf("  Target:   CISPR 32 Class B conducted emissions\n\n");

    /* Step 2: Get preset design specification */
    printf("─── Step 2: Load Design Preset ───\n");
    filter_design_spec_t *spec = filter_preset_get(APP_AC_DC_SMPS);
    if (spec == NULL) {
        printf("ERROR: Could not create preset\n");
        return 1;
    }
    printf("  V_nominal:    %.0f V\n", spec->v_nominal);
    printf("  I_nominal:    %.2f A\n", spec->i_nominal);
    printf("  f_switching:  %.0f Hz\n", spec->f_switching);
    printf("  f_grid:       %.0f Hz\n", spec->f_grid);
    printf("  EMC Standard: CISPR 32 Class B\n");
    printf("  Design margin: %.0f dB\n\n", spec->design_margin_db);

    /* Step 3: Get EMC limits */
    printf("─── Step 3: EMC Limit Reference ───\n");
    emc_limit_summary_t limit = emc_limit_summary_get(EMC_STD_CISPR32, 0);
    printf("  %s\n", limit.description);
    printf("  150 kHz QP limit: %.0f dBµV\n", limit.limit_150k_qp_dbuV);
    printf("  500 kHz QP limit: %.0f dBµV\n", limit.limit_500k_qp_dbuV);
    printf("  5 MHz QP limit:   %.0f dBµV\n", limit.limit_5M_qp_dbuV);
    printf("  30 MHz QP limit:  %.0f dBµV\n\n", limit.limit_30M_qp_dbuV);

    /* Step 4: Simulate noise spectrum (representative values) */
    printf("─── Step 4: Noise Spectrum Estimate ───\n");
    #define N_FREQS 8
    double f_vals[N_FREQS] = {65e3, 130e3, 195e3, 325e3, 650e3, 1.3e6, 5e6, 10e6};
    double n_vals[N_FREQS] = {95.0, 85.0, 78.0, 72.0, 65.0, 60.0, 55.0, 50.0};
    noise_spectrum_t noise = {f_vals, n_vals, N_FREQS, 9e3, 0, 0.0};

    printf("  Frequency Bins and Estimated Noise:\n");
    for (size_t i = 0; i < N_FREQS; i++) {
        printf("    %8.0f Hz:  %.0f dBµV", f_vals[i], n_vals[i]);
        double lim_val;
        if (f_vals[i] < 500e3)
            lim_val = limit.limit_150k_qp_dbuV + (f_vals[i] - 150e3) / 350e3
                      * (limit.limit_500k_qp_dbuV - limit.limit_150k_qp_dbuV);
        else if (f_vals[i] < 5e6)
            lim_val = limit.limit_500k_qp_dbuV;
        else
            lim_val = limit.limit_5M_qp_dbuV;
        printf("  (limit: %.0f dBµV", lim_val);
        if (n_vals[i] > lim_val) printf(" - VIOLATION");
        printf(")\n");
    }
    printf("\n");

    /* Step 5: Compute required attenuation */
    printf("─── Step 5: Required Attenuation ───\n");
    emc_limit_t *emc_lim = emc_limit_get(EMC_STD_CISPR32, 0);
    size_t n_req;
    double *il_req = required_attenuation(&noise, emc_lim, 6.0, &n_req);
    double max_il_needed = 0.0;
    for (size_t i = 0; i < n_req; i++) {
        if (il_req[i] > max_il_needed) max_il_needed = il_req[i];
    }
    printf("  Maximum required IL: %.1f dB (incl. 6 dB margin)\n\n", max_il_needed);

    /* Step 6: Design the filter */
    printf("─── Step 6: Filter Design ───\n");
    filter_design_result_t *design = filter_design_emi(spec, &noise);
    if (design == NULL) {
        printf("ERROR: Filter design failed\n");
        free(il_req);
        emc_limit_free(emc_lim);
        free(spec);
        return 1;
    }

    printf("  Number of stages: %zu\n", design->filter.num_stages);
    printf("  Estimated CM IL:  %.1f dB\n", design->filter.cm_insertion_loss);
    printf("  Estimated DM IL:  %.1f dB\n", design->filter.dm_insertion_loss);
    printf("  DM cutoff freq:   %.0f Hz\n", design->filter.dm_cutoff_freq);
    printf("  CM cutoff freq:   %.0f Hz\n", design->filter.cm_cutoff_freq);
    printf("  Estimated CMRR:   %.1f dB\n", design->filter.cmrr_estimate);
    printf("  Power loss:       %.3f W\n", design->filter.power_loss);
    printf("  EMC margin:       %d dB\n", design->filter.compliance_margin_db);
    printf("  Estimated volume: %.1f cm³\n", design->estimated_volume_cm3);
    printf("  Estimated BOM:   $%.2f\n\n", design->estimated_cost_usd);

    /* Step 7: Report filter stage details */
    printf("─── Step 7: Stage Details ───\n");
    for (size_t s = 0; s < design->filter.num_stages; s++) {
        filter_stage_t *stage = &design->filter.stages[s];
        printf("  Stage %zu: cutoff = %.0f Hz, topology = %d, %zu elements\n",
               s + 1, stage->cutoff_freq_design, (int)stage->topology,
               stage->num_elements);
        for (size_t e = 0; e < stage->num_elements; e++) {
            const char *type_names[] = {"CM choke", "DM inductor",
                "X-capacitor", "Y-capacitor", "Ferrite bead",
                "Resistor", "TVS", "Feedthrough"};
            printf("    - %s: %.2e (value), ±%.0f%% tolerance\n",
                   type_names[stage->elements[e].type],
                   stage->elements[e].nominal_value,
                   stage->elements[e].tolerance_pct);
        }
    }
    printf("\n");

    /* Step 8: Compliance verdict */
    printf("─── Step 8: Compliance Assessment ───\n");
    if (design->filter.compliance_margin_db >= 6) {
        printf("  ✓ PASS — Margin ≥ 6 dB\n");
        printf("  The filter design meets CISPR 32 Class B with margin.\n");
    } else if (design->filter.compliance_margin_db >= 0) {
        printf("  △ MARGINAL PASS — Margin < 6 dB\n");
        printf("  Recommend adding one more stage or using higher-grade components.\n");
    } else {
        printf("  ✗ FAIL — Insufficient attenuation\n");
        printf("  Additional filter stages required.\n");
    }

    /* Step 9: Safety check */
    printf("\n─── Step 9: Safety Considerations ───\n");
    double v_peak = 230.0 * sqrt(2.0);
    double r_bleed = x_cap_bleed_resistor(1e-6, v_peak, 1.0);
    printf("  Required X-cap bleed resistor: ≤ %.0f kΩ\n", r_bleed / 1000.0);
    double i_leak = y_cap_leakage_current(2.2e-9, 50.0, 230.0);
    printf("  Y-cap leakage current (2.2 nF): %.3f mA\n", i_leak * 1000.0);
    printf("  ITE limit: 3.5 mA (stationary) / 0.75 mA (portable)\n");

    /* Cleanup */
    free(il_req);
    emc_limit_free(emc_lim);
    filter_design_result_free(design);
    free(spec);

    printf("\n╔══════════════════════════════════════════════════════════╗\n");
    printf("║  Filter Design Complete                                  ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n");
    return 0;
}