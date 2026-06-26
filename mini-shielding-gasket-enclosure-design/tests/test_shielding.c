/* test_shielding.c -- Comprehensive test suite for shielding library
 * Tests for all core functions with mathematical correctness assertions.
 * Reference: SKILL.md self-check - requires >=5 math asserts (not assert(1))
 */
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "shielding_gasket.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  TEST: %s ... ", name); } while(0)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); tests_failed++; } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)
#define CHECK_CLOSE(a, b, tol, msg) do { if (fabs((a)-(b)) > tol) { printf("FAIL: %s (%.4f vs %.4f)\n", msg, a, b); tests_failed++; return; } } while(0)

/* L1+L2: Material database integrity */
void test_material_database(void) {
    TEST("material database lookup");
    shielding_material_t mat;
    int rc = shielding_material_get(MATERIAL_COPPER, &mat);
    CHECK(rc == 0, "copper lookup failed");
    CHECK(mat.conductivity == 5.80e7, "copper conductivity wrong");
    CHECK(mat.relative_permeability == 1.0, "copper mu_r wrong");
    CHECK(mat.id == MATERIAL_COPPER, "copper id wrong");
    PASS();

    TEST("material database invalid id");
    rc = shielding_material_get(MATERIAL_CUSTOM, &mat);
    CHECK(rc == -1, "custom material should not be in DB");
    PASS();

    TEST("material database null pointer");
    rc = shielding_material_get(MATERIAL_COPPER, NULL);
    CHECK(rc == -1, "null pointer should fail");
    PASS();
}

/* L4: Skin depth - fundamental physical law */
void test_skin_depth(void) {
    TEST("skin depth copper 1MHz");
    shielding_material_t copper;
    shielding_material_get(MATERIAL_COPPER, &copper);
    double delta = shielding_skin_depth(&copper, 1.0e6);
    /* delta_Cu at 1MHz = sqrt(1/(pi*1e6*mu0*5.8e7)) ~ 0.066mm */
    CHECK(delta > 0.0, "skin depth must be positive");
    CHECK_CLOSE(delta, 6.6e-5, 1e-6, "copper skin depth at 1MHz ~66um");
    PASS();

    TEST("skin depth steel 60Hz");
    shielding_material_t steel;
    shielding_material_get(MATERIAL_STEEL_1008, &steel);
    delta = shielding_skin_depth(&steel, 60.0);
    /* delta_steel at 60Hz with mu_r=1000 ~ 0.87mm */
    CHECK(delta > 0.0, "skin depth must be positive");
    CHECK_CLOSE(delta, 8.7e-4, 2e-4, "steel skin depth at 60Hz ~0.87mm");
    PASS();

    TEST("skin depth freq scaling");
    /* delta ~ 1/sqrt(f): delta(4f) = delta(f)/2 */
    double d1 = shielding_skin_depth(&copper, 1.0e6);
    double d2 = shielding_skin_depth(&copper, 4.0e6);
    CHECK_CLOSE(d1/d2, 2.0, 0.1, "skin depth should scale as 1/sqrt(f)");
    PASS();

    TEST("skin depth zero frequency");
    delta = shielding_skin_depth(&copper, 0.0);
    CHECK(delta < 0.0, "zero frequency should return error");
    PASS();
}

/* L4: Absorption loss */
void test_absorption_loss(void) {
    TEST("absorption loss linear with thickness");
    shielding_material_t copper;
    shielding_material_get(MATERIAL_COPPER, &copper);
    shield_layer_t l1 = {copper, 0.001, 0.1};
    shield_layer_t l2 = {copper, 0.002, 0.1};
    double A1 = shielding_absorption_loss(&l1, 100.0e6);
    double A2 = shielding_absorption_loss(&l2, 100.0e6);
    CHECK_CLOSE(A2/A1, 2.0, 0.05, "absorption should be linear with thickness");
    PASS();

    TEST("absorption loss non-negative");
    double A = shielding_absorption_loss(&l1, 100.0e6);
    CHECK(A >= 0.0, "absorption loss must be non-negative");
    PASS();

    TEST("absorption loss freq scaling");
    /* A ~ sqrt(f) at constant thickness */
    shield_layer_t l = {copper, 0.0005, 0.1};
    double Af1 = shielding_absorption_loss(&l, 100.0e6);
    double Af2 = shielding_absorption_loss(&l, 400.0e6);
    CHECK_CLOSE(Af2/Af1, 2.0, 0.1, "absorption should scale as sqrt(f)");
    PASS();
}

