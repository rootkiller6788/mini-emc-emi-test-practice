/* =========================================================================
 * test_radiated_emission.c — Comprehensive Test Suite
 *
 * Tests for:
 *   L1: Antenna factor computation, dB conversions, field strength
 *   L3: Near/far field classification, wave impedance
 *   L4: Friis equation, NSA theoretical, ground reflection
 *   L5: AF interpolation, height scan optimization, SVSWR
 *   L6: Limit line queries, emission scan
 *   L7: Standard-specific limits (CISPR 22/32, FCC, MIL-STD, CISPR 25)
 * ========================================================================= */

#include "radiated_emission.h"
#include "antenna_types.h"
#include "chamber_theory.h"
#include "field_propagation.h"
#include "emission_limits.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <assert.h>

#define PI 3.14159265358979323846
#define EPS 0.01
#define EPS_DB 0.5

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { tests_run++; printf("  TEST: %s ... ", name); } while(0)
#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)
#define CHECK_CLOSE(a, b, eps, msg) \
    do { if (fabs((a)-(b)) > (eps)) { \
        printf("FAIL: %s (got %.4f, expected %.4f)\n", msg, a, b); return; } \
    } while(0)

/* ---- L1: dB Conversions ---- */
static void test_dbm_to_dbuv(void)
{
    TEST("dBm to dBuV conversion (50 ohm)");
    double result = re_dbm_to_dbuv(-73.0, 50.0);
    CHECK_CLOSE(result, 34.0, 0.5, "Expected -73 dBm = ~34 dBuV at 50 ohm");
    PASS();
}

static void test_dbm_to_dbuv_75ohm(void)
{
    TEST("dBm to dBuV conversion (75 ohm)");
    double result = re_dbm_to_dbuv(-73.0, 75.0);
    CHECK_CLOSE(result, 35.75, 0.1, "Expected -73 dBm approx 35.75 dBuV at 75 ohm");
    PASS();
}

/* ---- L1: Antenna Factor from Gain ---- */
static void test_antenna_factor_halfwave_dipole(void)
{
    TEST("Antenna factor of half-wave dipole at 300 MHz");
    /* Half-wave dipole: G = 1.64 (2.15 dBi) */
    double af = re_antenna_factor_from_gain(300e6, 1.64);
    /* Expected: 20*log10(300) - 10*log10(1.64) - 29.78
     *         = 49.54 - 2.14 - 29.78 = 17.62 dB/m */
    CHECK_CLOSE(af, 17.62, 0.5, "AF at 300 MHz for half-wave dipole");
    PASS();
}

static void test_antenna_factor_high_gain(void)
{
    TEST("Antenna factor of high-gain horn at 10 GHz");
    /* Horn at 10 GHz: G = 100 (20 dBi) */
    double af = re_antenna_factor_from_gain(10e9, 100.0);
    /* 20*log10(10000) - 10*log10(100) - 29.78 = 80 - 20 - 29.78 = 30.22 */
    CHECK_CLOSE(af, 30.22, 0.5, "AF at 10 GHz for 20 dBi horn");
    PASS();
}

/* ---- L1: Field Strength Computation ---- */
static void test_field_strength_compute(void)
{
    TEST("Field strength from receiver reading");
    /* Example: -80 dBm at receiver, AF=10 dB/m, cable=3 dB, preamp=20 dB, 50 ohm
     * V_dBuV = -80 + 107 = 27 dBuV
     * E = 27 + 10 + 3 - 20 = 20 dBuV/m */
    double e = re_field_strength_compute(-80.0, 10.0, 3.0, 20.0, 50.0);
    CHECK_CLOSE(e, 20.0, 0.5, "Field strength computation");
    PASS();
}

/* ---- L3: Free-space Impedance ---- */
static void test_free_space_impedance(void)
{
    TEST("Free-space impedance");
    double eta = fp_free_space_impedance();
    CHECK_CLOSE(eta, 376.73, 1.0, "eta_0 approx 376.73 ohm");
    PASS();
}

