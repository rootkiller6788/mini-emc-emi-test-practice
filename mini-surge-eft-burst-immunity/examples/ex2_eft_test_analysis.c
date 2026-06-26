/**
 * ex2_eft_test_analysis.c - EFT/Burst Immunity Test Simulation
 *
 * L6 Canonical Problem: Simulate an EFT burst immunity test as per
 * IEC 61000-4-4, including burst pattern generation, coupling clamp
 * modeling, filter design, and statistical analysis.
 *
 * This example demonstrates:
 *   1. EFT burst parameter setup for all test levels
 *   2. Individual pulse and burst pattern generation
 *   3. Coupling clamp response analysis
 *   4. EFT filter design and attenuation prediction
 *   5. Statistical worst-case analysis
 *
 * Reference: IEC 61000-4-4: EFT/Burst Immunity Test
 */

#include "surge_defs.h"
#include "surge_waveform.h"
#include "surge_burst.h"
#include <stdio.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int main(void)
{
    printf("=== EFT/Burst Immunity Test Analysis (IEC 61000-4-4) ===\n\n");

    /* Step 1: EFT test level overview */
    printf("Step 1: IEC 61000-4-4 Test Level Overview\n");
    printf("  %-12s %-12s %-12s %-12s\n",
           "Level", "V_peak(kV)", "Rep(kHz)", "Pulses/burst");
    printf("  %-12s %-12s %-12s %-12s\n",
           "----------", "----------", "----------", "----------");

    eft_test_level_t levels[] = {EFT_LEVEL_1, EFT_LEVEL_2, EFT_LEVEL_3, EFT_LEVEL_4};
    const char *level_names[] = {"Level 1", "Level 2", "Level 3", "Level 4"};
    int i;

    for (i = 0; i < 4; i++) {
        eft_burst_params_t burst;
        eft_burst_init(&burst, levels[i]);
        printf("  %-12s %-12.1f %-12.1f %-12d\n",
               level_names[i], burst.v_peak, burst.repetition_khz,
               burst.pulses_per_burst);
    }
    printf("\n");

    /* Step 2: Detailed analysis of Level 3 EFT burst */
    printf("Step 2: Detailed EFT Level 3 Burst Analysis\n");
    eft_burst_params_t burst;
    int ret = eft_burst_init(&burst, EFT_LEVEL_3);
    (void)ret;  /* ignore init return, params verified below */

    printf("  Individual pulse:\n");
    printf("    Rise time: %.1f ns\n", burst.pulse_rise_ns);
    printf("    Pulse width: %.1f ns\n", burst.pulse_width_ns);
    printf("    Peak voltage: %.1f kV\n", burst.v_peak);
    printf("\n");
    printf("  Burst characteristics:\n");
    printf("    Burst duration: %.1f ms\n", burst.burst_duration_ms);
    printf("    Burst period: %.1f ms\n", burst.burst_period_ms);
    printf("    Repetition rate: %.1f kHz\n", burst.repetition_khz);
    printf("    Pulses per burst: %d\n", burst.pulses_per_burst);
    printf("    Coupling capacitance: %.0f pF (%.1f nF)\n",
           burst.coupling_cap_pf, burst.coupling_cap_pf/1000.0);
    printf("    Test duration: %.0f s (%.1f min)\n",
           burst.test_duration_sec, burst.test_duration_sec/60.0);
    printf("\n");

    /* Step 3: Generate burst pulse timing */
    printf("Step 3: Burst Pulse Timing\n");
    double pulse_times[100];
    int n_pulses = 100;
    ret = eft_burst_generate(pulse_times, &n_pulses, &burst);
    printf("  First 5 pulse times (us):\n");
    for (i = 0; i < 5 && i < n_pulses; i++) {
        printf("    Pulse %2d: t = %.1f us\n", i+1, pulse_times[i]);
    }
    printf("  ...\n");
    printf("  Pulse period: %.1f us (%.3f ms)\n\n",
           1000.0/burst.repetition_khz, 1.0/burst.repetition_khz);

    /* Step 4: Individual pulse analysis */
    printf("Step 4: Individual EFT Pulse Waveform\n");
    double t_samples[] = {0.0, 1.0, 3.0, 5.0, 10.0, 20.0, 50.0, 100.0, 200.0}; /* ns */
    printf("  Time (ns)     V(t) (kV)\n");
    printf("  ----------    ---------\n");
    for (i = 0; i < 9; i++) {
        double v_pulse = eft_pulse(t_samples[i] * 1e-3, 0.0, burst.v_peak);
        printf("  %10.1f    %10.3f\n", t_samples[i], v_pulse);
    }
    printf("\n");

    /* Step 5: Coupling clamp response */
    printf("Step 5: Capacitive Coupling Clamp Response\n");
    double z_source = 50.0;  /* ohms */
    printf("  Clamp capacitance: %.0f pF\n", burst.coupling_cap_pf);
    printf("  Source impedance: %.0f ohms\n", z_source);

    double fc_clamp = 1.0 / (2.0 * M_PI * z_source *
                      burst.coupling_cap_pf * 1e-12);
    printf("  Corner frequency: %.1f kHz\n", fc_clamp/1000.0);

    double freq_test[] = {1000.0, 10000.0, 100000.0, 1e6, 10e6, 100e6};
    printf("\n  Frequency      |H(f)|       Attenuation\n");
    printf("  -------------  ---------    -----------\n");
    for (i = 0; i < 6; i++) {
        double h = eft_coupling_clamp_response(freq_test[i],
                    burst.coupling_cap_pf, z_source);
        double att_db = -20.0 * log10(h > 0.001 ? h : 0.001);
        printf("  %9.0f Hz    %9.4f      %6.1f dB\n",
               freq_test[i], h, att_db);
    }
    printf("\n");

    /* Step 6: Delivered voltage to EUT */
    printf("Step 6: Delivered Voltage to EUT\n");
    double z_eut_values[] = {50.0, 500.0, 5000.0, 50000.0};
    printf("  Z_EUT (ohms)   V_delivered (kV)   V/Vgen\n");
    printf("  -------------  -----------------  -------\n");
    for (i = 0; i < 4; i++) {
        double v_del = eft_delivered_voltage(burst.v_peak, z_eut_values[i],
                        burst.coupling_cap_pf, burst.pulse_rise_ns);
        printf("  %13.0f  %17.3f  %8.1f%%\n",
               z_eut_values[i], v_del, v_del/burst.v_peak*100.0);
    }
    printf("\n");

    /* Step 7: EFT filter design */
    printf("Step 7: EFT Suppression Filter Design\n");
    printf("  Designing filters for different topologies:\n\n");
    printf("  %-15s %-12s %-12s %-12s\n",
           "Topology", "fc (kHz)", "L (uH)", "Att@100MHz");
    printf("  %-15s %-12s %-12s %-12s\n",
           "--------", "--------", "--------", "--------");

    eft_filter_topology_t topos[] = {EFT_FILTER_LC, EFT_FILTER_PI,
                                      EFT_FILTER_T, EFT_FILTER_CM_CHOKE,
                                      EFT_FILTER_FERRITE};
    const char *topo_names[] = {"LC", "Pi (C-L-C)", "T (L-C-L)",
                                 "CM Choke", "Ferrite Bead"};

    for (i = 0; i < 5; i++) {
        eft_filter_params_t filter;
        eft_filter_design(&filter, topos[i], 10000.0, 5, 500.0);
        printf("  %-15s %-12.1f %-12.1f %-12.1f\n",
               topo_names[i], filter.cutoff_freq_hz/1000.0,
               filter.inductance_uh, filter.attenuation_at_100mhz_db);
    }
    printf("\n");

    /* Step 8: Statistical worst-case analysis */
    printf("Step 8: Worst-Case EFT Stress Analysis\n");
    double w_v, w_dvdt, w_energy;
    eft_worst_case_stress(&burst, &w_v, &w_dvdt, &w_energy);
    printf("  Worst-case peak voltage: %.2f kV\n", w_v);
    printf("  Worst-case dV/dt: %.1f kV/us\n", w_dvdt);
    printf("  Worst-case energy per burst: %.3f J\n", w_energy);

    double avg_rate = eft_effective_pulse_rate(&burst);
    printf("  Average pulse rate (thermal): %.1f Hz\n", avg_rate);

    /* Step 9: Amplitude distribution modeling */
    printf("\nStep 9: Statistical Amplitude Variation\n");
    double amplitudes[75];
    eft_burst_amplitude_distribution(amplitudes, 75, burst.v_peak, 5.0);
    double amp_min = amplitudes[0], amp_max = amplitudes[0], amp_sum = 0.0;
    for (i = 0; i < 75; i++) {
        if (amplitudes[i] < amp_min) amp_min = amplitudes[i];
        if (amplitudes[i] > amp_max) amp_max = amplitudes[i];
        amp_sum += amplitudes[i];
    }
    printf("  With 5%% std dev Gaussian variation:\n");
    printf("    Min amplitude: %.3f kV\n", amp_min);
    printf("    Max amplitude: %.3f kV\n", amp_max);
    printf("    Mean amplitude: %.3f kV\n", amp_sum/75.0);
    printf("    Nominal: %.3f kV\n\n", burst.v_peak);

    printf("=== Analysis Complete ===\n");
    return 0;
}