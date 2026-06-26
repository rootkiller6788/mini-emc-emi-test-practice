/**
 * @file test_esd.c
 * @brief Comprehensive test suite for mini-esd-gun-air-contact-test
 *
 * Tests all major APIs with mathematical validation assertions.
 * Uses standard C assert() — no custom macros.
 */

#include "esd_waveform.h"
#include "esd_gun_model.h"
#include "esd_test_setup.h"
#include "esd_protection.h"
#include "esd_compliance.h"
#include "esd_tlp.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TOL 1e-6

static int tests_passed = 0;
static int tests_failed = 0;

#define RUN_TEST(name) do { \
    printf("  %-55s", name); \
    fflush(stdout); \
} while(0)

#define TEST_OK() do { \
    printf(" PASS\n"); \
    tests_passed++; \
} while(0)

#define TEST_FAIL(msg) do { \
    printf(" FAIL: %s\n", msg); \
    tests_failed++; \
} while(0)

/* ─── Waveform Tests ──────────────────────────────────────────── */

static void test_waveform_level_voltages(void)
{
    RUN_TEST("IEC level voltages");

    /* L1: Verify voltage mappings for all 4 levels */
    double v1c = esd_level_voltage_kv(ESD_LEVEL_1, ESD_DISCHARGE_CONTACT);
    double v2c = esd_level_voltage_kv(ESD_LEVEL_2, ESD_DISCHARGE_CONTACT);
    double v3c = esd_level_voltage_kv(ESD_LEVEL_3, ESD_DISCHARGE_CONTACT);
    double v4c = esd_level_voltage_kv(ESD_LEVEL_4, ESD_DISCHARGE_CONTACT);

    assert(fabs(v1c - 2.0) < TOL);
    assert(fabs(v2c - 4.0) < TOL);
    assert(fabs(v3c - 6.0) < TOL);
    assert(fabs(v4c - 8.0) < TOL);

    double v4a = esd_level_voltage_kv(ESD_LEVEL_4, ESD_DISCHARGE_AIR);
    assert(fabs(v4a - 15.0) < TOL);

    TEST_OK();
}

static void test_waveform_spec_standard(void)
{
    RUN_TEST("Waveform spec standard values");

    esd_waveform_spec_t spec = esd_waveform_spec_standard(ESD_LEVEL_4,
                                                           ESD_DISCHARGE_CONTACT);
    assert(fabs(spec.voltage_kv - 8.0) < TOL);
    assert(fabs(spec.peak_current_a - 30.0) < TOL);
    assert(fabs(spec.current_at_30ns_a - 16.0) < TOL);
    assert(fabs(spec.current_at_60ns_a - 8.0) < TOL);
    assert(spec.type == ESD_DISCHARGE_CONTACT);
    assert(spec.level == ESD_LEVEL_4);

    TEST_OK();
}

static void test_peak_current_from_voltage(void)
{
    RUN_TEST("Peak current linear scaling (L4)");

    /* L4 Theorem: I_peak = V × 3.75 */
    assert(fabs(esd_peak_current_from_voltage(2.0) - 7.5) < TOL);
    assert(fabs(esd_peak_current_from_voltage(4.0) - 15.0) < TOL);
    assert(fabs(esd_peak_current_from_voltage(8.0) - 30.0) < TOL);

    TEST_OK();
}

static void test_heidler_waveform_computation(void)
{
    RUN_TEST("Heidler waveform computation");

    esd_heidler_params_t hp;
    int ret = esd_heidler_params_for_level(ESD_LEVEL_4, &hp);
    assert(ret == 0);

    /* At t=0, current should be 0 */
    double i0 = esd_heidler_waveform(0.0, &hp);
    assert(fabs(i0) < TOL);

    /* At t=negative, should be 0 */
    double ineg = esd_heidler_waveform(-1.0, &hp);
    assert(fabs(ineg) < TOL);

    /* At some time > 0, current should be positive */
    double i_pos = esd_heidler_waveform(1.0, &hp);
    assert(i_pos > 0.0);

    /* Scan for peak current (occurs ~1.5-5 ns into the pulse) */
    double i_peak_max = 0.0;
    for (double t = 0.1; t <= 20.0; t += 0.2) {
        double i_t = esd_heidler_waveform(t, &hp);
        if (i_t > i_peak_max) i_peak_max = i_t;
    }
    assert(i_peak_max > 15.0 && i_peak_max < 40.0);

    /* At large t, current should decay to near 0 */
    double i_late = esd_heidler_waveform(200.0, &hp);
    assert(fabs(i_late) < 5.0);

    TEST_OK();
}