/* ---- L3: Wavenumber ---- */
static void test_wavenumber(void)
{
    TEST("Wavenumber at 300 MHz");
    double k = fp_wavenumber(300e6);
    /* k = 2*pi*300e6/3e8 = 2*pi*1 = 6.283 */
    CHECK_CLOSE(k, 6.283, 0.01, "k at 300 MHz");
    PASS();
}

/* ---- L3: Wavelength ---- */
static void test_wavelength(void)
{
    TEST("Wavelength at 300 MHz");
    double lambda = at_wavelength(300e6);
    CHECK_CLOSE(lambda, 1.0, 0.01, "lambda = 1m at 300 MHz");
    PASS();
}

/* ---- L3: Free-space Path Loss ---- */
static void test_fspl(void)
{
    TEST("Free-space path loss at 3m, 300 MHz");
    double fspl = at_free_space_path_loss(3.0, 300e6);
    /* FSPL = 20*log10(4*pi*3/1) = 20*log10(37.7) = 31.53 dB */
    CHECK_CLOSE(fspl, 31.53, 0.5, "FSPL at 3m, 300 MHz");
    PASS();
}

/* ---- L3: Field Region Classification ---- */
static void test_field_region_far(void)
{
    TEST("Far-field classification at 10m, 1 GHz");
    fp_field_region_t region = fp_classify_region(10.0, 1.0, 1e9);
    CHECK(region == FP_FAR_FIELD, "10m at 1 GHz should be far-field");
    PASS();
}

static void test_field_region_near(void)
{
    TEST("Near-field classification at 0.1m, 100 MHz");
    fp_field_region_t region = fp_classify_region(0.1, 1.0, 100e6);
    CHECK(region == FP_REACTIVE_NEAR_FIELD, "0.1m at 100 MHz should be reactive near-field");
    PASS();
}

/* ---- L3: Wave Impedance vs Distance ---- */
static void test_wave_impedance_far_field(void)
{
    TEST("Wave impedance in far-field");
    double zw = fp_wave_impedance_magnitude(10.0, 1e9);
    CHECK_CLOSE(zw, 376.73, 5.0, "Zw should be 377 ohm in far-field");
    PASS();
}

static void test_wave_impedance_near_field(void)
{
    TEST("Wave impedance in near-field (high)");
    double zw = fp_wave_impedance_magnitude(0.01, 100e6);
    /* Near-field of E-field source: Zw > 377 ohm */
    CHECK(zw > 377.0, "Zw > 377 ohm in reactive near-field of E-field source");
    PASS();
}

/* ---- L3: Poynting Vector ---- */
static void test_poynting(void)
{
    TEST("Poynting vector from E-field");
    /* E = 1 V/m -> |S| = 1^2/377 = 0.00265 W/m2 */
    double s = fp_poynting_from_efield(1.0);
    CHECK_CLOSE(s, 0.00265, 0.0001, "Poynting vector for 1 V/m");
    PASS();
}

/* ---- L4: Friis Field from Power ---- */
static void test_field_from_power(void)
{
    TEST("E-field from transmitter power (far-field)");
    /* P_tx=1W, G_tx=1 (isotropic), d=3m
     * E = sqrt(30*1*1)/3 = sqrt(30)/3 = 5.477/3 = 1.826 V/m */
    double e = fp_field_from_power(1.0, 1.0, 3.0);
    CHECK_CLOSE(e, 1.826, 0.01, "E-field for 1W isotropic at 3m");
    PASS();
}

/* ---- L4: Ground Plane Reflection ---- */
static void test_ground_reflection_horizontal(void)
{
    TEST("Ground reflection horizontal polarization (perfect conductor)");
    /* At 45 deg grazing, perfect conductor: Gamma_h = -1 in magnitude */
    double gamma = ct_ground_reflection_coeff(1e9, 1e6, 1e9, PI/4, RE_POL_HORIZONTAL);
    CHECK(gamma >= 0.5 && gamma <= 1.0, "Reflection coefficient magnitude reasonable");
    PASS();
}