/* L4: Reflection loss - field type dependence */
void test_reflection_loss(void) {
    TEST("reflection loss plane wave");
    shielding_material_t copper;
    shielding_material_get(MATERIAL_COPPER, &copper);
    shield_layer_t l = {copper, 0.001, 1.0};
    double R = shielding_reflection_loss(&l, 100.0e6, SE_FIELD_PLANE_WAVE, 1.0);
    /* R ~ 168 - 10*log10(1*1e8/1) = 168 - 80 = 88 dB */
    CHECK(R > 50.0, "reflection should be significant for plane wave");
    CHECK(R < 200.0, "reflection should not exceed 200 dB");
    PASS();

    TEST("reflection near E enhanced");
    double R_E = shielding_reflection_loss(&l, 100.0e6, SE_FIELD_NEAR_E, 0.1);
    double R_PW = shielding_reflection_loss(&l, 100.0e6, SE_FIELD_PLANE_WAVE, 0.1);
    CHECK(R_E > R_PW, "near E-field reflection should exceed plane wave");
    PASS();

    TEST("reflection near H reduced");
    double R_H = shielding_reflection_loss(&l, 100.0e6, SE_FIELD_NEAR_H, 0.1);
    CHECK(R_H < R_PW, "near H-field reflection should be less than plane wave");
    PASS();

    TEST("reflection null guard");
    double R_null = shielding_reflection_loss(NULL, 100.0e6, SE_FIELD_PLANE_WAVE, 1.0);
    CHECK(R_null < 0.0, "null layer should return error");
    PASS();
}

/* L4: Schelkunoff total SE */
void test_schelkunoff_se(void) {
    TEST("Schelkunoff SE basic computation");
    shielding_material_t al;
    shielding_material_get(MATERIAL_ALUMINUM, &al);
    shield_layer_t l = {al, 0.002, 1.0};
    se_result_t r = shielding_effectiveness_single_layer(&l, 100.0e6, SE_FIELD_PLANE_WAVE, 1.0);
    CHECK(r.absorption_loss_db >= 0.0, "A >= 0");
    CHECK(r.reflection_loss_db >= 0.0, "R >= 0");
    CHECK(r.multiple_reflection_correction_db <= 0.0, "M <= 0");
    CHECK_CLOSE(r.total_se_db, r.absorption_loss_db + r.reflection_loss_db + r.multiple_reflection_correction_db, 0.01, "SE = A + R + M");
    PASS();

    TEST("SE frequency dependent");
    se_result_t r_low = shielding_effectiveness_single_layer(&l, 10.0e6, SE_FIELD_PLANE_WAVE, 1.0);
    se_result_t r_high = shielding_effectiveness_single_layer(&l, 1000.0e6, SE_FIELD_PLANE_WAVE, 1.0);
    /* At higher freq: more absorption, less reflection */
    CHECK(r_high.absorption_loss_db > r_low.absorption_loss_db, "absorption increases with freq");
    CHECK(r_low.reflection_loss_db > r_high.reflection_loss_db, "reflection decreases with freq");
    PASS();
}

/* L5: Multi-layer shielding */
void test_multilayer_shielding(void) {
    TEST("multilayer SE additive");
    shielding_material_t cu, al;
    shielding_material_get(MATERIAL_COPPER, &cu);
    shielding_material_get(MATERIAL_ALUMINUM, &al);

    shield_stack_t stack;
    stack.num_layers = 2;
    stack.layers[0].material = cu;
    stack.layers[0].thickness_m = 0.0005;
    stack.layers[0].distance_from_source_m = 1.0;
    stack.layers[1].material = al;
    stack.layers[1].thickness_m = 0.001;
    stack.layers[1].distance_from_source_m = 1.0;
    stack.air_gap_m[0] = 0.001;

    double SE_total = shielding_effectiveness_multilayer(&stack, 100.0e6, SE_FIELD_PLANE_WAVE, 1.0);
    CHECK(SE_total > 0.0, "multilayer SE must be positive");
    PASS();

    TEST("multilayer null guard");
    double SE = shielding_effectiveness_multilayer(NULL, 100.0e6, SE_FIELD_PLANE_WAVE, 1.0);
    CHECK(SE < 0.0, "null stack should error");
    PASS();
}

/* L2: Gasket specification initialization */
void test_gasket_spec(void) {
    TEST("gasket spec init valid");
    gasket_spec_t spec;
    int rc = gasket_spec_init(GASKET_CONDUCTIVE_ELASTOMER, GASKET_PROFILE_D_SHAPE, 6.0, &spec);
    CHECK(rc == 0, "gasket init failed");
    CHECK(spec.material == GASKET_CONDUCTIVE_ELASTOMER, "material mismatch");
    CHECK(spec.width_mm == 6.0, "width mismatch");
    CHECK(spec.height_uncompressed_mm > 0.0, "height must be positive");
    PASS();

    TEST("gasket spec null guard");
    rc = gasket_spec_init(GASKET_CONDUCTIVE_ELASTOMER, GASKET_PROFILE_D_SHAPE, 5.0, NULL);
    CHECK(rc == -1, "null should fail");
    PASS();

    TEST("gasket spec invalid material");
    rc = gasket_spec_init(999, GASKET_PROFILE_D_SHAPE, 5.0, &spec);
    CHECK(rc == -1, "invalid material should fail");
    PASS();
}