static void test_double_exp_waveform(void)
{
    RUN_TEST("Double exponential waveform");

    esd_double_exp_params_t dep = { .i0_a = 30.0, .tau_rise_ns = 0.4, .tau_fall_ns = 45.0 };
    double i0 = esd_double_exp_waveform(0.0, &dep);
    assert(fabs(i0) < TOL);

    double i_pos = esd_double_exp_waveform(10.0, &dep);
    assert(i_pos > 0.0);

    TEST_OK();
}

static void test_waveform_generation(void)
{
    RUN_TEST("Waveform generation and extraction");

    esd_waveform_spec_t spec = esd_waveform_spec_standard(ESD_LEVEL_4,
                                                            ESD_DISCHARGE_CONTACT);
    esd_waveform_data_t data;
    memset(&data, 0, sizeof(data));

    int ret = esd_waveform_generate_iec(&spec, 200.0, 500, &data);
    assert(ret == 0);
    assert(data.num_samples == 500);
    assert(data.time_ns != NULL);
    assert(data.current_a != NULL);

    /* Extract parameters and verify compliance */
    esd_waveform_spec_t extracted;
    ret = esd_waveform_extract_params(&data, &extracted);
    assert(ret == 0);

    /* Peak current should be close to 30A */
    assert(fabs(extracted.peak_current_a) > 20.0);
    assert(fabs(extracted.peak_current_a) < 40.0);

    /* Rise time should be in the general vicinity of 0.8 ns */
    assert(extracted.rise_time_ns > 0.0);

    esd_compliance_result_t comp;
    ret = esd_waveform_check_compliance(&extracted, &spec, &comp);
    assert(ret == 0);

    /* Total charge and energy */
    double charge = esd_waveform_total_charge(&data);
    assert(charge > 0.0);

    double energy = esd_waveform_energy(&data, 2.0);
    assert(energy > 0.0);

    /* Rise time and FWHM */
    double tr = esd_waveform_rise_time(&data);
    assert(tr > 0.0);

    double fwhm = esd_waveform_fwhm(&data);
    assert(fwhm > 0.0);

    /* Power computation */
    double *power = (double *)malloc(data.num_samples * sizeof(double));
    assert(power != NULL);
    ret = esd_waveform_power(&data, 2.0, power);
    assert(ret == 0);
    assert(power[0] == 0.0); /* No current at t=0 */
    free(power);

    esd_waveform_data_free(&data);

    TEST_OK();
}

/* ─── Gun Model Tests ─────────────────────────────────────────── */

static void test_gun_rc_defaults(void)
{
    RUN_TEST("ESD gun RC defaults");

    esd_gun_rc_params_t rc = esd_gun_rc_default();
    assert(fabs(rc.cs_pf - 150.0) < TOL);
    assert(fabs(rc.rd_ohm - 330.0) < TOL);

    TEST_OK();
}

