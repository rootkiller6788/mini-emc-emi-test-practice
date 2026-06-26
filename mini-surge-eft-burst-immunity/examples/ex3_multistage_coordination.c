/**
 * ex3_multistage_coordination.c - Multi-Stage Protection Coordination
 *
 * L6 Canonical Problem: Design and verify a three-stage surge protection
 * system for industrial equipment, demonstrating proper coordination
 * between GDT (primary), MOV (secondary), and TVS (tertiary) stages.
 *
 * This example demonstrates:
 *   1. Three-stage protection scheme design
 *   2. Energy distribution across stages
 *   3. Coordination verification
 *   4. Residual voltage prediction
 *   5. Thermal budget analysis
 *
 * Reference: Standler, "Protection of Electronic Circuits from Overvoltages"
 *            IEC 61643-12: SPD Selection and Application
 *            Boeing 747 surge protection design principles
 */

#include "surge_defs.h"
#include "surge_waveform.h"
#include "surge_protection.h"
#include "surge_coupling.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Energy analysis functions (declared in surge_energy.c, used here) */
extern double surge_energy_in_protector(double v_clamp, double i_peak,
                                         double t_duration_us);
extern double surge_generator_stored_energy(surge_waveform_type_t type,
                                              double v_charge_kv);
extern int surge_mov_sizing(double energy_per_pulse, int num_pulses,
                             double rep_rate, int *disk_mm, double *derated_e);
extern int surge_check_thermal_runaway(const protection_device_params_t *dev,
                                        double v_line, double t_ambient,
                                        int *is_stable);

