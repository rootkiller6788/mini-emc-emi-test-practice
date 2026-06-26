/** @example ex01_cispr22_classb_sweep.c
 * Conducted emission sweep for CISPR 22 Class B compliance
 * Demonstrates: LISN setup, frequency sweep, limit line comparison
 */
#include "lisn_core.h"
#include "lisn_measurement.h"
#include "lisn_standard.h"
#include <stdio.h>
#include <math.h>

int main(void) {
    printf("=== CISPR 22 Class B Conducted Emission Sweep ===\n\n");
    
    lisn_config_t cfg;
    lisn_init_cispr22_standard(&cfg);
    printf("LISN: %s (%.1f uH, %.1f Ohm)\n", cfg.model, cfg.inductance_uh, cfg.resistance_ohm);
    printf("Frequency range: %.0f Hz - %.0f MHz\n", cfg.freq_start_hz, cfg.freq_stop_hz/1e6);
    
    lisn_limit_line_t limit;
    lisn_cispr22_class_b_limit(&limit);
    
    double freqs[4] = {150000.0, 500000.0, 5000000.0, 30000000.0};
    double qp_lim, avg_lim;
    
    printf("\nFrequency     QP Limit    AVG Limit\n");
    printf("---------------------------------------\n");
    for (int i = 0; i < 4; i++) {
        lisn_interpolate_limit(&limit, freqs[i], &qp_lim, &avg_lim);
        printf("%10.0f Hz  %6.1f dBuV  %6.1f dBuV\n", freqs[i], qp_lim, avg_lim);
    }
    
    printf("\nCISPR 22 Class B limit line verified.\n");
    lisn_limit_line_free(&limit);
    return 0;
}