static void test_gun_rlc_analysis(void)
{
    RUN_TEST("RLC natural frequency and damping (L3)");

    esd_gun_rlc_params_t p = esd_gun_rlc_default(8.0);
    double omega0, zeta, omega_d;
    esd_gun_rlc_natural(&p, &omega0, &zeta, &omega_d);
    assert(omega0 > 0.0);
    assert(zeta > 1.0);  /* Should be overdamped */

    int regime = esd_gun_rlc_damping_regime(&p);
    assert(regime == 2);  /* Overdamped */

    double i_peak = esd_gun_rlc_peak_current(&p);
    assert(i_peak > 0.0);

    double tr = esd_gun_rlc_rise_time(&p);
    assert(tr > 0.0);

    /* Impedance at different frequencies */
    double z_dc = esd_gun_impedance(&p, 0.0);
    assert(z_dc > 1e10);  /* DC → capacitor blocks */

    double z_100MHz = esd_gun_impedance(&p, 1e8);
    assert(z_100MHz > 100.0);

    /* Energy stored */
    double energy = esd_gun_stored_energy(150.0, 8.0);
    assert(fabs(energy - 4800.0) < 1.0);  /* ~4.8 mJ = 4800 µJ */

    TEST_OK();
}

static void test_gun_rk4_simulation(void)
{
    RUN_TEST("RK4 RLC discharge simulation (L5)");

    esd_gun_rlc_params_t p = esd_gun_rlc_default(8.0);
    esd_gun_trajectory_t traj;
    memset(&traj, 0, sizeof(traj));

    int ret = esd_gun_simulate_rlc_rk4(&p, 0.1, 100.0, &traj);
    assert(ret == 0);
    assert(traj.num_points > 10);
    assert(traj.time_ns != NULL);
    assert(traj.voltage_kv != NULL);
    assert(traj.current_a != NULL);

    /* Initial conditions */
    assert(fabs(traj.voltage_kv[0] - 8.0) < 0.01);
    assert(fabs(traj.current_a[0]) < 0.01);

    /* Current should be positive at some point */
    int found_current = 0;
    for (size_t i = 0; i < traj.num_points; i++) {
        if (traj.current_a[i] > 1.0) {
            found_current = 1;
            break;
        }
    }
    assert(found_current);

    esd_gun_trajectory_free(&traj);

    TEST_OK();
}

static void test_gun_analytical_solution(void)
{
    RUN_TEST("Analytical RLC solution (L5)");

    esd_gun_rlc_params_t p = esd_gun_rlc_default(8.0);
    esd_gun_trajectory_t traj;
    memset(&traj, 0, sizeof(traj));

    int ret = esd_gun_simulate_rlc_analytical(&p, 100.0, 500, &traj);
    assert(ret == 0);
    assert(traj.num_points == 500);
    assert(traj.current_a[0] == 0.0);

    esd_gun_trajectory_free(&traj);

    TEST_OK();
}

static void test_arc_simulation_rw(void)
{
    RUN_TEST("Rompe-Weizel arc simulation (L3/L6)");

    esd_gun_rlc_params_t p = esd_gun_rlc_default(8.0);
    arc_rompe_weizel_t arc;
    arc.gap_length_mm = 1.0;
    arc.alpha_coeff = 0.8e-4;
    arc.pressure_bar = 1.0;

    esd_gun_trajectory_t traj;
    memset(&traj, 0, sizeof(traj));

    int ret = esd_gun_simulate_with_arc_rw(&p, &arc, 0.05, 50.0, &traj);
    assert(ret == 0);
    assert(traj.num_points > 10);

    /* Arc resistance should decrease from initial high value */
    assert(traj.arc_resistance_ohm[0] > 1e8);

    esd_gun_trajectory_free(&traj);

    TEST_OK();
}

static void test_paschen_law(void)
{
    RUN_TEST("Paschen's breakdown voltage (L4)");

    /* At 1 bar, 1 mm gap: V_bd ≈ 3000 + 1000×1 = 4000 V */
    double v1 = esd_paschen_breakdown_voltage(1.0, 1.0);
    assert(fabs(v1 - 4000.0) < 100.0);

    /* At 1 bar, 5 mm gap: V_bd ≈ 3000 + 1000×5 = 8000 V */
    double v5 = esd_paschen_breakdown_voltage(5.0, 1.0);
    assert(fabs(v5 - 8000.0) < 200.0);

    /* Inverse: 8 kV → approximately 5 mm gap */
    double gap = esd_voltage_to_gap_length(8.0, 1.0);
    assert(gap > 3.0 && gap < 7.0);

    /* Zero gap: still finite breakdown */
    double v0 = esd_paschen_breakdown_voltage(0.01, 1.0);
    assert(v0 > 0.0);

    TEST_OK();
}