int main(void)
{
    printf("=== Multi-Stage Surge Protection Coordination ===\n");
    printf("Industrial Equipment: 230V AC, 4 kV Surge, ISO 7637-2\n\n");

    /* Step 1: Define the surge threat (severe industrial) */
    printf("Step 1: Surge Threat Definition\n");
    double v_surge_kv = 4.0;  /* IEC Level 4 */
    double z_source = 2.0;    /* IEC combination wave */
    double i_surge_ka = v_surge_kv / z_source;  /* 2 kA */
    double v_line_max = 375.0;  /* 230Vrms + 15% tolerance = 375V peak */
    double v_withstand = 50.0;  /* Sensitive electronics: max 50V residual */

    printf("  Surge: %.1f kV, %.1f kA (IEC 61000-4-5 Level 4)\n",
           v_surge_kv, i_surge_ka);
    printf("  Mains: %.0f V peak\n", v_line_max);
    printf("  Protected circuit withstand: %.0f V\n\n", v_withstand);

    /* Step 2: Design three-stage protection */
    printf("Step 2: Three-Stage Protection Design\n");
    printf("  Architecture:\n");
    printf("    Surge Input -> [GDT] -> [Z_decouple] -> [MOV] -> [Z_decouple] -> [TVS] -> Load\n");
    printf("\n");

    multistage_protection_t mp;
    memset(&mp, 0, sizeof(mp));
    mp.num_stages = 3;

    /* Stage 1: GDT for primary energy handling (ka-level) */
    mp.stage_types[0] = PROT_STAGE_COARSE;
    printf("  Stage 1 - GDT (Primary):\n");
    int ret = surge_select_gdt(&mp.devices[0], v_line_max, i_surge_ka * 1000.0, 600.0);
    if (ret != SURGE_OK) {
        /* If GDT selection fails, use MOV as primary */
        printf("    GDT selection failed, using MOV as primary instead\n");
        ret = surge_select_mov(&mp.devices[0], v_line_max, i_surge_ka * 1000.0, 600.0);
        mp.stage_types[0] = PROT_STAGE_COARSE;
    }
    printf("    Device: %s\n", mp.devices[0].part_number);
    printf("    V_spark/V_1mA: %.0f V\n", mp.devices[0].v_breakdown_max);
    printf("    I_peak rated: %.0f A\n", mp.devices[0].i_peak_rated);
    printf("    V_clamp: %.1f V\n\n", mp.devices[0].v_clamp_rated);

    /* Stage 2: MOV for intermediate energy handling */
    mp.stage_types[1] = PROT_STAGE_FINE;
    double i_residual_1 = i_surge_ka * 1000.0 * 0.15;  /* 15% residual current */
    printf("  Stage 2 - MOV (Secondary):\n");
    ret = surge_select_mov(&mp.devices[1], v_line_max * 0.8, i_residual_1, 200.0);
    printf("    Device: %s\n", mp.devices[1].part_number);
    printf("    V_1mA: %.0f V\n", mp.devices[1].v_breakdown_max);
    printf("    I_peak rated: %.0f A\n", mp.devices[1].i_peak_rated);
    printf("    V_clamp: %.1f V\n\n", mp.devices[1].v_clamp_rated);

    /* Stage 3: TVS for precision clamping */
    mp.stage_types[2] = PROT_STAGE_ONBOARD;
    double i_residual_2 = i_residual_1 * 0.1;  /* 10% of residual */
    printf("  Stage 3 - TVS (Tertiary):\n");
    ret = surge_select_tvs(&mp.devices[2], 30.0, i_residual_2, 50.0, 1);
    printf("    Device: %s\n", mp.devices[2].part_number);
    printf("    V_standoff: %.0f V\n", mp.devices[2].v_standoff);
    printf("    V_BR: %.0f V\n", mp.devices[2].v_breakdown_max);
    printf("    V_clamp: %.1f V\n", mp.devices[2].v_clamp_rated);
    printf("    Response time: %.0f ps\n\n", mp.devices[2].response_time_ps);

    /* Step 3: Decoupling network design */
    printf("Step 3: Decoupling Network Design\n");
    /* Decoupling between GDT and MOV */
    mp.decoupling_R[0] = 2.0;   /* 2 ohms */
    mp.decoupling_L[0] = 15.0;  /* 15 uH */
    /* Decoupling between MOV and TVS */
    mp.decoupling_R[1] = 1.0;   /* 1 ohm */
    mp.decoupling_L[1] = 5.0;   /* 5 uH */

    printf("  Z1 (GDT->MOV): R = %.1f ohm, L = %.1f uH\n",
           mp.decoupling_R[0], mp.decoupling_L[0]);
    printf("    Impedance @ 10 kHz: %.1f ohm\n",
           mp.decoupling_R[0] + 2.0 * M_PI * 10000.0 * mp.decoupling_L[0] * 1e-6);
    printf("  Z2 (MOV->TVS): R = %.1f ohm, L = %.1f uH\n",
           mp.decoupling_R[1], mp.decoupling_L[1]);
    printf("    Impedance @ 10 kHz: %.1f ohm\n\n",
           mp.decoupling_R[1] + 2.0 * M_PI * 10000.0 * mp.decoupling_L[1] * 1e-6);

    /* Step 4: Energy distribution */
    printf("Step 4: Energy Distribution Analysis\n");
    surge_waveform_params_t wf;
    surge_waveform_init(&wf, SURGE_WAVE_1_2_50_US, v_surge_kv, z_source);

    double e_total = surge_energy_resistive(v_surge_kv, wf.tau1,
                                             wf.tau2, z_source);
    printf("  Total surge energy: %.2f J\n", e_total);

    /* Manual energy distribution estimate based on stage characteristics */
    double e_stage1 = e_total * 0.70;  /* GDT absorbs 70% */
    double e_stage2 = e_total * 0.25;  /* MOV absorbs 25% */
    double e_stage3 = e_total * 0.05;  /* TVS absorbs 5% */

    printf("  Energy distribution (estimated):\n");
    printf("    Stage 1 (GDT): %.2f J (%.0f%%) - rating: %.1f J\n",
           e_stage1, 70.0, mp.devices[0].energy_rating);
    printf("    Stage 2 (MOV): %.2f J (%.0f%%) - rating: %.1f J\n",
           e_stage2, 25.0, mp.devices[1].energy_rating);
    printf("    Stage 3 (TVS): %.2f J (%.0f%%) - rating: %.1f J\n",
           e_stage3, 5.0, mp.devices[2].energy_rating);
    printf("\n");

    /* Check for overstress */
    int s;
    double energies[] = {e_stage1, e_stage2, e_stage3};
    int any_overstress = 0;
    for (s = 0; s < 3; s++) {
        if (energies[s] > mp.devices[s].energy_rating) {
            printf("  WARNING: Stage %d overstress! %.2f J > %.1f J rating\n",
                   s+1, energies[s], mp.devices[s].energy_rating);
            any_overstress = 1;
        }
    }
    if (!any_overstress) {
        printf("  All stages within energy ratings.\n");
    }
    printf("\n");

    /* Step 5: Coordination verification */
    printf("Step 5: Coordination Verification\n");
    printf("  Voltage handoff check:\n");
    printf("    Stage 1 V_clamp (%.0f V) < Stage 2 V_breakdown (%.0f V)? %s\n",
           mp.devices[0].v_clamp_rated, mp.devices[1].v_breakdown_min,
           mp.devices[0].v_clamp_rated < mp.devices[1].v_breakdown_min ?
           "YES (coordinated)" : "NO (coordination failure)");
    printf("    Stage 2 V_clamp (%.0f V) < Stage 3 V_breakdown (%.0f V)? %s\n",
           mp.devices[1].v_clamp_rated, mp.devices[2].v_breakdown_min,
           mp.devices[1].v_clamp_rated < mp.devices[2].v_breakdown_min ?
           "YES (coordinated)" : "NO (coordination failure)");
    printf("\n");

    /* Step 6: Residual voltage at load */
    printf("Step 6: Residual Voltage Prediction\n");
    double v_res = mp.devices[2].v_clamp_rated + 5.0 * (mp.decoupling_R[0] +
                   mp.decoupling_R[1]);
    printf("  V_residual = V_clamp(TVS) + I * Sum(R_decouple)\n");
    printf("  V_residual = %.1f + 5 * (%.1f + %.1f) = %.1f V\n",
           mp.devices[2].v_clamp_rated, mp.decoupling_R[0], mp.decoupling_R[1], v_res);
    printf("  V_withstand = %.0f V\n", v_withstand);
    printf("  Protection status: %s\n\n",
           v_res < v_withstand ? "OK (residual < withstand)" :
           "FAIL (residual exceeds withstand)");

    /* Step 7: Generator stored energy comparison */
    printf("Step 7: Generator Energy vs Protection Rating\n");
    double e_gen = surge_generator_stored_energy(SURGE_WAVE_1_2_50_US, v_surge_kv);
    printf("  Surge generator stored energy: %.1f J\n", e_gen);
    printf("  Total protection energy capacity: %.1f J\n",
           mp.devices[0].energy_rating + mp.devices[1].energy_rating +
           mp.devices[2].energy_rating);
    printf("  Generator energy / Protection capacity: %.1f%%\n",
           e_gen / (mp.devices[0].energy_rating + mp.devices[1].energy_rating +
           mp.devices[2].energy_rating) * 100.0);

    /* Step 8: MOV sizing for given energy */
    printf("\nStep 8: MOV Sizing for Repetitive Surges\n");
    int disk_mm;
    double derated_e;
    surge_mov_sizing(e_stage2, 10, 0.2, &disk_mm, &derated_e);
    printf("  For %.2f J, 10 pulses @ 0.2 Hz:\n", e_stage2);
    printf("    Recommended MOV disk: %d mm\n", disk_mm);
    printf("    Derated energy rating: %.1f J\n\n", derated_e);

    printf("=== Multi-Stage Protection Design Complete ===\n");
    return 0;
}