/* ---- L4: NSA Theoretical ---- */
static void test_nsa_theoretical(void)
{
    TEST("NSA theoretical at 3m, 300 MHz, horizontal");
    double nsa = ct_nsa_theoretical(3.0, 2.0, 1.0, 300e6, RE_POL_HORIZONTAL);
    /* NSA should be a reasonable positive value */
    CHECK(nsa > 0.0 && nsa < 80.0, "NSA within physically reasonable range");
    PASS();
}

/* ---- L4: SVSWR from Reflectivity ---- */
static void test_svswr(void)
{
    TEST("SVSWR from absorber reflectivity");
    /* -30 dB absorber -> Gamma = 0.0316
     * SVSWR = (1+0.0316)/(1-0.0316) = 1.065 */
    double svswr = ct_svswr_from_reflectivity(-30.0, 0.0);
    CHECK_CLOSE(svswr, 1.065, 0.01, "SVSWR for -30 dB absorber");
    PASS();
}

/* ---- L5: Antenna Factor Table Initialization ---- */
static void test_af_table_init(void)
{
    TEST("Antenna factor table initialization (BICONICAL)");
    re_antenna_factor_table_t table;
    re_af_table_init_standard(&table, "BICONICAL");
    CHECK(table.num_points > 0, "Table should have points");
    CHECK(table.num_points <= RE_MAX_AF_POINTS, "Table within max points");
    CHECK(table.points[0].frequency_hz >= 30e6, "First freq >= 30 MHz");
    PASS();
}

/* ---- L5: Antenna Factor Interpolation ---- */
static void test_af_interpolate(void)
{
    TEST("Antenna factor interpolation");
    re_antenna_factor_table_t table;
    re_af_table_init_standard(&table, "LPDA");

    double af_db;
    int ret = re_af_interpolate(&table, 500e6, &af_db);
    CHECK(ret == 0, "Interpolation should succeed");
    CHECK(af_db > 0.0 && af_db < 40.0, "AF within reasonable range");
    PASS();
}

/* ---- L5: Height Scan Optimization ---- */
static void test_height_scan_opt(void)
{
    TEST("Height scan optimization");
    double h_opt, max_ratio;
    ct_nsa_height_scan_opt(3.0, 2.0, 300e6, RE_POL_HORIZONTAL, &h_opt, &max_ratio);
    CHECK(h_opt >= 1.0 && h_opt <= 4.0, "Optimal height in 1-4m range");
    CHECK(max_ratio > 0.0, "Signal ratio is positive");
    PASS();
}

/* ---- L5: Far-field Boundary ---- */
static void test_far_field_boundary(void)
{
    TEST("Far-field boundary computation");
    double r_nf, r_ff;
    at_far_field_boundary(1.0, 1e9, &r_nf, &r_ff);
    CHECK(r_ff > r_nf, "Far-field > near-field boundary");
    CHECK(r_ff > 0.0, "Far-field distance positive");
    PASS();
}

/* ---- L6: Limit Line CISPR 32 Class B ---- */
static void test_cispr32_classb_limit(void)
{
    TEST("CISPR 32 Class B at 100 MHz, 3m");
    double limit = el_get_standard_limit(RE_STD_CISPR32, 100e6,
                                          RE_DETECTOR_QUASI_PEAK, 3.0, 1);
    CHECK_CLOSE(limit, 40.0, 1.0, "CISPR 32 Class B QP limit at 100 MHz, 3m");
    PASS();
}

/* ---- L6: Limit Line FCC Class B ---- */
static void test_fcc15_classb_limit(void)
{
    TEST("FCC Part 15 Class B at 100 MHz, 3m");
    double limit = el_get_standard_limit(RE_STD_FCC_PART15, 100e6,
                                          RE_DETECTOR_QUASI_PEAK, 3.0, 1);
    /* 88-216 MHz: 43.5 dBuV/m */
    CHECK_CLOSE(limit, 43.5, 1.0, "FCC Class B QP limit at 100 MHz, 3m");
    PASS();
}