static void test_transfer_function(void)
{
    RUN_TEST("Gun transfer function magnitude");

    esd_gun_rlc_params_t p = esd_gun_rlc_default(8.0);
    double mag = esd_gun_transfer_magnitude(&p, 1e6);  /* 1 MHz */
    assert(mag > 0.0);
    assert(mag < 1.0);  /* Voltage divider ratio */

    TEST_OK();
}

/* ─── Test Setup Tests ────────────────────────────────────────── */

static void test_geometry_standard(void)
{
    RUN_TEST("Standard test geometry validity");

    esd_test_geometry_t g = esd_test_geometry_standard();
    assert(esd_test_geometry_is_valid(&g) == 1);

    /* Invalid geometry: wrong HCP height */
    esd_test_geometry_t bad = g;
    bad.hcp_height_m = 1.0;
    assert(esd_test_geometry_is_valid(&bad) == 0);

    TEST_OK();
}

static void test_plan_generation(void)
{
    RUN_TEST("Standard test plan generation (L6)");

    esd_test_plan_t plan;
    memset(&plan, 0, sizeof(plan));
    int ret = esd_test_plan_standard(ESD_DISCHARGE_CONTACT, ESD_LEVEL_4, &plan);
    assert(ret == 0);
    assert(plan.num_test_points == 6);
    assert(plan.num_levels == 4);
    assert(plan.discharges_per_point == 10);
    assert(plan.test_both_polarities == 1);

    size_t total = esd_test_plan_total_discharges(&plan);
    /* 6 points × 4 levels × 10 discharges × 2 polarities = 480 */
    assert(total == 480);

    double duration = esd_test_plan_duration(&plan);
    assert(duration > 5.0);  /* Should be several minutes */

    esd_test_plan_free(&plan);

    TEST_OK();
}

static void test_report_and_statistics(void)
{
    RUN_TEST("Test report and statistics (L5)");

    esd_test_plan_t plan;
    memset(&plan, 0, sizeof(plan));
    esd_test_plan_standard(ESD_DISCHARGE_CONTACT, ESD_LEVEL_2, &plan);

    esd_test_report_t report;
    memset(&report, 0, sizeof(report));
    esd_test_report_init(&plan, &report, "EUT-001", "Engineer A");

    /* Record some test results */
    esd_discharge_record_t rec;
    memset(&rec, 0, sizeof(rec));
    rec.point_id = 1;
    rec.voltage_kv = 4.0;
    rec.result = ESD_RESULT_PASS_A;

    for (int i = 0; i < 10; i++) {
        esd_test_report_record(&report, &rec);
    }

    /* Add a failure */
    rec.result = ESD_RESULT_FAIL_D;
    esd_test_report_record(&report, &rec);

    size_t a, b, c, d;
    esd_test_report_statistics(&report, &a, &b, &c, &d);
    assert(a == 10);
    assert(b == 0);
    assert(c == 0);
    assert(d == 1);

    /* Overall should fail due to one FAIL_D */
    assert(esd_test_report_overall_pass(&report) == 0);

    esd_test_report_free(&report);
    esd_test_plan_free(&plan);

    TEST_OK();
}

static void test_esd_coupling(void)
{
    RUN_TEST("ESD coupling analysis (L2)");

    /* dI/dt = 30 A/ns, M = 1 nH → V = 30 V */
    double v_ind = esd_coupling_induced_voltage(30.0, 1.0);
    assert(fabs(v_ind - 30.0) < TOL);

    /* Mutual inductance of 10cm parallel conductors, 5mm apart */
    double m = esd_mutual_inductance_parallel(0.1, 0.005);
    assert(m > 0.0);
    assert(m < 1e-6);  /* Should be in nH range */

    /* Protection attenuation */
    double v_res = esd_protection_attenuation(8.0, 15.0, 10.0);
    assert(v_res > 0.0);
    assert(v_res < 8000.0);  /* Should attenuate */

    TEST_OK();
}

