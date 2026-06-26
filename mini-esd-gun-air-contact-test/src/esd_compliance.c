/**
 * @file esd_compliance.c
 * @brief Cross-standard ESD compliance verification
 *
 * Implements compliance checking across IEC 61000-4-2, ISO 10605,
 * ANSI/ESDA/JEDEC JS-001 (HBM), and related standards.
 * Includes HBM classification, cross-standard equivalence mapping,
 * and application-specific requirements (automotive, medical).
 *
 * Knowledge coverage: L1-L7
 *   L7: Automotive ESD (ISO 10605), Medical device ESD (IEC 60601-1-2)
 */

#include "esd_compliance.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ─── ESD Standard Database ───────────────────────────────────────
 *
 * L1 Definition: Each ESD standard defines a specific source
 * impedance (R, C) that models a particular ESD threat scenario.
 *
 * The differences between standards are critical because:
 *   - Different R/C produce different current waveforms
 *   - Different peak currents for the same voltage
 *   - Different energy delivered to the DUT
 *
 * Cross-standard comparison requires understanding these differences
 * and applying appropriate equivalence factors.
 */

static const esd_standard_params_t standard_db[] = {
    { ESD_STD_IEC_61000_4_2,  150.0, 330.0,  15.0,
      "IEC 61000-4-2:2008", "Generic EMC — system-level ESD immunity" },
    { ESD_STD_ISO_10605,      150.0, 330.0,  25.0,
      "ISO 10605:2008", "Road vehicles — ESD immunity" },
    { ESD_STD_ANSI_ESDA_JS001, 100.0, 1500.0,  8.0,
      "ANSI/ESDA/JEDEC JS-001-2017", "HBM — device-level sensitivity" },
    { ESD_STD_JEDEC_JESD22_A114, 100.0, 1500.0, 8.0,
      "JEDEC JESD22-A114F", "HBM — IC qualification" },
    { ESD_STD_IEC_61340_3_1,  150.0, 330.0,   0.0,
      "IEC 61340-3-1", "ESD control program requirements" },
    { ESD_STD_MIL_STD_883,    100.0, 1500.0,  8.0,
      "MIL-STD-883 Method 3015.7", "Military HBM classification" },
    { ESD_STD_AEC_Q100,       100.0, 1500.0,  8.0,
      "AEC-Q100-002", "Automotive IC HBM qualification" },
    { ESD_STD_GR_1089_CORE,   150.0, 330.0,  15.0,
      "GR-1089-CORE", "Telecom equipment ESD" }
};

static const size_t num_standards =
    sizeof(standard_db) / sizeof(standard_db[0]);

/**
 * @brief Get standard parameters.
 *
 * L1 Definition: Returns the defining parameters of an ESD standard:
 *   - Storage capacitance (Cs)
 *   - Discharge resistance (Rd)
 *   - Maximum test voltage
 *
 * These three parameters uniquely define the ESD source model
 * for that standard. Differences in these values mean that
 * passing one standard does not guarantee passing another.
 */
esd_standard_params_t esd_standard_get(esd_standard_id_t standard)
{
    for (size_t i = 0; i < num_standards; i++) {
        if (standard_db[i].standard == standard) {
            return standard_db[i];
        }
    }

    /* Return zero-filled struct for unknown standard */
    esd_standard_params_t unknown;
    memset(&unknown, 0, sizeof(unknown));
    unknown.standard = standard;
    return unknown;
}

/* ─── HBM Classification ──────────────────────────────────────── */

/**
 * @brief Classify HBM sensitivity level.
 *
 * L1 Definition: The ANSI/ESDA/JEDEC JS-001 standard defines
 * component sensitivity classes based on the HBM voltage at
 * which the device fails.
 *
 * The classification directly impacts:
 *   - Manufacturing handling requirements (ESD protected area class)
 *   - Packaging requirements (static-shielding bags)
 *   - Assembly line ESD control measures
 *
 * A Class 0 device (< 250 V) requires the strictest ESD controls,
 * while Class 3B devices (≥ 8000 V) are considered robust.
 */
esd_hbm_class_t esd_hbm_classify(double hbm_voltage_v)
{
    if (hbm_voltage_v < 250.0)  return HBM_CLASS_0;
    if (hbm_voltage_v < 500.0)  return HBM_CLASS_1A;
    if (hbm_voltage_v < 1000.0) return HBM_CLASS_1B;
    if (hbm_voltage_v < 2000.0) return HBM_CLASS_1C;
    if (hbm_voltage_v < 4000.0) return HBM_CLASS_2;
    if (hbm_voltage_v < 8000.0) return HBM_CLASS_3A;
    return HBM_CLASS_3B;
}

