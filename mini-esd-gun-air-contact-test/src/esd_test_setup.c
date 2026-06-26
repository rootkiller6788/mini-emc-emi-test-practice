/**
 * @file esd_test_setup.c
 * @brief IEC 61000-4-2 ESD test setup configuration and management
 *
 * Implements test geometry validation, test plan generation,
 * discharge recording, compliance statistics, and ESD coupling
 * analysis.
 *
 * Knowledge coverage: L1-L7
 *   L6: Full compliance test execution
 *   L7: Automotive/medical test setup requirements
 */

#include "esd_test_setup.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_SQRT2
#define M_SQRT2 1.41421356237309504880
#endif

/* ─── IEC 61000-4-2 Standard Test Geometry ────────────────────────
 *
 * L1 Definition: The standard table-top ESD test setup per
 * IEC 61000-4-2:2008 Section 7.1.1:
 *
 * Component | Dimension | Tolerance
 * ──────────────────────────────────
 * GRP width  | ≥ 0.8 m   | —
 * GRP length | ≥ 1.6 m   | —
 * HCP size   | 1.6×0.8 m | ±0.01 m
 * HCP height | 0.8 m     | ±0.01 m
 * VCP size   | 0.5×0.5 m | ±0.01 m
 * VCP gap    | 0.1 m     | ±0.01 m
 * Insulator  | 0.5 mm    | ±0.05 mm
 * Bleed R    | 470 kΩ    | ±10%
 */

/**
 * @brief Standard test geometry.
 *
 * Returns the nominal dimensions as specified in IEC 61000-4-2.
 * These are the dimensions that a compliant test lab must provide.
 */
esd_test_geometry_t esd_test_geometry_standard(void)
{
    esd_test_geometry_t g;
    g.grp_width_m           = 0.8;
    g.grp_length_m          = 1.6;
    g.hcp_width_m           = 1.6;
    g.hcp_length_m          = 0.8;
    g.hcp_height_m          = 0.8;
    g.vcp_width_m           = 0.5;
    g.vcp_height_m          = 0.5;
    g.vcp_distance_m        = 0.1;
    g.insulator_thickness_mm = 0.5;
    g.bleed_resistor_kohm   = 470.0;
    return g;
}

/**
 * @brief Validate geometry against IEC 61000-4-2 requirements.
 *
 * L2 Core Concept: Proper test geometry is essential for
 * reproducible ESD testing. The dimensions affect:
 *   - Coupling plane capacitance to GRP
 *   - Electromagnetic field distribution
 *   - Current return path impedance
 *
 * A non-compliant setup can produce significantly different
 * ESD stress on the EUT, leading to false pass/fail results.
 */
int esd_test_geometry_is_valid(const esd_test_geometry_t *geom)
{
    if (!geom) return 0;

    /* GRP: minimum 0.8 m × 1.6 m */
    if (geom->grp_width_m < 0.78 || geom->grp_length_m < 1.58) {
        return 0;
    }

    /* HCP height: 0.8 m ± 0.01 m */
    if (fabs(geom->hcp_height_m - 0.8) > 0.011) {
        return 0;
    }

    /* Insulator: 0.5 mm ± 0.05 mm */
    if (fabs(geom->insulator_thickness_mm - 0.5) > 0.06) {
        return 0;
    }

    /* Bleed resistor: 470 kΩ ± 10% */
    if (fabs(geom->bleed_resistor_kohm - 470.0) > 47.0) {
        return 0;
    }

    /* VCP distance: 0.1 m ± 0.01 m */
    if (fabs(geom->vcp_distance_m - 0.1) > 0.011) {
        return 0;
    }

    return 1;
}

/**
 * @brief Create a standard compliance test plan.
 *
 * L6 Canonical Problem: A standard IEC 61000-4-2 compliance test
 * requires systematic testing of:
 *
 *   1. Direct contact discharge to EUT conductive surfaces
 *      (at least 10 discharges per point, both polarities)
 *   2. Indirect contact discharge to HCP
 *      (at edges and center, 10 discharges per location)
 *   3. Indirect contact discharge to VCP
 *      (center of VCP, 10 discharges)
 *   4. Air discharge to insulating surfaces
 *      (approach at ~1 mm from surface)
 *
 * Test levels: Progressively increase from Level 1 to target level.
 */
