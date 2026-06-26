/**
 * @file test_cm_dm_decomp.c
 * @brief Tests for CM/DM decomposition, CMRR, insertion loss, and filter elements
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include "cm_dm_filter.h"
#include "cm_choke_model.h"
#include "dm_filter_model.h"

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) printf("  %s:\n", name)
#define CHECK(cond) do { tests_run++; if (cond) { printf("    PASS"); tests_passed++; } else { printf("    FAIL (%s:%d)", __FILE__, __LINE__); } printf("\n"); } while(0)
#define CHECK_FEQ(a, b, tol) CHECK(fabs((a)-(b)) < (tol))

int main(void) {
    printf("=== Test: CM/DM Decomposition ===\n");

    /* Test 1: Balanced single-phase decomposition */
    TEST("Balanced 230V single-phase CM decomposition");
    {
        cm_dm_decomposition_t result;
        int rc = cm_dm_decompose(230.0, 0.0, 1.0, -1.0, 50.0, &result);
        CHECK(rc == 0);
        CHECK_FEQ(result.cm.voltage_cm, 115.0, 1e-6);  /* (230+0)/2 */
        CHECK_FEQ(result.dm.voltage_dm, 230.0, 1e-6);  /* 230-0 */
        CHECK_FEQ(result.cm.current_cm, 0.0, 1e-6);    /* 1+(-1)=0 */
        CHECK_FEQ(result.dm.current_dm, 1.0, 1e-6);    /* (1-(-1))/2 */
    }

    /* Test 2: CM/DM decomposition with both conductors offset */
    TEST("CM/DM decomposition with common offset");
    {
        cm_dm_decomposition_t result;
        int rc = cm_dm_decompose(240.0, 10.0, 0.5, 0.5, 60.0, &result);
        CHECK(rc == 0);
        CHECK_FEQ(result.cm.voltage_cm, 125.0, 1e-6);  /* (240+10)/2 */
        CHECK_FEQ(result.dm.voltage_dm, 230.0, 1e-6);  /* 240-10 */
        CHECK_FEQ(result.cm.current_cm, 1.0, 1e-6);    /* 0.5+0.5 */
        CHECK_FEQ(result.dm.current_dm, 0.0, 1e-6);    /* (0.5-0.5)/2 */
    }

    /* Test 3: NULL pointer handling */
    TEST("CM/DM decompose NULL result returns error");
    {
        int rc = cm_dm_decompose(10.0, 5.0, 1.0, 0.5, 50.0, NULL);
        CHECK(rc == -1);
    }

    /* Test 4: Invalid frequency */
    TEST("CM/DM decompose invalid frequency returns error");
    {
        cm_dm_decomposition_t result;
        int rc = cm_dm_decompose(10.0, 5.0, 1.0, 0.5, -1.0, &result);
        CHECK(rc == -1);
    }

    printf("\n=== Test: Three-Phase CM/DM Decomposition ===\n");

    /* Test 5: Balanced three-phase */
    TEST("Balanced 400V three-phase CM/DM decomposition");
    {
        cm_quantities_t cm;
        dm_quantities_t dm_a, dm_b;
        int rc = cm_dm_decompose_3phase(230.0, 230.0, 230.0,
                                         1.0, 1.0, 1.0, 50.0,
                                         &cm, &dm_a, &dm_b);
        CHECK(rc == 0);
        CHECK_FEQ(cm.voltage_cm, 230.0, 1e-6);        /* All three equal → CM = 230 */
        CHECK_FEQ(cm.current_cm, 3.0, 1e-6);          /* Sum of currents */
    }

    printf("\n=== Test: CMRR Calculation ===\n");

    /* Test 6: CMRR for a good CM choke */
    TEST("CMRR calculation for good CM choke (1000Ω/1Ω)");
    {
        cmrr_result_t *r = cmrr_calculate(1000.0, 1.0, 100e3);
        CHECK(r != NULL);
        CHECK_FEQ(r->cmrr_db, 60.0, 0.1);  /* 20*log10(1000/1) = 60 dB */
        CHECK_FEQ(r->cm_impedance, 1000.0, 1e-6);
        CHECK_FEQ(r->dm_impedance, 1.0, 1e-6);
        cmrr_free(r);
    }

    /* Test 7: CMRR for a poor CM choke */
    TEST("CMRR calculation for poor CM choke (100Ω/50Ω)");
    {
        cmrr_result_t *r = cmrr_calculate(100.0, 50.0, 100e3);
        CHECK(r != NULL);
        CHECK(r->cmrr_db < 10.0);  /* 20*log10(100/50) ≈ 6 dB */
        cmrr_free(r);
    }

    printf("\n=== Test: Insertion Loss ===\n");

    /* Test 8: Basic insertion loss */
    TEST("Insertion loss with series impedance in 50Ω system");
    {
        insertion_loss_result_t *r = insertion_loss_calc(50.0, 50.0,
            100.0, 0.1, 100e3, 1);  /* CM mode */
        CHECK(r != NULL);
        CHECK(r->il_db > 0.0);  /* Should have positive IL */
        CHECK(r->is_cm == 1);
        insertion_loss_free(r);
    }

    /* Test 9: Zero insertion loss for zero filter impedance */
    TEST("Insertion loss zero when filter impedance is zero");
    {
        insertion_loss_result_t *r = insertion_loss_calc(50.0, 50.0,
            0.0, 0.0, 100e3, 0);  /* DM mode */
        CHECK(r != NULL);
        CHECK_FEQ(r->il_db, 0.0, 0.01);
        insertion_loss_free(r);
    }

    printf("\n=== Test: Filter Elements ===\n");

    /* Test 10: Create and configure filter element */
    TEST("Create capacitor element and set parasitics");
    {
        filter_element_t *elem = filter_element_create(
            FILTER_ELEM_X_CAPACITOR, 1e-6, 10.0);
        CHECK(elem != NULL);
        CHECK_FEQ(elem->nominal_value, 1e-6, 1e-12);
        CHECK_FEQ(elem->tolerance_pct, 10.0, 0.01);

        filter_element_set_parasitics(elem, 0.1, 5e-9, 0.0, 1e8);
        CHECK_FEQ(elem->esr, 0.1, 1e-9);
        CHECK_FEQ(elem->esl, 5e-9, 1e-12);
        filter_element_free(elem);
    }

    /* Test 11: Filter element impedance at DC */
    TEST("Capacitor impedance at DC is very high");
    {
        filter_element_t *elem = filter_element_create(
            FILTER_ELEM_X_CAPACITOR, 1e-6, 5.0);
        complex_t z = filter_element_impedance(elem, 0.1);
        double mag = complex_mag(z);
        CHECK(mag > 1e5);  /* High impedance at low frequency */
        filter_element_free(elem);
    }

    /* Test 12: Inductor impedance increases with frequency */
    TEST("Inductor impedance increases with frequency");
    {
        filter_element_t *elem = filter_element_create(
            FILTER_ELEM_DM_INDUCTOR, 1e-3, 10.0);
        complex_t z_low = filter_element_impedance(elem, 1e3);
        complex_t z_high = filter_element_impedance(elem, 10e3);
        double mag_low = complex_mag(z_low);
        double mag_high = complex_mag(z_high);
        CHECK(mag_high > mag_low);
        filter_element_free(elem);
    }

    printf("\n=== Test: Complex Arithmetic ===\n");

    /* Test 13: Complex addition */
    TEST("Complex arithmetic: basic operations");
    {
        complex_t a = {3.0, 4.0};
        complex_t b = {1.0, 2.0};
        complex_t c = complex_add(a, b);
        CHECK_FEQ(c.real, 4.0, 1e-12);
        CHECK_FEQ(c.imag, 6.0, 1e-12);

        complex_t d = complex_mul(a, b);
        /* (3+j4)(1+j2) = 3+j6+j4-8 = -5+j10 */
        CHECK_FEQ(d.real, -5.0, 1e-12);
        CHECK_FEQ(d.imag, 10.0, 1e-12);

        double mag = complex_mag(a);
        CHECK_FEQ(mag, 5.0, 1e-6);  /* sqrt(9+16)=5 */
    }

    printf("\n=== Test: CM Choke Model ===\n");

    /* Test 14: Create CM choke */
    TEST("Create CM choke model from core parameters");
    {
        core_material_t *mat = core_material_lookup(CORE_MAT_N87);
        CHECK(mat != NULL);
        core_geometry_t geom = core_geom_calculate(
            CORE_SHAPE_TOROID, 0.020, 0.012, 0.008, mat->mu_i);

        cm_choke_model_t *choke = cm_choke_create(mat, &geom, 20.0, 0.99);
        CHECK(choke != NULL);
        CHECK(choke->l_cm > choke->l_dm_leakage);  /* CM L >> DM L */
        CHECK(choke->num_turns == 20.0);
        CHECK_FEQ(choke->coupling_coef, 0.99, 1e-9);

        /* Check CM impedance at 100 kHz */
        complex_t z_cm = cm_choke_cm_impedance(choke, 100e3);
        CHECK(complex_mag(z_cm) > 0.0);  /* Should have non-zero impedance */

        /* Check DM impedance is much lower */
        complex_t z_dm = cm_choke_dm_impedance(choke, 100e3);
        CHECK(complex_mag(z_cm) > complex_mag(z_dm));

        /* Check saturation at safe voltage */
        int sat = cm_choke_check_saturation(choke, 1.0, 100e3);
        CHECK(sat == 0);  /* Should not saturate at 1V */

        cm_choke_free(choke);
        core_material_free(mat);
    }

    printf("\n=== Test: EMC Limits ===\n");

    /* Test 15: EMC limit retrieval */
    TEST("Get CISPR 32 Class B limit");
    {
        emc_limit_t *limit = emc_limit_get(EMC_STD_CISPR32, 0);
        CHECK(limit != NULL);
        CHECK(limit->num_segments == 3);
        emc_limit_free(limit);
    }

    printf("\n=== Test: Filter Design ===\n");

    /* Test 16: Filter design for AC-DC SMPS */
    TEST("Design filter for AC-DC SMPS application");
    {
        filter_design_spec_t *spec = filter_preset_get(APP_AC_DC_SMPS);
        CHECK(spec != NULL);
        CHECK_FEQ(spec->v_nominal, 230.0, 0.1);
        CHECK(spec->standard == EMC_STD_CISPR32);

        filter_design_result_t *result = filter_design_emi(spec, NULL);
        CHECK(result != NULL);
        CHECK(result->filter.num_stages >= 1);
        CHECK(result->filter.num_stages <= 4);
        CHECK(result->filter.dm_cutoff_freq > 0.0);

        filter_design_result_free(result);
        free(spec);
    }

    printf("\n=== Test: Required Attenuation ===\n");

    /* Test 17: Required attenuation */
    TEST("Required attenuation given noise and limit");
    {
        emc_limit_t *limit = emc_limit_get(EMC_STD_CISPR32, 0);
        double f[3] = {150e3, 500e3, 5e6};
        double n[3] = {80.0, 70.0, 70.0};  /* Noise levels */
        noise_spectrum_t noise = {f, n, 3, 9e3, 0, 0.0};

        size_t n_out;
        double *il_req = required_attenuation(&noise, limit, 6.0, &n_out);
        CHECK(il_req != NULL);
        CHECK(n_out == 3);
        CHECK(il_req[0] > 0.0);  /* Should need attenuation at 150k */

        free(il_req);
        emc_limit_free(limit);
    }

    printf("\n=== Test: Resonance and Damping ===\n");

    /* Test 18: LC resonance and damping */
    TEST("LC resonance frequency calculation");
    {
        double L = 1e-3;  /* 1 mH */
        double C = 1e-6;  /* 1 µF */
        double f_res = filter_resonance_frequency(L, C);
        CHECK(f_res > 1000.0);
        CHECK(f_res < 10000.0);  /* f_res = 1/(2π√(1e-9)) ≈ 5033 Hz */

        double r_damp = filter_optimal_damping_resistance(L, C);
        CHECK(r_damp > 10.0 && r_damp < 100.0);  /* √(1e-3/1e-6)=31.6Ω */

        double Q = filter_quality_factor(L, C, r_damp, 0);
        CHECK(Q > 0.8 && Q < 1.2);  /* Q ≈ 1 for optimal damping */
    }

    printf("\n=== Test: Safety Calculations ===\n");

    /* Test 19: X-cap bleed resistor */
    TEST("X-cap bleed resistor calculation");
    {
        double v_peak = 230.0 * sqrt(2.0);  /* 325V */
        double c = 1e-6;  /* 1 µF */
        double r = x_cap_bleed_resistor(c, v_peak, 1.0);
        CHECK(r > 100e3);   /* Should be reasonable resistance */
        CHECK(r < 2e6);     /* Not crazy large */
    }

    /* Test 20: Y-cap leakage current */
    TEST("Y-cap leakage current");
    {
        double i_leak = y_cap_leakage_current(4.7e-9, 50.0, 230.0);
        CHECK(i_leak < 0.001);  /* <1 mA for typical Y-cap */
        CHECK(i_leak > 0.0);

        double c_max = y_cap_max_for_leakage(0.5, 50.0, 230.0);
        CHECK(c_max > 1e-9 && c_max < 1e-6);
    }

    printf("\n=== Results: %d/%d tests passed ===\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}