/**
 * @brief HBM voltage range for a given class.
 *
 * Returns the [min, max) voltage range for the classification.
 * 3B has no upper bound (returns INFINITY for v_max).
 */
void esd_hbm_class_range(esd_hbm_class_t hbm_class,
                          double *v_min, double *v_max)
{
    if (!v_min || !v_max) return;

    switch (hbm_class) {
    case HBM_CLASS_0:  *v_min = 0.0;    *v_max = 250.0;  break;
    case HBM_CLASS_1A: *v_min = 250.0;  *v_max = 500.0;  break;
    case HBM_CLASS_1B: *v_min = 500.0;  *v_max = 1000.0; break;
    case HBM_CLASS_1C: *v_min = 1000.0; *v_max = 2000.0; break;
    case HBM_CLASS_2:  *v_min = 2000.0; *v_max = 4000.0; break;
    case HBM_CLASS_3A: *v_min = 4000.0; *v_max = 8000.0; break;
    case HBM_CLASS_3B: *v_min = 8000.0; *v_max = INFINITY; break;
    default:           *v_min = 0.0;    *v_max = 0.0;    break;
    }
}

/* ─── Compliance Margin Analysis ─────────────────────────────── */

/**
 * @brief Compute ESD compliance margin.
 *
 * L4 Theorem: The compliance margin quantifies how much headroom
 * a design has above the required test level.
 *
 *   Margin_ratio = V_failure / V_required
 *   Margin_dB    = 20 × log10(Margin_ratio)
 *
 * A margin of 2× (6 dB) is typically considered adequate for
 * production. Safety-critical applications may require 3× or more.
 *
 * The margin accounts for:
 *   - Manufacturing variation in protection devices
 *   - Degradation over lifetime
 *   - Environmental factors (temperature, humidity)
 *   - Measurement uncertainty in ESD testing
 */
void esd_compliance_margin_compute(double failure_threshold_kv,
                                    double required_level_kv,
                                    double min_margin_ratio,
                                    esd_compliance_margin_t *margin)
{
    if (!margin) return;

    margin->required_level_kv = required_level_kv;
    margin->failure_threshold_kv = failure_threshold_kv;

    if (required_level_kv > 0.0) {
        margin->margin_ratio = failure_threshold_kv / required_level_kv;
        margin->margin_db = 20.0 * log10(margin->margin_ratio);
    } else {
        margin->margin_ratio = INFINITY;
        margin->margin_db = INFINITY;
    }

    margin->passes = (margin->margin_ratio >= min_margin_ratio) ? 1 : 0;
    margin->min_margin = min_margin_ratio;
}

/* ─── Cross-Standard Equivalence ──────────────────────────────── */

/**
 * @brief HBM to IEC equivalent.
 *
 * L2 Core Concept: Converting between HBM (device-level) and
 * IEC (system-level) ESD voltages is inherently approximate
 * because the source impedances differ fundamentally:
 *
 *   HBM: 100 pF, 1500 Ω → τ ≈ 150 ns, I_peak ≈ V/1500
 *   IEC: 150 pF, 330 Ω  → τ_rise ≈ 0.8 ns, I_peak ≈ V×3.75
 *
 * Peak current equivalence:
 *   HBM @ 1000V: I_peak ≈ 1000/1500 = 0.67 A
 *   IEC @ 2000V:  I_peak ≈ 2000×3.75/1000 ≈ 7.5 A
 *
 * So IEC delivers ~11× more peak current for the same voltage!
 *
 * Empirical rule (Paul, "Introduction to EMC", 2006):
 *   IEC voltage ≈ HBM_voltage / 4  (for equal peak current)
 *
 * But this doesn't account for:
 *   - Different waveform shapes (ns spike vs µs decay)
 *   - Different failure mechanisms (gate oxide vs thermal)
 *
 * Hence, these equivalences should be treated as rough
 * engineering guidelines, not precise conversions.
 */
double esd_hbm_to_iec_equivalent(double hbm_voltage_v)
{
    if (hbm_voltage_v <= 0.0) return 0.0;

    /* IEC 2kV contact produces ~7.5A peak */
    /* HBM needs ~8000V to produce ~5.3A peak (8000/1500) */
    /* Crude linear mapping based on peak current equality: */
    /* I_HBM = V_HBM / 1500, I_IEC ≈ V_IEC * 3.75 */
    /* Equal → V_IEC = V_HBM / (1500 * 3.75 / 1000) */
    /* V_IEC[kV] = V_HBM[V] / 5625 */

    return hbm_voltage_v / 5625.0;  /* V → kV */
}

