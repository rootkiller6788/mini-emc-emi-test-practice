/** @example ex03_precompliance_lisn.c
 * Pre-compliance low-cost conducted emission estimation
 * Demonstrates: pre-compliance correction, calibration, GTEM correlation
 */
#include "lisn_core.h"
#include "lisn_impedance.h"
#include "lisn_calibration.h"
#include <stdio.h>
#include <math.h>

int main(void) {
    printf("=== Pre-Compliance Conducted Emission Estimation ===\n\n");
    
    lisn_config_t cfg;
    lisn_init_cispr22_standard(&cfg);
    
    lisn_network_model_t model;
    lisn_network_model_init_cispr22(&model);
    
    printf("LISN Model: V-network 50 uH || 50 Ohm\n");
    printf("Frequency    |Z|        Phase     IL       VDF\n");
    printf("-------------------------------------------------\n");
    
    double freqs[] = {150000.0, 500000.0, 1e6, 5e6, 10e6, 30e6};
    for (int i = 0; i < 6; i++) {
        double f = freqs[i];
        double z_mag = lisn_impedance_magnitude(50.0, 50.0, f);
        double phase = lisn_impedance_phase_rad(50.0, 50.0, f) * 180.0 / M_PI;
        double il = lisn_cal_measure_insertion_loss(&model, f);
        double vdf = lisn_cal_voltage_division_factor(&model, f);
        printf("%9.0f Hz  %6.1f Ohm  %6.1f deg  %5.2f dB  %5.2f dB\n", f, z_mag, phase, il, vdf);
    }
    
    printf("\nPre-compliance correction factors:\n");
    for (int i = 0; i < 6; i++) {
        double cf = lisn_precompliance_correction_factor(freqs[i]);
        printf("  %.0f kHz: %.1f dB\n", freqs[i]/1000, cf);
    }
    
    printf("\nPre-compliance method analyzed.\n");
    return 0;
}
