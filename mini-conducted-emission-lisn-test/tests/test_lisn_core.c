/** @file test_lisn_core.c - Tests for core LISN impedance functions */
#include "lisn_core.h"
#include <stdio.h>
#include <math.h>
#include <assert.h>

static int test_impedance_magnitude(void) {
    double z = lisn_impedance_magnitude(50.0, 50.0, 150000.0);
    assert(z > 30.0 && z < 45.0);  /* At 150 kHz: |Z| ~ 34.3 Ohm (near fc ~ 159 kHz) */
    z = lisn_impedance_magnitude(50.0, 50.0, 30000000.0);
    assert(z > 45.0 && z < 50.0);
    assert(lisn_impedance_magnitude(0, 50, 1000) == 0.0);
    assert(lisn_impedance_magnitude(50, 0, 1000) == 0.0);
    return 0;
}

static int test_corner_frequency(void) {
    double fc = lisn_corner_frequency_hz(50.0, 50.0);
    assert(fc > 150000.0 && fc < 170000.0);
    fc = lisn_corner_frequency_hz(5.0, 50.0);
    assert(fc > 1500000.0 && fc < 1700000.0);
    return 0;
}

static int test_phase(void) {
    double phase = lisn_impedance_phase_rad(50.0, 50.0, 150000.0);
    assert(phase > 0.7 && phase < 1.0);
    phase = lisn_impedance_phase_rad(50.0, 50.0, 30000000.0);
    assert(phase > 0.0 && phase < 0.1);
    return 0;
}

static int test_standard_init(void) {
    lisn_config_t cfg;
    lisn_init_cispr22_standard(&cfg);
    assert(cfg.type == LISN_V_50UH);
    assert(cfg.inductance_uh == 50.0);
    assert(cfg.resistance_ohm == 50.0);
    assert(cfg.freq_start_hz == 150000.0);
    assert(cfg.freq_stop_hz == 30000000.0);
    lisn_init_cispr25_standard(&cfg);
    assert(cfg.inductance_uh == 5.0);
    lisn_init_mil461_standard(&cfg);
    assert(cfg.coupling_cap_uf == 10.0);
    return 0;
}

static int test_insertion_loss(void) {
    lisn_config_t cfg;
    lisn_init_cispr22_standard(&cfg);
    double il = lisn_insertion_loss_db(&cfg, 1000000.0);
    assert(il >= 0.0 && il < 2.0);
    return 0;
}

static int test_vdf(void) {
    lisn_config_t cfg;
    lisn_init_cispr22_standard(&cfg);
    double vdf = lisn_voltage_division_factor(&cfg, 150000.0, 50.0);
    assert(vdf > 0.9 && vdf <= 1.0);
    return 0;
}

static int test_compliance_validation(void) {
    lisn_config_t cfg;
    lisn_init_cispr22_standard(&cfg);
    int ok = lisn_validate_impedance_compliance(&cfg, 500000.0, 20.0);
    assert(ok == 1);  /* At 500 kHz: |Z| ~ 48.5 Ohm, within +/-20% of 50 */
    ok = lisn_validate_impedance_compliance(&cfg, 50000.0, 20.0);
    assert(ok == 0);
    return 0;
}

static int test_transfer_function(void) {
    lisn_config_t cfg;
    lisn_init_cispr22_standard(&cfg);
    double h = lisn_transfer_function_magnitude(&cfg, 1000000.0, 50.0, 0.1e-6);
    assert(h > 0.0 && h <= 1.0);  /* Transfer function positive and bounded */
    return 0;
}

int main(void) {
    printf("Running lisn_core tests...\n");
    test_impedance_magnitude();
    test_corner_frequency();
    test_phase();
    test_standard_init();
    test_insertion_loss();
    test_vdf();
    test_compliance_validation();
    test_transfer_function();
    printf("All lisn_core tests passed!\n");
    return 0;
}