/**
 * @brief IEC to HBM equivalent.
 *
 * Inverse of HBM-to-IEC mapping.
 */
double esd_iec_to_hbm_equivalent(double iec_kv)
{
    if (iec_kv <= 0.0) return 0.0;

    return iec_kv * 5625.0;  /* kV → V */
}

/**
 * @brief Cross-standard equivalence mapping.
 *
 * Provides approximate equivalences between IEC 61000-4-2,
 * HBM (JS-001), and ISO 10605.
 *
 * L7 Application: Product engineers need to understand that
 * passing HBM 2kV at device level does NOT mean passing
 * IEC 2kV at system level — the system-level test is much
 * more severe in terms of peak current (7.5A vs 1.3A).
 */
void esd_cross_standard_map(double iec_level_kv,
                             esd_cross_standard_map_t *mapping)
{
    if (!mapping) return;

    memset(mapping, 0, sizeof(*mapping));
    mapping->iec_61000_4_2_kv = iec_level_kv;
    mapping->hbm_voltage_v = esd_iec_to_hbm_equivalent(iec_level_kv);

    /* ISO 10605 uses same 150pF/330Ω network as IEC for basic test,
     * so the voltage equivalence is 1:1. However, ISO 10605 also
     * defines other discharge networks (330pF/330Ω, 150pF/2000Ω) */
    mapping->iso_10605_kv = iec_level_kv;

    /* The equivalence is approximate — HBM waveform is much slower */
    mapping->equivalent_valid = 1;

    snprintf(mapping->caveat, sizeof(mapping->caveat),
             "WARNING: HBM equiv based on peak current only. "
             "HBM ~150ns vs IEC ~1ns pulse. "
             "For rough comparison only, not qualification.");
}

/* ─── Automotive ESD (ISO 10605) ─────────────────────────────── */

/**
 * @brief ISO 10605 automotive ESD test parameters.
 *
 * L7 Application: Automotive electronics face unique ESD challenges:
 *
 *   1. Higher voltages (up to 25 kV) for air discharge
 *      → Occupants entering vehicle can generate very high potentials
 *
 *   2. Multiple discharge networks:
 *      a. 150 pF / 330 Ω — standard "human body" through fingertip
 *      b. 330 pF / 330 Ω — human body through metal tool/keys
 *      c. 150 pF / 2000 Ω — human body through clothing
 *
 *   3. Powered vs unpowered testing:
 *      → Some failures only occur when the ECU is operating
 *
 *   4. Connector pin testing:
 *      → ESD into exposed connector pins during assembly/maintenance
 *
 *   5. Painted surface testing:
 *      → Air discharge through paint to underlying metal
 */
int esd_auto_test_params(double voltage_kv,
                          int discharge_network,
                          esd_auto_test_params_t *params)
{
    if (!params) return -1;
    if (voltage_kv <= 0.0 || voltage_kv > 25.0) return -1;

    memset(params, 0, sizeof(*params));
    params->voltage_kv = voltage_kv;

    switch (discharge_network) {
    case 0:
        params->cs_pf = 150.0;
        params->rd_ohm = 330.0;
        break;
    case 1:
        params->cs_pf = 330.0;
        params->rd_ohm = 330.0;
        break;
    case 2:
        params->cs_pf = 150.0;
        params->rd_ohm = 2000.0;
        break;
    default:
        return -1;
    }

    /* Default: powered test, connector pins tested */
    params->powered_test = 1;
    params->esd_into_connector_pins = 1;
    params->esd_into_paint_surface = 0;
    params->air_discharge_only = (voltage_kv > 8.0) ? 1 : 0;

    return 0;
}

/* ─── IEC 61000-4-2 Compliance Verification ──────────────────── */

/**
 * @brief Verify test plan against IEC 61000-4-2 requirements.
 *
 * L6 Canonical Problem: Full compliance verification checklist.
 *
 * IEC 61000-4-2 requires:
 *   ✓ All severity levels up to target are tested
 *   ✓ At least 10 discharges per test point
 *   ✓ Both polarities applied
 *   ✓ Test geometry meets standard dimensions
 *   ✓ Environmental conditions: 15-35°C, 30-60% RH, 86-106 kPa
 *   ✓ ≥ 1 second between discharges
 *   ✓ Contact discharge applied to conductive surfaces
 *   ✓ Air discharge applied to insulating surfaces
 */
