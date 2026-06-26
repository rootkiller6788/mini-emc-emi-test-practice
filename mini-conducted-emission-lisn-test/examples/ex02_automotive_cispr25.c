/** @example ex02_automotive_cispr25.c
 * Automotive conducted emission test per CISPR 25 Class 5
 * Demonstrates: CISPR 25 LISN, extended frequency range, CM/DM separation
 */
#include "lisn_core.h"
#include "lisn_measurement.h"
#include "lisn_standard.h"
#include "lisn_noise_separator.h"
#include <stdio.h>
#include <math.h>

int main(void) {
    printf("=== CISPR 25 Class 5 Automotive CE Test ===\n\n");
    
    lisn_config_t cfg;
    lisn_init_cispr25_standard(&cfg);
    printf("LISN: %s\n", cfg.model);
    printf("Frequency: %.0f kHz - %.0f MHz\n", cfg.freq_start_hz/1000, cfg.freq_stop_hz/1e6);
    
    lisn_limit_line_t limit;
    lisn_cispr25_class5_limit(&limit);
    printf("Limit: %s (%d breakpoints)\n\n", limit.description, limit.num_points);
    
    double f_test = 1000000.0;  /* 1 MHz */
    double qp_lim, avg_lim;
    lisn_interpolate_limit(&limit, f_test, &qp_lim, &avg_lim);
    printf("At %.1f MHz: QP limit = %.1f dBuV\n", f_test/1e6, qp_lim);
    
    lisn_cm_dm_result_t cmdm;
    lisn_separate_math_decomp(40.0, 35.0, f_test, &cmdm);
    printf("\nCM/DM separation at %.1f MHz:\n", f_test/1e6);
    printf("  CM: %.1f dBuV (%.1f%%)\n", cmdm.cm_voltage_dbuV, cmdm.cm_ratio_pct);
    printf("  DM: %.1f dBuV (%.1f%%)\n", cmdm.dm_voltage_dbuV, cmdm.dm_ratio_pct);
    
    double atten_cm, atten_dm;
    lisn_filter_attenuation_required(cmdm.cm_voltage_dbuV, cmdm.dm_voltage_dbuV, qp_lim, 6.0, &atten_cm, &atten_dm);
    printf("\nRequired filter attenuation (6 dB margin):\n");
    printf("  CM: %.1f dB\n", atten_cm);
    printf("  DM: %.1f dB\n", atten_dm);
    
    lisn_limit_line_free(&limit);
    return 0;
}
