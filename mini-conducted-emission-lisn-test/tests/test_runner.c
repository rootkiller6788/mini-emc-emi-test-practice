#include "lisn_core.h"
#include "lisn_impedance.h"
#include "lisn_measurement.h"
#include "lisn_standard.h"
#include "lisn_calibration.h"
#include "lisn_noise_separator.h"
#include <stdio.h>
#include <math.h>
#include <assert.h>

int main(void) {
    printf("=== LISN Conducted Emission Test Suite ===\n");
    
    /* L1: Core definitions */
    lisn_config_t cfg;
    lisn_init_cispr22_standard(&cfg);
    assert(cfg.type == LISN_V_50UH);
    printf("  L1 Core definitions: PASS\n");
    
    /* L2: Impedance state */
    lisn_impedance_state_t state;
    lisn_compute_impedance_state(&cfg, 1000000.0, 50.0, &state);
    assert(state.z_magnitude_ohm > 40.0);
    printf("  L2 Impedance state: PASS\n");
    
    /* L3: Network theory */
    lisn_network_model_t model;
    lisn_network_model_init_cispr22(&model);
    complex_z_t z = lisn_v_network_impedance(&model, 1000000.0);
    assert(sqrt(z.real_ohm*z.real_ohm + z.imag_ohm*z.imag_ohm) > 0.0);
    printf("  L3 Network theory: PASS\n");
    
    /* L4: Fundamental laws */
    double fc = lisn_corner_frequency_hz(50.0, 50.0);
    assert(fc > 150000.0 && fc < 170000.0);
    printf("  L4 Corner frequency: PASS\n");
    
    /* L5: Algorithms */
    double samples[] = {0.001, 0.002, 0.0015, 0.003, 0.002};
    double pk = lisn_peak_detect(samples, 5, 50.0);
    assert(pk > 0.0);
    printf("  L5 Peak detector: PASS\n");
    
    /* L6: Standard limits */
    lisn_limit_line_t line;
    lisn_cispr22_class_b_limit(&line);
    assert(line.num_points == 4);
    double qp, avg;
    int rc = lisn_interpolate_limit(&line, 1000000.0, &qp, &avg);
    assert(rc == 0 && qp > 50.0);
    printf("  L6 CISPR 22 Class B limit: PASS\n");
    lisn_limit_line_free(&line);
    
    /* L7: Application */
    lisn_cm_dm_result_t cmdm;
    lisn_separate_math_decomp(60.0, 50.0, 1000000.0, &cmdm);
    assert(cmdm.cm_voltage_dbuV > 0.0);
    printf("  L7 CM/DM separation: PASS\n");
    
    printf("\n=== ALL TESTS PASSED ===\n");
    return 0;
}