int esd_compliance_verify_plan(const esd_test_plan_t *plan,
                                esd_test_level_t target_level)
{
    if (!plan) return 0;

    /* Check: test geometry valid */
    if (!esd_test_geometry_is_valid(&plan->geometry)) {
        return 0;
    }

    /* Check: target level is in the plan */
    int target_found = 0;
    for (size_t i = 0; i < plan->num_levels; i++) {
        if (plan->levels[i] == target_level) {
            target_found = 1;
            break;
        }
    }
    if (!target_found) return 0;

    /* Check: at least 10 discharges per point */
    if (plan->discharges_per_point < 10) return 0;

    /* Check: both polarities */
    if (!plan->test_both_polarities) return 0;

    /* Check: interval ≥ 1s */
    if (plan->interval_seconds < 0.9) return 0;

    /* Check: environmental limits */
    if (plan->ambient_temp_c < 15.0 || plan->ambient_temp_c > 35.0) return 0;
    if (plan->ambient_humidity_pct < 30.0 || plan->ambient_humidity_pct > 60.0) return 0;
    if (plan->atmospheric_pressure_hpa < 860.0 || plan->atmospheric_pressure_hpa > 1060.0) return 0;

    /* Check: at least one test point */
    if (plan->num_test_points == 0) return 0;

    return 1;
}

/**
 * @brief Estimated number of test points for an EUT.
 *
 * L5 Algorithm: IEC 61000-4-2 recommends test points at:
 *   - All accessible conductive surfaces
 *   - Corners and edges
 *   - Seams and joints
 *   - User-accessible controls
 *
 * Empirical formula based on EUT surface area:
 *   N_points ≈ ceil(A_surface / 0.1 m²)
 *
 * where A_surface ≈ 2·(W·H + W·D + H·D) (rectangular approximation).
 */
int esd_compliance_num_test_points(double eut_width_m,
                                    double eut_height_m,
                                    double eut_depth_m)
{
    if (eut_width_m <= 0.0 || eut_height_m <= 0.0 || eut_depth_m <= 0.0) {
        return 0;
    }

    /* Surface area of rectangular prism */
    double area = 2.0 * (eut_width_m * eut_height_m +
                          eut_width_m * eut_depth_m +
                          eut_height_m * eut_depth_m);

    int n = (int)ceil(area / 0.1);
    if (n < 4) n = 4;     /* Minimum 4 test points */
    if (n > 50) n = 50;   /* Practical upper limit */

    return n;
}

/* ─── Medical Device ESD Requirements ────────────────────────── */

/**
 * @brief Medical device ESD immunity requirements.
 *
 * L7 Application: IEC 60601-1-2 (Medical electrical equipment —
 * Electromagnetic disturbances) specifies ESD immunity levels
 * for medical devices.
 *
 * Standard medical equipment:
 *   Contact: ±8 kV
 *   Air:     ±15 kV
 *
 * Life-support equipment (ventilators, infusion pumps, etc.):
 *   Contact: ±8 kV (same)
 *   Air:     ±15 kV (same)
 *   Additional: ±25 kV air discharge recommended
 *   Additional: Testing with patient-connected parts
 *   Additional: 100 discharges per point instead of 10
 *
 * The rationale for increased stringency is that ESD-induced
 * malfunction of life-support equipment could cause patient
 * injury or death.
 */
void esd_medical_requirements(int is_life_support,
                               double *contact_level_kv,
                               double *air_level_kv,
                               char *additional_reqs)
{
    if (contact_level_kv) *contact_level_kv = 8.0;
    if (air_level_kv)     *air_level_kv     = 15.0;

    if (additional_reqs) {
        if (is_life_support) {
            snprintf(additional_reqs, 256,
                     "LIFE-SUPPORT: Additional ±25 kV air discharge recommended. "
                     "Test with patient-connected parts. "
                     "100 discharges/point (vs standard 10). "
                     "Continuous monitoring for any degradation. "
                     "No performance degradation allowed (Criterion A only).");
        } else {
            snprintf(additional_reqs, 256,
                     "Standard medical: ±8 kV contact, ±15 kV air. "
                     "Criterion B acceptable. "
                     "Test per IEC 60601-1-2:2014.");
        }
    }
}

/* ─── Print ──────────────────────────────────────────────────── */

int esd_standard_print(const esd_standard_params_t *params,
                        char *buffer, size_t buf_size)
{
    if (!params || !buffer || buf_size == 0) return -1;

    snprintf(buffer, buf_size,
             "Standard: %s\n"
             "  Scope:      %s\n"
             "  Cs:         %.0f pF\n"
             "  Rd:         %.0f ohm\n"
             "  Max Voltage: %.1f kV\n",
             params->name, params->scope,
             params->cs_pf, params->rd_ohm,
             params->max_voltage_kv);

    return 0;
}