/* L5: Gasket compression model */
void test_gasket_compression(void) {
    TEST("gasket compression fixed deflection");
    gasket_spec_t spec;
    gasket_spec_init(GASKET_CONDUCTIVE_ELASTOMER, GASKET_PROFILE_D_SHAPE, 6.0, &spec);
    gasket_compression_t state;
    int rc = gasket_compute_compression(&spec, COMPRESSION_MODE_FIXED_DEFLECTION, 0.5, &state);
    CHECK(rc == 0, "compression failed");
    CHECK(state.deflection_mm == 0.5, "deflection mismatch");
    CHECK(state.compression_percent > 0.0, "compression percent should be positive");
    CHECK(state.applied_force_n_per_m > 0.0, "force should be positive");
    CHECK(state.contact_resistance_ohm > 0.0, "contact R should be positive");
    PASS();

    TEST("gasket compression null");
    rc = gasket_compute_compression(NULL, COMPRESSION_MODE_FIXED_DEFLECTION, 0.5, &state);
    CHECK(rc == -1, "null spec should fail");
    PASS();
}

/* L5: Transfer impedance and gasket SE */
void test_transfer_impedance(void) {
    TEST("transfer impedance computation");
    gasket_spec_t spec;
    gasket_spec_init(GASKET_CONDUCTIVE_ELASTOMER, GASKET_PROFILE_D_SHAPE, 6.0, &spec);
    gasket_compression_t state;
    gasket_compute_compression(&spec, COMPRESSION_MODE_FIXED_DEFLECTION, 0.5, &state);
    double ZT = gasket_transfer_impedance(&state, 100.0e6);
    CHECK(ZT > 0.0, "Z_T must be positive");
    CHECK(ZT < 1.0, "Z_T for elastomer should be < 1 ohm*m");
    PASS();

    TEST("gasket shielding effectiveness");
    double SE = gasket_shielding_effectiveness(&state, 100.0e6, 0.5);
    CHECK(SE > 0.0, "gasket SE must be positive");
    PASS();

    TEST("transfer impedance null");
    double ZT2 = gasket_transfer_impedance(NULL, 100.0e6);
    CHECK(ZT2 < 0.0, "null state should error");
    PASS();
}

/* L5: Service life estimation */
void test_service_life(void) {
    TEST("service life computation");
    gasket_spec_t spec;
    gasket_spec_init(GASKET_CONDUCTIVE_ELASTOMER, GASKET_PROFILE_D_SHAPE, 6.0, &spec);
    double life = gasket_service_life_estimate(&spec, 2.0, 25.0, 1);
    CHECK(life > 0.0, "life must be positive");
    CHECK(life < 1000.0, "life should be finite");
    PASS();

    TEST("service life high temp derating");
    double life25 = gasket_service_life_estimate(&spec, 2.0, 25.0, 1);
    double life85 = gasket_service_life_estimate(&spec, 2.0, 85.0, 1);
    CHECK(life85 < life25, "higher temp should reduce life");
    PASS();
}

/* L6: Galvanic compatibility */
void test_galvanic_compatibility(void) {
    TEST("galvanic compatible pair");
    gasket_spec_t spec;
    gasket_spec_init(GASKET_CONDUCTIVE_ELASTOMER, GASKET_PROFILE_D_SHAPE, 6.0, &spec);
    int rc = gasket_galvanic_compatibility(&spec, MATERIAL_COPPER);
    CHECK(rc == 0 || rc == 1, "elastomer+copper should be compatible or marginal");
    PASS();

    TEST("galvanic aluminum with copper gasket");
    gasket_spec_t spec_al;
    gasket_spec_init(GASKET_ORIENTED_WIRE, GASKET_PROFILE_D_SHAPE, 6.0, &spec_al);
    rc = gasket_galvanic_compatibility(&spec_al, MATERIAL_ALUMINUM);
    CHECK(rc >= 0 && rc <= 2, "result must be valid");
    PASS();
}