int esd_test_plan_standard(esd_discharge_type_t discharge_type,
                            esd_test_level_t max_level,
                            esd_test_plan_t *plan)
{
    if (!plan) return -1;
    if (max_level < ESD_LEVEL_1 || max_level > ESD_LEVEL_4) {
        return -1;
    }

    memset(plan, 0, sizeof(*plan));
    plan->geometry = esd_test_geometry_standard();
    plan->discharge_type = discharge_type;
    plan->discharges_per_point = 10;
    plan->interval_seconds = 1.0;
    plan->test_both_polarities = 1;
    plan->eut_mode = EUT_MODE_NORMAL;
    plan->ambient_temp_c = 23.0;
    plan->ambient_humidity_pct = 40.0;
    plan->atmospheric_pressure_hpa = 1013.25;

    /* Set test levels from Level 1 up to max_level */
    plan->num_levels = (size_t)(max_level + 1);
    for (size_t i = 0; i < plan->num_levels; i++) {
        plan->levels[i] = (esd_test_level_t)i;
    }

    /* Standard test points */
    plan->num_test_points = 0;
    plan->test_points = NULL;

    /* Point 1: EUT front center (direct) */
    esd_test_point_t p1;
    memset(&p1, 0, sizeof(p1));
    p1.point_id = 1;
    p1.type = ESD_POINT_EUT_DIRECT;
    p1.x_m = 0.0;
    p1.y_m = 0.0;
    p1.z_m = 0.1;
    snprintf(p1.description, sizeof(p1.description), "EUT front face center");
    esd_test_plan_add_point(plan, &p1);

    /* Point 2: EUT top-left corner (direct) */
    esd_test_point_t p2;
    memset(&p2, 0, sizeof(p2));
    p2.point_id = 2;
    p2.type = ESD_POINT_EUT_DIRECT;
    p2.x_m = -0.15;
    p2.y_m = 0.15;
    p2.z_m = 0.1;
    snprintf(p2.description, sizeof(p2.description), "EUT top-left corner");
    esd_test_plan_add_point(plan, &p2);

    /* Point 3: EUT top-right corner (direct) */
    esd_test_point_t p3;
    memset(&p3, 0, sizeof(p3));
    p3.point_id = 3;
    p3.type = ESD_POINT_EUT_DIRECT;
    p3.x_m = 0.15;
    p3.y_m = 0.15;
    p3.z_m = 0.1;
    snprintf(p3.description, sizeof(p3.description), "EUT top-right corner");
    esd_test_plan_add_point(plan, &p3);

    /* Point 4: HCP front edge center (indirect) */
    esd_test_point_t p4;
    memset(&p4, 0, sizeof(p4));
    p4.point_id = 4;
    p4.type = ESD_POINT_HCP_INDIRECT;
    p4.x_m = 0.0;
    p4.y_m = -0.4;
    p4.z_m = 0.0;
    snprintf(p4.description, sizeof(p4.description), "HCP front edge center");
    esd_test_plan_add_point(plan, &p4);

    /* Point 5: HCP left edge center (indirect) */
    esd_test_point_t p5;
    memset(&p5, 0, sizeof(p5));
    p5.point_id = 5;
    p5.type = ESD_POINT_HCP_INDIRECT;
    p5.x_m = -0.8;
    p5.y_m = 0.0;
    p5.z_m = 0.0;
    snprintf(p5.description, sizeof(p5.description), "HCP left edge center");
    esd_test_plan_add_point(plan, &p5);

    /* Point 6: VCP center (indirect) */
    esd_test_point_t p6;
    memset(&p6, 0, sizeof(p6));
    p6.point_id = 6;
    p6.type = ESD_POINT_VCP_INDIRECT;
    p6.x_m = 0.0;
    p6.y_m = 0.25;
    p6.z_m = 0.0;
    snprintf(p6.description, sizeof(p6.description), "VCP center");
    esd_test_plan_add_point(plan, &p6);

    return 0;
}

/**
 * @brief Total number of discharges in a test plan.
 *
 * L5 Algorithm:
 *
 *   N_total = N_points × N_levels × N_discharges × P_factor
 *
 * where P_factor = 2 if both polarities are tested, else 1.
 *
 * For a typical Level 4 contact discharge test:
 *   N_total = 6 × 4 × 10 × 2 = 480 discharges
 */
size_t esd_test_plan_total_discharges(const esd_test_plan_t *plan)
{
    if (!plan) return 0;

    size_t polarity_factor = plan->test_both_polarities ? 2 : 1;
    return plan->num_test_points * plan->num_levels *
           (size_t)plan->discharges_per_point * polarity_factor;
}

/**
 * @brief Estimated test duration.
 *
 * duration_minutes = (total_discharges × interval + setup_overhead) / 60
 *
 * Adds 15% overhead for documentation, EUT reset between failures,
 * and environmental stabilization.
 */
double esd_test_plan_duration(const esd_test_plan_t *plan)
{
    if (!plan) return 0.0;

    size_t total = esd_test_plan_total_discharges(plan);
    double raw_seconds = (double)total * plan->interval_seconds;
    double overhead_seconds = 0.15 * raw_seconds + 600.0; /* +10 min setup */
    return (raw_seconds + overhead_seconds) / 60.0;
}

