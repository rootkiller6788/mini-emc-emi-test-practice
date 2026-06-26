/* example_probe_calibration.c - DIY Near-Field Probe Calibration */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "emi_precompliance_core.h"
#include "emi_probe_theory.h"
#include "emi_measurement_setup.h"
#include "emi_spectrum_analysis.h"

int main(void) {
    printf("=== DIY Near-Field Probe Calibration Example ===\n");
    printf("Low-cost probe calibration using TEM cell method\n\n");

    emi_tem_cell_spec_t tem;
    memset(&tem, 0, sizeof(tem));
    tem.cell_length_m = 2.0;
    tem.cell_width_m = 0.8;
    tem.cell_height_m = 0.6;
    tem.septum_height_m = 0.45;
    tem.characteristic_impedance_ohm = 50.0;
    tem.max_freq_hz = 1e9;
    printf("Reference: GTEM cell, septum height = %.2f m\n", tem.septum_height_m);

    emi_hprobe_spec_t hprobe;
    memset(&hprobe, 0, sizeof(hprobe));
    hprobe.loop_diameter_m = 0.03;
    hprobe.num_turns = 2;
    hprobe.wire_diameter_m = 0.0005;
    hprobe.self_inductance_h = emi_hprobe_self_inductance(0.015, 0.00025, 2);
    hprobe.self_resonance_hz = 500e6;
    hprobe.cal_factor_db = 0.0;

    printf("Probe under test: %.0f mm loop, %d turns\n",
           hprobe.loop_diameter_m * 1000.0, hprobe.num_turns);
    printf("Self inductance: %.2f nH\n\n", hprobe.self_inductance_h * 1e9);

    int n_points = 10;
    double freqs[10];
    double v_measured[10];
    double cal_factors[10];

    double log_fmin = log10(10e6);
    double log_fmax = log10(500e6);
    printf("Calibration Frequencies:\n");
    printf("%-6s %-14s %-14s %-16s %s\n",
           "Point", "Freq(MHz)", "V_ideal(mV)", "V_meas(mV)", "K_dB");

    for (int i = 0; i < n_points; i++) {
        double log_f = log_fmin + (log_fmax - log_fmin) * i / (n_points - 1);
        freqs[i] = pow(10.0, log_f);

        double p_in_dbm = 20.0;
        double e_ref = emi_tem_cell_field_strength(p_in_dbm, &tem);
        double h_ref = emi_efield_to_hfield_far(e_ref);

        double v_ideal = emi_hprobe_output_voltage(freqs[i], h_ref, &hprobe);
        double measurement_error_db = ((double)(i * 13 % 5) - 2.0) * 0.3;
        double measurement_error = pow(10.0, measurement_error_db / 20.0);
        v_measured[i] = v_ideal * measurement_error;

        cal_factors[i] = 20.0 * log10(h_ref / v_measured[i]);

        printf("%-6d %-14.3f %-14.3f %-16.3f %-+16.2f\n",
               i + 1, freqs[i] / 1e6, v_ideal * 1000.0,
               v_measured[i] * 1000.0, cal_factors[i]);
    }

    printf("\nProbe Frequency Response:\n");
    printf("%-14s %-16s %s\n", "Freq(MHz)", "AF_H(dB S/m)", "Note");

    for (int i = 0; i < n_points; i++) {
        double freq_hz = freqs[i];
        double af = emi_hprobe_antenna_factor(freq_hz, &hprobe);
        const char *note = "Valid";
        if (freq_hz > hprobe.self_resonance_hz * 0.5) note = "Near SRF";
        printf("%-14.3f %-16.1f %s\n", freq_hz / 1e6, af, note);
    }

    printf("\nCalibration Curve Interpolation:\n");
    double test_freqs[] = {15e6, 55e6, 150e6, 350e6};
    for (int i = 0; i < 4; i++) {
        double cal_interp = emi_interpolate_cal_factor(
            test_freqs[i], freqs, cal_factors, n_points);
        printf("  At %.1f MHz: K_cal = %.2f dB\n",
               test_freqs[i] / 1e6, cal_interp);
    }

    double cal_mean = 0.0;
    double cal_var = 0.0;
    for (int i = 0; i < n_points; i++) {
        double theoretical_cal = emi_hprobe_antenna_factor(freqs[i], &hprobe);
        double diff = cal_factors[i] - theoretical_cal;
        cal_mean += diff;
        cal_var += diff * diff;
    }
    cal_mean /= n_points;
    cal_var = cal_var / n_points - cal_mean * cal_mean;
    if (cal_var < 0.0) cal_var = 0.0;
    double cal_std = sqrt(cal_var);

    printf("\nCalibration Quality:\n");
    printf("  Mean calibration offset: %.2f dB\n", cal_mean);
    printf("  Standard deviation: %.2f dB\n", cal_std);
    printf("  Expanded uncertainty (k=2): %.2f dB\n", 2.0 * cal_std);
    printf("  Probe usable bandwidth: %.0f - %.0f MHz\n",
           freqs[0] / 1e6, freqs[n_points - 1] / 1e6);

    printf("\n--- Probe calibration complete ---\n");
    return 0;
}
