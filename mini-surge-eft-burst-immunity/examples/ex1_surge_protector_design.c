/**
 * ex1_surge_protector_design.c - Complete Surge Protector Design Example
 *
 * L6 Canonical Problem: Design a surge protector for a 230V AC mains input
 * compliant with IEC 61000-4-5 Level 3 (2 kV surge, 1 kA current).
 *
 * This example demonstrates:
 *   1. Waveform generation for IEC 1.2/50us - 8/20us combination wave
 *   2. MOV device selection and validation
 *   3. Clamping voltage prediction
 *   4. Energy absorption calculation
 *   5. Thermal analysis and margin checking
 *
 * Reference: IEC 61000-4-5 Class 3 installation requirements
 */

#include "surge_defs.h"
#include "surge_waveform.h"
#include "surge_protection.h"
#include <stdio.h>
#include <math.h>

int main(void)
{
    printf("=== Surge Protector Design: 230V AC Mains, IEC 61000-4-5 Level 3 ===\n\n");

    /* Step 1: Define the surge threat */
    double v_mains_rms = 230.0;
    double v_mains_peak = v_mains_rms * sqrt(2.0);  /* 325V peak */
    double v_surge_kv = surge_test_voltage_kv(SURGE_LEVEL_3);  /* 2 kV */

    printf("Step 1: Surge Threat Definition\n");
    printf("  Mains voltage: %.1f Vrms (%.1f Vpeak)\n", v_mains_rms, v_mains_peak);
    printf("  Surge level: Level 3 = %.1f kV\n", v_surge_kv);
    printf("  Waveform: 1.2/50us voltage, 8/20us current\n");
    printf("  Source impedance: 2 ohms (combination wave)\n\n");

    /* Step 2: Initialize the surge waveform */
    surge_waveform_params_t surge;
    int ret = surge_waveform_init(&surge, SURGE_WAVE_1_2_50_US,
                                   v_surge_kv, 2.0);
    if (ret != SURGE_OK) {
        printf("ERROR: Waveform initialization failed\n");
        return 1;
    }

    double i_surge_peak = v_surge_kv * 1000.0 / 2.0;  /* V/R = 2kV/2ohm = 1kA */
    surge.i_peak = i_surge_peak / 1000.0;  /* Store as kA */

    printf("Step 2: Surge Waveform Parameters\n");
    printf("  Type: %s\n", surge_waveform_name(surge.type));
    printf("  V_peak: %.2f kV\n", surge.v_peak);
    printf("  I_peak: %.1f A (%.3f kA)\n", i_surge_peak, surge.i_peak);
    printf("  Front time: %.1f us\n", surge.t_front);
    printf("  Duration: %.1f us\n", surge.t_duration);
    printf("  Tau1 = %.4f us, Tau2 = %.2f us\n", surge.tau1, surge.tau2);
    printf("  Source impedance: %.1f ohms\n", surge.source_impedance);

    /* Compute waveform characteristics */
    double t_peak = surge_peak_time(surge.tau1, surge.tau2);
    double k = surge_normalization_factor(surge.tau1, surge.tau2);
    double v_peak_actual = surge_double_exponential(t_peak, surge.v_peak,
                                                     surge.tau1, surge.tau2);
    double tr = surge_rise_time_10_90(surge.tau1, surge.tau2, 1e-6);
    double pw = surge_pulse_width_fwhm(surge.tau1, surge.tau2, 1e-6);

    printf("\n  Waveform Analysis:\n");
    printf("    Peak time: %.3f us\n", t_peak);
    printf("    Normalization factor k: %.4f\n", k);
    printf("    v(t_peak) = %.3f kV (should equal V_peak)\n", v_peak_actual);
    printf("    Rise time (10-90%%): %.2f us\n", tr);
    printf("    Pulse width (FWHM): %.2f us\n", pw);

    /* Compute energy */
    surge.energy_per_pulse = surge_energy_resistive(surge.v_peak,
                              surge.tau1, surge.tau2, surge.source_impedance);
    double i2t_val = surge_i2t(i_surge_peak, surge.tau1, surge.tau2);
    double q_c = surge_charge_transfer(i_surge_peak, surge.tau1, surge.tau2);

    printf("    Energy (2-ohm load): %.2f J\n", surge.energy_per_pulse);
    printf("    I^2t: %.2f A^2*s\n", i2t_val);
    printf("    Charge transfer: %.6f C\n\n", q_c);

    /* Step 3: Select MOV protection device */
    printf("Step 3: MOV Protection Device Selection\n");
    printf("  Requirements:\n");
    printf("    V_line_max = %.1f V\n", v_mains_peak);
    printf("    I_surge = %.0f A\n", i_surge_peak);
    printf("    V_withstand (assumed) = 600 V\n\n");

    double v_withstand = 600.0;  /* Protected circuit can withstand 600V */
    protection_device_params_t mov_dev;
    ret = surge_select_mov(&mov_dev, v_mains_peak, i_surge_peak, v_withstand);

    if (ret != SURGE_OK) {
        printf("  FAILED: No suitable MOV found (error %d)\n", ret);
        printf("  Consider using multi-stage protection (GDT + MOV + TVS)\n");
        return 1;
    }

    printf("  Selected MOV:\n");
    printf("    Part number: %s\n", mov_dev.part_number);
    printf("    Type: Metal Oxide Varistor\n");
    printf("    V_1mA: %.0f V\n", mov_dev.v_breakdown_max);
    printf("    Alpha (non-linearity): %.1f\n", mov_dev.alpha);
    printf("    I_peak rated: %.0f A\n", mov_dev.i_peak_rated);
    printf("    I_peak max: %.0f A\n", mov_dev.i_peak_max);
    printf("    Energy rating: %.1f J\n", mov_dev.energy_rating);
    printf("    Clamping voltage @ surge: %.1f V\n", mov_dev.v_clamp_rated);
    printf("    Capacitance: %.0f pF\n", mov_dev.capacitance_typ);
    printf("    Clamping ratio: %.2f\n\n", mov_dev.clamping_ratio);

    /* Step 4: Validate the selected device */
    printf("Step 4: Device Validation\n");
    ret = surge_validate_device(&mov_dev, &surge, v_withstand);
    if (ret == SURGE_OK) {
        printf("  VALIDATION: PASSED\n");
        printf("    Peak current: %.0f A < %.0f A (rated)\n",
               i_surge_peak, mov_dev.i_peak_max);
        printf("    Clamp voltage: %.1f V < %.0f V (withstand)\n",
               mov_dev.v_clamp_rated, v_withstand);
        printf("    Energy: %.2f J < %.1f J (rating)\n",
               surge.energy_per_pulse, mov_dev.energy_rating);
    } else {
        printf("  VALIDATION: FAILED (error code %d)\n", ret);
    }

    /* Step 5: Protection margin analysis */
    double margin = surge_protection_margin(&mov_dev, &surge, v_withstand);
    printf("\nStep 5: Protection Margin Analysis\n");
    printf("  Margin: %.2fx\n", margin);
    if (margin >= 2.0) {
        printf("  Assessment: EXCELLENT (>2x margin, suitable for industrial)\n");
    } else if (margin >= 1.5) {
        printf("  Assessment: GOOD (>1.5x margin, suitable for commercial)\n");
    } else if (margin >= 1.0) {
        printf("  Assessment: MARGINAL (>1x, consider next size up)\n");
    } else {
        printf("  Assessment: FAILED (<1x, redesign required)\n");
    }

    /* Step 6: Spectrum analysis */
    printf("\nStep 6: Frequency Domain Analysis\n");
    double fc = surge_corner_frequency(surge.tau1, surge.tau2, surge.v_peak);
    printf("  Corner frequency (-3dB): %.1f Hz (%.1f kHz)\n", fc, fc/1000.0);

    double f_test[] = {100.0, 1000.0, 10000.0, 100000.0, 1000000.0};
    int i;
    for (i = 0; i < 5; i++) {
        double mag = surge_spectrum_magnitude(f_test[i], surge.v_peak,
                                               surge.tau1, surge.tau2);
        double mag_dc = surge_spectrum_magnitude(0.0, surge.v_peak,
                                                  surge.tau1, surge.tau2);
        double att_db = 20.0 * log10(mag / mag_dc);
        printf("    f = %8.0f Hz: |V(f)| = %.2f kV, attenuation = %.1f dB\n",
               f_test[i], mag, att_db);
    }

    /* Step 7: Summary */
    printf("\n=== Design Summary ===\n");
    printf("  Application: 230V AC mains surge protection\n");
    printf("  Standard: IEC 61000-4-5 Level 3 (2 kV, 1 kA)\n");
    printf("  Protection device: %s\n", mov_dev.part_number);
    printf("  Protection margin: %.2fx\n", margin);
    printf("  Design status: %s\n",
           margin >= 1.5 ? "ACCEPTABLE" : "NEEDS IMPROVEMENT");

    return 0;
}