/**
 * @brief Add a test point to a plan.
 *
 * Dynamically expands the test_points array using realloc.
 * Each test point represents a unique physical location where
 * the ESD gun tip is applied.
 */
int esd_test_plan_add_point(esd_test_plan_t *plan,
                             const esd_test_point_t *point)
{
    if (!plan || !point) return -1;

    size_t new_count = plan->num_test_points + 1;
    esd_test_point_t *new_array =
        (esd_test_point_t *)realloc(plan->test_points,
                                     new_count * sizeof(esd_test_point_t));
    if (!new_array) return -1;

    plan->test_points = new_array;
    plan->test_points[plan->num_test_points] = *point;
    plan->num_test_points = new_count;

    return 0;
}

/**
 * @brief Initialize a test report.
 *
 * Pre-allocates the records array to avoid repeated reallocation
 * during testing.
 */
int esd_test_report_init(const esd_test_plan_t *plan,
                          esd_test_report_t *report,
                          const char *eut_id,
                          const char *engineer)
{
    if (!plan || !report) return -1;

    memset(report, 0, sizeof(*report));
    report->plan = *plan;

    /* Note: test_points pointer is copied; both plan and report
     * share the same array. Do not free plan's test_points until
     * report is finalized. */

    size_t total = esd_test_plan_total_discharges(plan);
    report->records = (esd_discharge_record_t *)calloc(total,
                         sizeof(esd_discharge_record_t));
    if (!report->records) return -1;

    report->num_records = 0;
    report->overall_pass = 0;

    if (eut_id) {
        snprintf(report->eut_identifier, sizeof(report->eut_identifier),
                 "%s", eut_id);
    }
    if (engineer) {
        snprintf(report->test_engineer, sizeof(report->test_engineer),
                 "%s", engineer);
    }
    snprintf(report->test_standard, sizeof(report->test_standard),
             "IEC 61000-4-2:2008");

    return 0;
}

/**
 * @brief Record a single ESD discharge event.
 *
 * Each discharge is recorded with the test point, voltage,
 * polarity, result (Pass A/B/C or Fail D), and notes.
 */
int esd_test_report_record(esd_test_report_t *report,
                            const esd_discharge_record_t *record)
{
    if (!report || !record) return -1;

    size_t total_allocated = esd_test_plan_total_discharges(&report->plan);
    if (report->num_records >= total_allocated) {
        return -1;  /* Report full */
    }

    report->records[report->num_records] = *record;
    report->num_records++;

    return 0;
}

/**
 * @brief Compute pass/fail statistics.
 *
 * L5 Algorithm: Statistical summary of ESD test results.
 *
 * Per IEC 61000-4-2, the performance criteria are:
 *   A: Normal performance within specification limits
 *   B: Temporary loss of function, self-recoverable
 *   C: Temporary loss requiring operator intervention or system reset
 *   D: Permanent damage (hardware failure, data loss unrecoverable)
 *
 * The test passes if ALL discharges achieve A, B, or C.
 * Any single D failure = overall FAIL.
 */
void esd_test_report_statistics(const esd_test_report_t *report,
                                 size_t *num_pass_a,
                                 size_t *num_pass_b,
                                 size_t *num_pass_c,
                                 size_t *num_fail)
{
    size_t a = 0, b = 0, c = 0, d = 0;

    if (report) {
        for (size_t i = 0; i < report->num_records; i++) {
            switch (report->records[i].result) {
            case ESD_RESULT_PASS_A: a++; break;
            case ESD_RESULT_PASS_B: b++; break;
            case ESD_RESULT_PASS_C: c++; break;
            case ESD_RESULT_FAIL_D: d++; break;
            }
        }
    }

    if (num_pass_a) *num_pass_a = a;
    if (num_pass_b) *num_pass_b = b;
    if (num_pass_c) *num_pass_c = c;
    if (num_fail)   *num_fail   = d;
}

/**
 * @brief Overall pass/fail determination.
 *
 * Per IEC 61000-4-2: The EUT FAILS if ANY discharge results in
 * performance criterion D (permanent damage or unrecoverable
 * loss of function).
 *
 * Note: Some product standards may also define failure if any
 * C results occur. This stricter criterion is product-specific.
 */
int esd_test_report_overall_pass(const esd_test_report_t *report)
{
    if (!report) return 0;
    if (report->num_records == 0) return 0;

    for (size_t i = 0; i < report->num_records; i++) {
        if (report->records[i].result == ESD_RESULT_FAIL_D) {
            return 0;  /* Any failure D = overall FAIL */
        }
    }
    return 1;  /* All discharges passed */
}

/* ─── Memory Management ─────────────────────────────────────── */