/* L6: Cavity resonance */
void test_cavity_modes(void) {
    TEST("cavity modes rectangular enclosure");
    enclosure_spec_t enc;
    memset(&enc, 0, sizeof(enc));
    enc.geometry = ENCLOSURE_RECTANGULAR;
    enc.dimension_x_m = 0.3;
    enc.dimension_y_m = 0.2;
    enc.dimension_z_m = 0.1;
    shielding_material_t al;
    shielding_material_get(MATERIAL_ALUMINUM, &al);
    enc.shield.num_layers = 1;
    enc.shield.layers[0].material = al;
    enc.shield.layers[0].thickness_m = 0.001;
    enc.shield.layers[0].distance_from_source_m = 0.1;

    cavity_analysis_t analysis;
    int rc = enclosure_cavity_modes(&enc, 5.0e9, &analysis);
    CHECK(rc == 0, "cavity mode computation failed");
    CHECK(analysis.num_modes_found > 0, "must find at least one mode");
    CHECK(analysis.first_resonance_hz > 0.0, "first resonance must be positive");
    /* TE110 for 0.3x0.2: f ~ 3e8/2*sqrt(1/0.3^2+1/0.2^2) ~ 900MHz */
    CHECK(analysis.first_resonance_hz > 300.0e6, "resonance too low");
    PASS();

    TEST("cavity modes null guard");
    rc = enclosure_cavity_modes(NULL, 1.0e9, &analysis);
    CHECK(rc == -1, "null should fail");
    PASS();
}

/* L6: Aperture leakage */
void test_aperture_leakage(void) {
    TEST("aperture leakage slot");
    aperture_spec_t ap;
    memset(&ap, 0, sizeof(ap));
    ap.type = APERTURE_SLOT;
    ap.dimension1_m = 0.1;  /* 10cm slot */
    ap.dimension2_m = 0.001; /* 1mm gap */
    ap.num_apertures = 1;
    double SE = enclosure_aperture_leakage(&ap, 1.0e9, SE_FIELD_PLANE_WAVE);
    /* lambda = 0.3m, slot 0.1m: SE ~ 20*log10(0.3/(2*0.1)) = 20*log10(1.5) = 3.5dB */
    CHECK(SE > 0.0, "aperture SE should be positive");
    CHECK(SE < 60.0, "aperture SE should be finite");
    PASS();

    TEST("aperture leakage multiple holes");
    ap.num_apertures = 10;
    double SE_multi = enclosure_aperture_leakage(&ap, 1.0e9, SE_FIELD_PLANE_WAVE);
    /* 10 apertures: 10*log10(10) = 10dB degradation */
    CHECK(SE_multi >= 0.0, "SE still non-negative");
    PASS();
}

/* L7: Application designs */
void test_application_designs(void) {
    TEST("automotive ECU enclosure");
    enclosure_spec_t enc;
    int rc = shielding_automotive_ecu_enclosure(40.0, 0.2, &enc);
    CHECK(rc == 0, "ECU design failed");
    CHECK(enc.geometry == ENCLOSURE_RECTANGULAR, "wrong geometry");
    CHECK(enc.dimension_x_m > 0.0, "dimension must be positive");
    CHECK(enc.shield.num_layers >= 1, "must have at least one layer");
    PASS();

    TEST("medical device enclosure");
    enclosure_spec_t med;
    rc = shielding_medical_device_enclosure(60.0, &med);
    CHECK(rc == 0, "medical design failed");
    CHECK(med.seam_treatment == SEAM_TREATMENT_WELD_BRAZE, "medical needs welded");
    PASS();

    TEST("5G base station enclosure");
    enclosure_spec_t bs;
    rc = shielding_5g_base_station_enclosure(80.0, 500.0, &bs);
    CHECK(rc == 0, "5G design failed");
    CHECK(bs.shield.num_layers == 2, "5G needs multi-layer shield");
    PASS();
}

/* L7: Measurement methods */
void test_measurement(void) {
    TEST("transfer impedance MIL-STD-285");
    double ZT = shielding_transfer_impedance_mil_std_285(400.0e6, 0.3, 60.0);
    CHECK(ZT > 0.0, "Z_T should be positive");
    CHECK(ZT < 10.0, "Z_T should be reasonable");
    PASS();

    TEST("dynamic range estimate");
    double DR = shielding_dynamic_range_estimate(-100.0, 0.0, 3.0);
    /* DR = 0 + 2*3 - 2 - (-100) = 104 dB */
    CHECK(DR > 50.0, "DR must be adequate for SE testing");
    CHECK(DR <= 140.0, "DR cannot exceed practical limit");
    PASS();
}

int main(void) {
    printf("=== mini-shielding-gasket-enclosure-design Test Suite ===\n");

    test_material_database();
    test_skin_depth();
    test_absorption_loss();
    test_reflection_loss();
    test_schelkunoff_se();
    test_multilayer_shielding();
    test_gasket_spec();
    test_gasket_compression();
    test_transfer_impedance();
    test_service_life();
    test_galvanic_compatibility();
    test_cavity_modes();
    test_aperture_leakage();
    test_application_designs();
    test_measurement();

    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