/* ---- L6: Limit Line FCC Class B above 1 GHz ---- */
static void test_fcc15_classb_above_1ghz(void)
{
    TEST("FCC Part 15 Class B at 2 GHz average limit");
    double limit = el_get_standard_limit(RE_STD_FCC_PART15, 2e9,
                                          RE_DETECTOR_AVERAGE, 3.0, 1);
    CHECK_CLOSE(limit, 54.0, 1.0, "FCC Class B Avg limit at 2 GHz, 3m");
    PASS();
}

/* ---- L6: Limit Line CISPR 25 ---- */
static void test_cispr25_limit(void)
{
    TEST("CISPR 25 Class 5 at 100 MHz, 1m");
    double limit = el_get_standard_limit(RE_STD_CISPR25, 100e6,
                                          RE_DETECTOR_PEAK, 1.0, 5);
    CHECK(limit > 20.0 && limit < 40.0, "CISPR 25 Class 5 limit reasonable");
    PASS();
}

/* ---- L6: Emission Scan ---- */
static void test_emission_scan(void)
{
    TEST("Full emission scan");
    re_emission_scan_t scan;
    re_eut_descriptor_t eut;
    re_measurement_config_t config;
    re_antenna_factor_table_t af_tbl;

    memset(&eut, 0, sizeof(eut));
    strncpy(eut.name, "TEST_EUT", sizeof(eut.name) - 1);
    eut.config_type = RE_EUT_TABLE_TOP;
    eut.op_state = RE_EUT_STATE_ACTIVE_TX;
    eut.base_height_m = 0.8;

    re_config_init_standard(&config, RE_STD_CISPR32, RE_SITE_SAC_3M);
    re_af_table_init_standard(&af_tbl, "BICONICAL");

    int n = re_emission_scan_perform(&scan, &eut, &config, &af_tbl);
    CHECK(n >= 0, "Scan should return valid count");
    PASS();
}

/* ---- L6: Pass/Fail Evaluation ---- */
static void test_pass_fail(void)
{
    TEST("Pass/fail evaluation");
    re_emission_scan_t scan;
    memset(&scan, 0, sizeof(scan));
    scan.num_points = 2;
    scan.points[0].margin_db = -10.0; /* PASS */
    scan.points[1].margin_db = -5.0;  /* PASS */

    int verdict = re_emission_evaluate_pass_fail(&scan);
    CHECK(verdict == 1, "All below limit should PASS");
    PASS();
}

/* ---- L6: Distance Extrapolation ---- */
static void test_distance_extrapolation(void)
{
    TEST("Distance extrapolation 3m to 10m");
    /* E_10m = E_3m - 20*log10(10/3) = E_3m - 10.46 dB */
    double e_10m = fp_extrapolate_distance(1.0, 3.0, 10.0, 1e9, 0);
    CHECK_CLOSE(e_10m, 0.3, 0.05, "1/r extrapolation 3m to 10m");
    PASS();
}

/* ---- L7: CISPR 22 limits ---- */
static void test_cispr22_classb_10m_limit(void)
{
    TEST("CISPR 22 Class B QP at 100 MHz, 10m via distance conversion");
    /* The el_get_standard_limit internally uses 3m and converts */
    double limit = el_get_standard_limit(RE_STD_CISPR22, 100e6,
                                          RE_DETECTOR_QUASI_PEAK, 10.0, 1);
    CHECK(limit > 25.0 && limit < 40.0, "CISPR 22 Class B at 10m reasonable");
    PASS();
}

/* ---- L7: MIL-STD-461 limit ---- */
static void test_mil461_limit(void)
{
    TEST("MIL-STD-461G RE102 at 100 MHz, 1m");
    double limit = el_get_standard_limit(RE_STD_MIL_STD_461, 100e6,
                                          RE_DETECTOR_PEAK, 1.0, 0);
    CHECK(limit > 20.0 && limit < 30.0, "MIL-STD-461 RE102 limit reasonable");
    PASS();
}

