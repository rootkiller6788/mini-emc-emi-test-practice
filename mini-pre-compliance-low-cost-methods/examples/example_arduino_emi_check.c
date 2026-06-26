/* example_arduino_emi_check.c - Arduino Board Radiated Emissions Pre-Check */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "emi_precompliance_core.h"
#include "emi_probe_theory.h"
#include "emi_measurement_setup.h"
#include "emi_spectrum_analysis.h"
#include "emi_lowcost_methods.h"
#include "emi_correlation_uncertainty.h"

int main(void) {
    printf("=== Arduino Board Radiated Emissions Pre-Check ===\n");
    printf("Low-cost near-field probe + budget SDR method\n\n");

    emi_limit_line_t limit_fcc;
    emi_limit_init_fcc15_radiated_classb(&limit_fcc);

    emi_sa_spec_t sa;
    emi_sa_init_rtlsdr(&sa);
    printf("Budget SA Configuration (RTL-SDR based):\n");
    printf("  Frequency range: %.0f MHz - %.0f MHz\n",
           sa.freq_min_hz / 1e6, sa.freq_max_hz / 1e6);
    printf("  Noise figure: %.0f dB\n", sa.noise_figure_db);
    printf("  Preamplifier gain: %.0f dB\n\n", sa.preamp_gain_db);

    emi_hprobe_spec_t hprobe;
    memset(&hprobe, 0, sizeof(hprobe));
    hprobe.loop_diameter_m = 0.02;
    hprobe.num_turns = 3;
    hprobe.wire_diameter_m = 0.0005;
    hprobe.self_resonance_hz = 800e6;
    hprobe.cal_factor_db = 0.0;

    emi_scan_config_t config;
    emi_scan_config_init(&config, EMI_EMISSION_RADIATED, EMI_STD_FCC_PART15);
    config.detector = EMI_DETECTOR_PEAK;

    printf("Scan Configuration:\n");
    printf("  Frequency: %.0f - %.0f MHz\n",
           config.start_freq_hz / 1e6, config.stop_freq_hz / 1e6);
    printf("  RBW: %.0f kHz, Points: %d\n\n",
           config.rbw_hz / 1e3, config.num_points);

    emi_scan_result_t scan;
    if (emi_peak_prescan(&config, &sa, &scan) != 0) {
        printf("ERROR: Scan failed!\n");
        return 1;
    }

    for (int i = 0; i < scan.num_points; i++) {
        double freq = scan.points[i].frequency_hz;
        double amp_dbuv = 15.0;

        double f_clock = 16e6;
        int clock_harm = (int)(freq / f_clock + 0.5);
        if (clock_harm > 0 && clock_harm <= 30) {
            double detune_clk = (freq - clock_harm * f_clock) / (config.rbw_hz);
            double clk_amp = 55.0 - 20.0 * log10((double)clock_harm);
            if (clk_amp > 0.0) {
                amp_dbuv += clk_amp * exp(-0.5 * detune_clk * detune_clk);
            }
        }

        double spi_clock = 4e6;
        int spi_harm = (int)(freq / spi_clock + 0.5);
        if (spi_harm > 0 && spi_harm <= 50) {
            double detune_spi = (freq - spi_harm * spi_clock) / (config.rbw_hz);
            double spi_amp = 40.0 - 20.0 * log10((double)spi_harm);
            if (spi_amp > 0.0) {
                amp_dbuv += spi_amp * exp(-0.5 * detune_spi * detune_spi);
            }
        }

        if (freq > 40e6) {
            double usb_clock = 48e6;
            int usb_harm = (int)(freq / usb_clock + 0.5);
            if (usb_harm > 0 && usb_harm <= 10) {
                double detune_usb = (freq - usb_harm * usb_clock) / (config.rbw_hz);
                double usb_amp = 45.0 - 25.0 * log10((double)usb_harm);
                if (usb_amp > 0.0) {
                    amp_dbuv += usb_amp * exp(-0.5 * detune_usb * detune_usb);
                }
            }
        }

        double bb_noise = 30.0 * exp(-freq / 100e6);
        amp_dbuv += bb_noise;

        scan.points[i].amplitude_dbuv = amp_dbuv;
        scan.points[i].amplitude_dbm = emi_dbuv_to_dbm(amp_dbuv, 50.0);
        scan.points[i].detector = EMI_DETECTOR_PEAK;
    }

    emi_antenna_spec_t ant;
    emi_antenna_init_biconical(&ant);
    printf("Using biconical antenna model (AF ~10-20 dB)\n");

    for (int i = 0; i < scan.num_points; i++) {
        double af = emi_antenna_factor_interpolate(&ant, scan.points[i].frequency_hz);
        double e_dbuvm = scan.points[i].amplitude_dbuv + af + 2.0;
        scan.points[i].field_strength_dbuvm = e_dbuvm;
    }

    emi_evaluate_all_margins(&scan, &limit_fcc);

    printf("Pre-Check Results (Arduino Board):\n");
    printf("  Max field strength: %.1f dBuV/m at %.2f MHz\n",
           scan.max_emission_dbuv, scan.max_emission_freq_hz / 1e6);
    printf("  Worst margin: %.1f dB\n", scan.worst_margin_db);
    printf("  Failing: %d, Marginal: %d\n\n",
           scan.num_failing_points, scan.num_marginal_points);

    int peaks[30];
    int n_peaks = emi_find_peaks(&scan, 6.0, peaks, 30);
    printf("Emission Source Classification:\n");
    printf("%-6s %-12s %-12s %-12s %s\n",
           "Peak", "Freq(MHz)", "Amp(dBuV/m)", "Margin(dB)", "Type");
    for (int i = 0; i < n_peaks && i < 15; i++) {
        int idx = peaks[i];
        double f = scan.points[idx].frequency_hz;
        double a = scan.points[idx].amplitude_dbuv;
        double field = scan.points[idx].field_strength_dbuvm;
        double lim = emi_limit_line_interpolate(&limit_fcc, f);
        emi_signal_class_t cls = emi_classify_nb_bb(a, a + 3.0, 9e3, 120e3);
        const char *type_str = (cls == EMI_SIGNAL_NARROWBAND) ? "NB-Clock"
                              : (cls == EMI_SIGNAL_BROADBAND) ? "BB-Noise"
                              : "Unknown";
        printf("%-6d %-12.3f %-12.1f %-12.1f %s\n",
               i + 1, f / 1e6, field, lim - field, type_str);
    }

    emi_correlation_result_t corr;
    memset(&corr, 0, sizeof(corr));
    corr.mean_offset_db = 3.0;
    corr.std_deviation_db = 4.0;
    corr.num_comparable_points = 30;
    double confidence = emi_precompliance_confidence(&corr, 46.0, 8.0);
    printf("\nPre-Compliance Confidence: %.1f%%\n", confidence * 100.0);

    printf("\nRecommendations:\n");
    if (scan.num_failing_points > 0) {
        printf("  - Add ferrite beads on I/O lines to suppress common-mode\n");
        printf("  - Improve PCB ground plane integrity near clock oscillator\n");
        printf("  - Consider series resistors on fast-switching GPIO pins\n");
        printf("  - Verify decoupling capacitor placement at MCU power pins\n");
    }
    if (scan.worst_margin_db < 10.0) {
        printf("  - Margin < 10 dB: full compliance test recommended\n");
    }
    printf("  - For improved accuracy, repeat with calibrated antenna\n");

    free(scan.points);
    emi_limit_line_free(&limit_fcc);
    free(ant.af_freq_hz);
    free(ant.af_db);
    printf("\n--- Arduino EMC pre-check complete ---\n");
    return 0;
}