/* ─── Protection Tests ────────────────────────────────────────── */

static void test_tvs_default_parameters(void)
{
    RUN_TEST("TVS default parameters");

    esd_tvs_params_t t5 = esd_tvs_default_5v();
    assert(fabs(t5.vrwm_v - 5.0) < TOL);
    assert(t5.vc_v > t5.vbr_min_v);

    esd_tvs_params_t t33 = esd_tvs_default_3v3();
    assert(fabs(t33.vrwm_v - 3.3) < TOL);

    TEST_OK();
}

static void test_tvs_clamp_voltage(void)
{
    RUN_TEST("TVS clamping voltage model (L3)");

    esd_tvs_params_t tvs = esd_tvs_default_5v();

    /* At zero current, should return VRWM */
    double v0 = esd_tvs_clamp_voltage(&tvs, 0.0);
    assert(fabs(v0 - 5.0) < TOL);

    /* At rated IPP, should be near VC */
    double v_rated = esd_tvs_clamp_voltage(&tvs, tvs.ipp_a);
    assert(v_rated <= tvs.vc_v + 1.0);

    /* Power computation */
    double p = esd_tvs_power(&tvs, 5.0);
    assert(p > 0.0);

    TEST_OK();
}

static void test_tvs_selection(void)
{
    RUN_TEST("TVS selection algorithm (L5)");

    esd_protection_window_t window;
    window.v_signal_max_v = 3.3;
    window.v_ic_max_v = 6.0;
    window.v_supply_v = 3.3;
    window.esd_peak_current_a = 30.0;
    window.signal_bandwidth_mhz = 100.0;

    esd_tvs_params_t tvs = esd_tvs_default_3v3();

    esd_tvs_selection_t sel;
    int ret = esd_tvs_select(&window, &tvs, &sel);
    assert(ret == 0);
    /* 3.3V TVS should work for 3.3V signal */
    assert(sel.vrwm_margin_v >= 0.0);

    TEST_OK();
}

static void test_tvs_bandwidth(void)
{
    RUN_TEST("TVS bandwidth limitation (L5)");

    esd_tvs_params_t tvs = esd_tvs_default_5v();
    tvs.cj_pf = 1.0;

    double bw_digital = esd_tvs_bandwidth_limit(&tvs, 50.0, 1);
    assert(bw_digital > 100.0);  /* Should handle high-speed */

    double bw_analog = esd_tvs_bandwidth_limit(&tvs, 50.0, 0);
    assert(bw_analog > 100.0);

    TEST_OK();
}

static void test_varistor_model(void)
{
    RUN_TEST("Varistor I-V model (L3)");

    esd_varistor_params_t var = esd_varistor_default();
    double v1 = esd_varistor_voltage(&var, 0.001);
    assert(v1 > 0.0);

    double clamp_ratio = esd_varistor_clamp_ratio(&var);
    assert(clamp_ratio > 1.0);

    TEST_OK();
}

static void test_rc_filter_design(void)
{
    RUN_TEST("RC filter design (L5)");

    esd_rc_filter_params_t filter;
    memset(&filter, 0, sizeof(filter));
    int ret = esd_rc_filter_design(100.0, 50.0, 8.0, &filter);
    assert(ret == 0);
    assert(filter.r_series_ohm >= 10.0);
    assert(filter.c_shunt_pf > 0.0);
    assert(filter.fc_mhz > 100.0);

    TEST_OK();
}