void esd_test_plan_free(esd_test_plan_t *plan)
{
    if (plan) {
        free(plan->test_points);
        plan->test_points = NULL;
        plan->num_test_points = 0;
    }
}

void esd_test_report_free(esd_test_report_t *report)
{
    if (report) {
        free(report->records);
        report->records = NULL;
        report->num_records = 0;
    }
}

/* ─── ESD Coupling Analysis ───────────────────────────────────────
 *
 * L2 Core Concept: ESD coupling is the mechanism by which an ESD
 * event at one location induces voltage/current disturbances at
 * another location. The two primary coupling mechanisms are:
 *
 *   1. Inductive coupling: V = M · dI/dt
 *      Fast ESD current changes (dI/dt ~ 30 A/ns) induce voltage
 *      in nearby loops through mutual inductance.
 *
 *   2. Capacitive coupling: I = C · dV/dt
 *      Fast voltage changes couple displacement current into
 *      adjacent conductors.
 *
 * These coupling mechanisms explain why indirect ESD to coupling
 * planes can cause failures even when the EUT is not directly
 * contacted.
 */

/**
 * @brief Induced voltage from mutual inductance.
 *
 * L4 Theorem: Faraday's law for mutual inductance:
 *
 *   V_induced = M · dI/dt
 *
 * For an IEC 61000-4-2 8 kV contact discharge:
 *   dI/dt ≈ 30 A / 0.8 ns = 37.5 A/ns
 *
 * With typical PCB-level mutual inductance M ≈ 1 nH:
 *   V_induced ≈ 1e-9 × 37.5e9 = 37.5 V
 *
 * This 37.5 V can be enough to upset or damage sensitive CMOS inputs.
 */
double esd_coupling_induced_voltage(double dI_dt_aps, double mutual_l_nh)
{
    /* dI/dt in [A/ns], M in [nH]
     * V = M[H] × dI/dt[A/s] = M[nH]×1e-9 × dI/dt[A/ns]×1e9
     *   = M × dI/dt  [V]  (the 1e-9 and 1e9 cancel)
     */
    return fabs(mutual_l_nh * dI_dt_aps);
}

/**
 * @brief Mutual inductance of parallel conductors.
 *
 * L3 Mathematical Structure:
 *
 * For two parallel round wires of length l, separated by distance d:
 *
 *   M = (μ₀ · l / 2π) × [ln(2l/d) - 1 + d/l]
 *
 * where μ₀ = 4π × 10⁻⁷ H/m (permeability of free space).
 *
 * This formula is valid for l >> d (long parallel conductors).
 * For the short traces on a PCB, corrections for length-to-
 * separation ratio are applied through the ln and d/l terms.
 *
 * @param length_m      Conductor length [m]
 * @param separation_m  Conductor separation [m]
 * @return              Mutual inductance [H]
 */
double esd_mutual_inductance_parallel(double length_m, double separation_m)
{
    if (length_m <= 0.0 || separation_m <= 0.0) {
        return 0.0;
    }

    double mu0 = 4.0 * M_PI * 1e-7;  /* H/m */
    double ratio = 2.0 * length_m / separation_m;

    /* Guard against log of negative/zero */
    if (ratio <= 0.0) return 0.0;

    double m = (mu0 * length_m / (2.0 * M_PI)) *
               (log(ratio) - 1.0 + separation_m / length_m);

    return m;
}

/**
 * @brief Protection attenuation calculation.
 *
 * L6 Canonical Problem: Determining whether a protection component
 * reduces the ESD voltage at the protected IC to a safe level.
 *
 * Simple voltage divider model:
 *
 *   V_residual = V_esd × R_clamp / (R_series + R_clamp)
 *
 * Where R_clamp ≈ V_clamp / I_esd_peak (dynamic resistance of TVS).
 *
 * Example: 8 kV ESD, 15 V TVS clamp, 10 Ω series R:
 *   If I_esd = 30 A, R_clamp ≈ 15/30 = 0.5 Ω
 *   V_residual ≈ 8000 × 0.5/10.5 ≈ 381 V  →  still dangerous!
 *
 * This shows why series resistance alone is insufficient;
 * a low-impedance shunt path (TVS to ground) is essential.
 */
double esd_protection_attenuation(double esd_voltage_kv,
                                   double clamp_voltage_v,
                                   double series_r_ohm)
{
    if (series_r_ohm <= 0.0) return esd_voltage_kv * 1000.0;

    /* Simplified model: residual voltage scaled by impedance ratio */
    double v_esd = esd_voltage_kv * 1000.0;
    double clamp_r = clamp_voltage_v / (esd_voltage_kv * 3.75); /* R ≈ Vc/Ip */

    if (clamp_r < 0.001) clamp_r = 0.001;

    double residual = v_esd * clamp_r / (series_r_ohm + clamp_r);

    return residual;
}