/* ---- L5: Antenna Gain Computations ---- */
static void test_biconical_gain(void)
{
    TEST("Biconical antenna gain estimate");
    double g = at_biconical_gain(45.0, 200e6, 0.5);
    CHECK(g > 1.0 && g < 3.0, "Biconical gain 1-3 linear");
    PASS();
}

static void test_lpda_gain(void)
{
    TEST("LPDA gain estimate");
    double g = at_lpda_gain(0.87, 0.15, 15);
    CHECK(g > 1.0 && g < 20.0, "LPDA gain reasonable");
    PASS();
}

static void test_horn_gain(void)
{
    TEST("Horn antenna gain estimate at 10 GHz");
    /* 10cm x 15cm aperture at 10 GHz */
    double g = at_horn_gain(0.10, 0.15, 10e9);
    CHECK(g > 50.0 && g < 500.0, "Horn gain reasonable");
    PASS();
}

static void test_horn_beamwidth(void)
{
    TEST("Horn antenna beamwidth");
    double hp_e, hp_h;
    at_horn_beamwidth(0.10, 0.15, 10e9, &hp_e, &hp_h);
    CHECK(hp_e > 5.0 && hp_e < 90.0, "E-plane beamwidth reasonable");
    CHECK(hp_h > 5.0 && hp_h < 90.0, "H-plane beamwidth reasonable");
    PASS();
}

/* ---- L4: NSA height scan sanity ---- */
static void test_nsa_height_scan_sanity(void)
{
    TEST("NSA height scan produces valid heights");
    double h_opt, max_ratio;
    /* 10m site, 100 MHz */
    ct_nsa_height_scan_opt(10.0, 2.0, 100e6, RE_POL_HORIZONTAL, &h_opt, &max_ratio);
    CHECK(h_opt >= 1.0 && h_opt <= 4.0, "10m site height scan valid");
    PASS();
}

int main(void)
{
    printf("=== Radiated Emission Antenna Chamber Test Suite ===\n\n");

    /* L1: Definitions */
    printf("--- L1: Core Definitions ---\n");
    test_dbm_to_dbuv();
    test_dbm_to_dbuv_75ohm();
    test_antenna_factor_halfwave_dipole();
    test_antenna_factor_high_gain();
    test_field_strength_compute();

    /* L3: Mathematical Structures */
    printf("\n--- L3: Mathematical Structures ---\n");
    test_free_space_impedance();
    test_wavenumber();
    test_wavelength();
    test_fspl();
    test_field_region_far();
    test_field_region_near();
    test_wave_impedance_far_field();
    test_wave_impedance_near_field();
    test_poynting();

    /* L4: Fundamental Laws */
    printf("\n--- L4: Fundamental Laws ---\n");
    test_field_from_power();
    test_ground_reflection_horizontal();
    test_nsa_theoretical();
    test_svswr();

    /* L5: Algorithms */
    printf("\n--- L5: Algorithms ---\n");
    test_af_table_init();
    test_af_interpolate();
    test_height_scan_opt();
    test_far_field_boundary();
    test_biconical_gain();
    test_lpda_gain();
    test_horn_gain();
    test_horn_beamwidth();

    /* L6: Canonical Problems */
    printf("\n--- L6: Canonical Problems ---\n");
    test_cispr32_classb_limit();
    test_fcc15_classb_limit();
    test_fcc15_classb_above_1ghz();
    test_cispr25_limit();
    test_emission_scan();
    test_pass_fail();
    test_distance_extrapolation();

    /* L7: Applications */
    printf("\n--- L7: Applications ---\n");
    test_cispr22_classb_10m_limit();
    test_mil461_limit();

    printf("\n====================================\n");
    printf("RESULTS: %d/%d tests passed\n", tests_passed, tests_run);
    printf("====================================\n");

    return (tests_passed == tests_run) ? 0 : 1;
}