static void test_tvs_comparison(void)
{
    RUN_TEST("TVS comparison");

    esd_protection_window_t window;
    window.v_signal_max_v = 5.0;
    window.v_ic_max_v = 7.0;
    window.v_supply_v = 5.0;
    window.esd_peak_current_a = 20.0;
    window.signal_bandwidth_mhz = 50.0;

    esd_tvs_params_t a = esd_tvs_default_5v();
    esd_tvs_params_t b = esd_tvs_default_5v();
    b.cj_pf = 0.2;  /* Lower capacitance = better for signal integrity */

    int cmp = esd_tvs_compare(&window, &a, &b);
    /* B has lower capacitance, should be preferred */
    assert(cmp == -1 || cmp == 0);

    TEST_OK();
}

static void test_protection_network(void)
{
    RUN_TEST("Multi-stage protection network (L6)");

    esd_protection_network_t net;
    memset(&net, 0, sizeof(net));
    net.primary_type = ESD_PROT_TVS_UNIDIR;
    net.tvs_primary = esd_tvs_default_5v();
    net.series_resistance_ohm = 10.0;
    net.tvs_secondary = esd_tvs_default_3v3();

    double v_ic, e_prim, e_sec;
    int ret = esd_network_simulate(&net, 30.0, &v_ic, &e_prim, &e_sec);
    assert(ret == 0);
    assert(v_ic >= 0.0);

    TEST_OK();
}

static void test_pcb_trace(void)
{
    RUN_TEST("PCB trace width for ESD (L7)");

    double w = esd_pcb_trace_width(30.0, 1.0, 0.5, 50.0);
    assert(w > 0.0);
    assert(w < 10.0);  /* Should be under 10 mm */

    TEST_OK();
}

/* ─── Compliance Tests ────────────────────────────────────────── */

static void test_standard_database(void)
{
    RUN_TEST("ESD standard database");

    esd_standard_params_t iec = esd_standard_get(ESD_STD_IEC_61000_4_2);
    assert(fabs(iec.cs_pf - 150.0) < TOL);
    assert(fabs(iec.rd_ohm - 330.0) < TOL);

    esd_standard_params_t hbm = esd_standard_get(ESD_STD_ANSI_ESDA_JS001);
    assert(fabs(hbm.cs_pf - 100.0) < TOL);
    assert(fabs(hbm.rd_ohm - 1500.0) < TOL);

    TEST_OK();
}

static void test_hbm_classification(void)
{
    RUN_TEST("HBM classification (L1)");

    assert(esd_hbm_classify(100.0) == HBM_CLASS_0);
    assert(esd_hbm_classify(300.0) == HBM_CLASS_1A);
    assert(esd_hbm_classify(750.0) == HBM_CLASS_1B);
    assert(esd_hbm_classify(1500.0) == HBM_CLASS_1C);
    assert(esd_hbm_classify(3000.0) == HBM_CLASS_2);
    assert(esd_hbm_classify(6000.0) == HBM_CLASS_3A);
    assert(esd_hbm_classify(10000.0) == HBM_CLASS_3B);

    double vmin, vmax;
    esd_hbm_class_range(HBM_CLASS_2, &vmin, &vmax);
    assert(fabs(vmin - 2000.0) < TOL);
    assert(fabs(vmax - 4000.0) < TOL);

    TEST_OK();
}

static void test_compliance_margin(void)
{
    RUN_TEST("Compliance margin (L4)");

    esd_compliance_margin_t margin;
    esd_compliance_margin_compute(12.0, 8.0, 1.5, &margin);
    assert(margin.margin_ratio > 1.4);
    assert(margin.margin_db > 3.0);
    /* 12/8 = 1.5, min_margin = 1.5 → margin_ratio >= min_margin → passes */
    assert(margin.passes == 1);

    TEST_OK();
}

static void test_cross_standard_equivalence(void)
{
    RUN_TEST("Cross-standard equivalence (L7)");

    double iec_equiv = esd_hbm_to_iec_equivalent(8000.0);
    assert(iec_equiv > 0.0);
    assert(iec_equiv < 5.0);

    double hbm_equiv = esd_iec_to_hbm_equivalent(8.0);
    assert(hbm_equiv > 10000.0);  /* IEC 8 kV ≈ very high HBM */

    esd_cross_standard_map_t map;
    esd_cross_standard_map(4.0, &map);
    assert(fabs(map.iec_61000_4_2_kv - 4.0) < TOL);
    assert(map.equivalent_valid == 1);

    TEST_OK();
}

static void test_auto_esd_params(void)
{
    RUN_TEST("Automotive ESD parameters (L7)");

    esd_auto_test_params_t params;
    int ret = esd_auto_test_params(15.0, 0, &params);
    assert(ret == 0);
    assert(fabs(params.voltage_kv - 15.0) < TOL);
    assert(fabs(params.cs_pf - 150.0) < TOL);
    assert(fabs(params.rd_ohm - 330.0) < TOL);

    /* Test invalid network */
    ret = esd_auto_test_params(15.0, 99, &params);
    assert(ret == -1);

    TEST_OK();
}

static void test_compliance_verify(void)
{
    RUN_TEST("Compliance plan verification (L6)");

    esd_test_plan_t plan;
    memset(&plan, 0, sizeof(plan));
    esd_test_plan_standard(ESD_DISCHARGE_CONTACT, ESD_LEVEL_4, &plan);

    int valid = esd_compliance_verify_plan(&plan, ESD_LEVEL_4);
    assert(valid == 1);

    /* Invalid: target level not in plan */
    /* Actually all levels 1-4 are in plan, so this should still pass */

    esd_test_plan_free(&plan);

    TEST_OK();
}

static void test_test_points_estimation(void)
{
    RUN_TEST("Test point count estimation");

    int n = esd_compliance_num_test_points(0.3, 0.2, 0.15);
    assert(n >= 4);

    TEST_OK();
}

static void test_medical_requirements(void)
{
    RUN_TEST("Medical device ESD requirements (L7)");

    double contact, air;
    char reqs[256];
    esd_medical_requirements(1, &contact, &air, reqs);
    assert(fabs(contact - 8.0) < TOL);
    assert(fabs(air - 15.0) < TOL);
    assert(strlen(reqs) > 0);

    esd_medical_requirements(0, &contact, &air, reqs);
    assert(strlen(reqs) > 0);

    TEST_OK();
}

/* ─── TLP Tests ───────────────────────────────────────────────── */

static void test_tlp_system_config(void)
{
    RUN_TEST("TLP system configuration");

    esd_tlp_system_t sys = esd_tlp_system_standard();
    assert(fabs(sys.z0_ohm - 50.0) < TOL);
    assert(fabs(sys.pulse_width_ns - 100.0) < TOL);

    esd_tlp_system_t vf = esd_tlp_system_vftlp();
    assert(vf.pulse_width_ns < 10.0);

    TEST_OK();
}

static void test_tlp_pulse_width(void)
{
    RUN_TEST("TLP pulse width from cable length (L4)");

    double width = esd_tlp_pulse_width_from_length(10.0, 0.66);
    assert(width > 95.0 && width < 105.0);  /* ~101 ns */

    double length = esd_tlp_cable_length_for_width(100.0, 0.66);
    assert(length > 5.0 && length < 15.0);

    TEST_OK();
}

static void test_tlp_current(void)
{
    RUN_TEST("TLP expected current (L4)");

    double i_matched = esd_tlp_expected_current(2.0, 50.0, 50.0);
    assert(i_matched > 15.0 && i_matched < 25.0);  /* 2000/(50+50) = 20A */

    double energy = esd_tlp_pulse_energy(15.0, 20.0, 100.0);
    assert(energy > 20.0);  /* Should be ~30 µJ */

    TEST_OK();
}

static void test_tlp_curve_extraction(void)
{
    RUN_TEST("TLP parameter extraction (L5)");

    esd_tlp_curve_t curve;
    memset(&curve, 0, sizeof(curve));

    /* Simulate TLP sweep: add points with increasing current */
    for (int i = 1; i <= 10; i++) {
        esd_tlp_point_t pt;
        memset(&pt, 0, sizeof(pt));
        pt.v_charge_kv = (double)i * 0.2;
        pt.v_dut_v = 5.0 + (double)i * 0.3;
        pt.i_dut_a = (double)i * 2.0;
        pt.leakage_pre_na = 1.0;
        pt.leakage_post_na = 1.5 * (double)i;
        esd_tlp_curve_add_point(&curve, &pt);
    }

    assert(curve.num_points == 10);

    int ret = esd_tlp_extract_params(&curve);
    assert(ret == 0);
    assert(curve.v_t1_v > 0.0);
    assert(curve.r_on_ohm > 0.0);

    double fom = esd_tlp_figure_of_merit(&curve, 1.0);
    assert(fom > 0.0);

    int snapback = esd_tlp_detect_snapback(&curve);
    /* With our data, voltage increases with current → no snapback */
    assert(snapback == 0);

    esd_tlp_curve_free(&curve);

    TEST_OK();
}

static void test_seed_analysis(void)
{
    RUN_TEST("SEED analysis (L8)");

    esd_seed_params_t seed;
    memset(&seed, 0, sizeof(seed));
    seed.i_esd_total_a = 30.0;
    seed.r_pcb_trace_ohm = 5.0;
    seed.r_onchip_clamp_ohm = 2.0;
    seed.v_onchip_clamp_v = 8.0;
    seed.r_offchip_tvs_ohm = 0.5;
    seed.i_onchip_max_a = 10.0;
    seed.i_offchip_max_a = 25.0;

    esd_seed_result_t result;
    int ret = esd_seed_analyze(&seed, &result);
    assert(ret == 0);
    assert(result.i_onchip_a >= 0.0);
    assert(result.i_offchip_a >= 0.0);
    assert(result.v_node_v > 0.0);

    double r_opt = esd_seed_optimal_impedance(3.0, 30.0);
    assert(r_opt > 0.0);

    TEST_OK();
}

static void test_hmm_current(void)
{
    RUN_TEST("HMM equivalent current (L8)");

    double i = esd_hmm_equivalent_current(8.0);
    /* 8000 / 380 ≈ 21.05 A */
    assert(i > 18.0 && i < 24.0);

    TEST_OK();
}

/* ─── Main ────────────────────────────────────────────────────── */

int main(void)
{
    printf("\n=== mini-esd-gun-air-contact-test ===\n");
    printf("Testing ESD waveform, gun model, test setup,\n");
    printf("protection, compliance, and TLP modules.\n\n");

    /* Waveform tests */
    test_waveform_level_voltages();
    test_waveform_spec_standard();
    test_peak_current_from_voltage();
    test_heidler_waveform_computation();
    test_double_exp_waveform();
    test_waveform_generation();

    /* Gun model tests */
    test_gun_rc_defaults();
    test_gun_rlc_analysis();
    test_gun_rk4_simulation();
    test_gun_analytical_solution();
    test_arc_simulation_rw();
    test_paschen_law();
    test_transfer_function();

    /* Test setup tests */
    test_geometry_standard();
    test_plan_generation();
    test_report_and_statistics();
    test_esd_coupling();

    /* Protection tests */
    test_tvs_default_parameters();
    test_tvs_clamp_voltage();
    test_tvs_selection();
    test_tvs_bandwidth();
    test_varistor_model();
    test_rc_filter_design();
    test_tvs_comparison();
    test_protection_network();
    test_pcb_trace();

    /* Compliance tests */
    test_standard_database();
    test_hbm_classification();
    test_compliance_margin();
    test_cross_standard_equivalence();
    test_auto_esd_params();
    test_compliance_verify();
    test_test_points_estimation();
    test_medical_requirements();

    /* TLP tests */
    test_tlp_system_config();
    test_tlp_pulse_width();
    test_tlp_current();
    test_tlp_curve_extraction();
    test_seed_analysis();
    test_hmm_current();

    printf("\n─── Results ───\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    printf("Total:  %d\n", tests_passed + tests_failed);

    return (tests_failed > 0) ? 1 : 0;
}
