/**
 * @file esd_tlp.c
 * @brief Transmission Line Pulse (TLP) measurement, analysis, and SEED methodology
 *
 * Implements TLP I-V curve extraction, snapback detection,
 * figure-of-merit calculation, and System-Efficient ESD Design
 * (SEED) current-sharing analysis.
 *
 * Knowledge coverage: L1-L8
 *   L5: TLP parameter extraction algorithm
 *   L8: SEED methodology, VFTLP, HMM
 */

#include "esd_tlp.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Speed of light [m/s] */
#define SPEED_OF_LIGHT 2.99792458e8

/* ─── TLP System Configuration ─────────────────────────────────── */

/**
 * @brief Standard TLP system parameters.
 *
 * L1 Definition: A commercial TLP system (e.g., Barth 4002)
 * typically uses:
 *   - 50 Ω characteristic impedance (RG-58 or similar coax)
 *   - 100 ns pulse width (most common standard)
 *   - 200 ps rise time (mercury-wetted reed relay switch)
 *   - Up to 4 kV charging voltage
 *
 * The 100 ns pulse width was chosen as a compromise:
 *   - Long enough to allow the DUT to reach quasi-steady state
 *   - Short enough to minimize thermal effects
 *   - Similar energy to a real HBM ESD event
 */
esd_tlp_system_t esd_tlp_system_standard(void)
{
    esd_tlp_system_t sys;
    memset(&sys, 0, sizeof(sys));
    sys.type            = TLP_SYSTEM_50OHM;
    sys.z0_ohm          = 50.0;
    sys.pulse_width_ns  = 100.0;
    sys.rise_time_ns    = 0.2;      /* 200 ps */
    sys.max_voltage_kv  = 4.0;
    sys.cable_length_m  = 10.0;
    sys.v_prop_factor   = 0.66;     /* RG-58 velocity factor */
    return sys;
}

/**
 * @brief VFTLP (Very Fast TLP) system parameters.
 *
 * L8 Advanced Topic: VFTLP uses much shorter pulses (1-10 ns)
 * with sub-nanosecond rise times to characterize:
 *
 *   1. CDM-like failure modes:
 *      The Charged Device Model involves discharge of the
 *      device's own capacitance through a very low impedance
 *      path, producing <1 ns rise times and ~1 ns pulse widths.
 *
 *   2. Turn-on time of protection devices:
 *      Standard TLP (100 ns) cannot resolve whether a
 *      protection device turns on in 200 ps or 2 ns.
 *      VFTLP with 50 ps rise time can distinguish this.
 *
 *   3. Transient voltage overshoot:
 *      Before a TVS diode fully turns on, the voltage can
 *      overshoot well above the DC clamping voltage. VFTLP
 *      captures this transient behavior.
 */
esd_tlp_system_t esd_tlp_system_vftlp(void)
{
    esd_tlp_system_t sys;
    memset(&sys, 0, sizeof(sys));
    sys.type            = TLP_SYSTEM_VFTLP;
    sys.z0_ohm          = 50.0;
    sys.pulse_width_ns  = 5.0;
    sys.rise_time_ns    = 0.05;     /* 50 ps */
    sys.max_voltage_kv  = 2.0;
    sys.cable_length_m  = 0.5;
    sys.v_prop_factor   = 0.70;
    return sys;
}

/**
 * @brief Compute TLP pulse width from transmission line length.
 *
 * L4 Theorem: A charged transmission line produces a rectangular
 * pulse whose width equals the round-trip propagation delay:
 *
 *   τ_pulse = 2 × l_cable / v_propagation
 *
 * where:
 *   v_propagation = v_prop_factor × c  (velocity in the cable)
 *   c = 2.998 × 10⁸ m/s (speed of light in vacuum)
 *
 * For RG-58 coaxial cable (v_prop = 0.66c):
 *   10 m cable → τ = 2×10/(0.66×3e8) = 101 ns
 *
 * This is the fundamental TLP principle: the cable is both
 * the energy storage element and the pulse-forming network.
 */
double esd_tlp_pulse_width_from_length(double cable_length_m,
                                        double v_prop_factor)
{
    if (cable_length_m <= 0.0 || v_prop_factor <= 0.0) {
        return 0.0;
    }

    double v_prop = v_prop_factor * SPEED_OF_LIGHT;  /* m/s */
    double round_trip_time = 2.0 * cable_length_m / v_prop;  /* s */

    return round_trip_time * 1e9;  /* s → ns */
}

/**
 * @brief Required cable length for a desired pulse width.
 *
 * Inverse of the pulse width formula:
 *
 *   l_cable = τ_pulse × v_propagation / 2
 */
double esd_tlp_cable_length_for_width(double pulse_width_ns,
                                       double v_prop_factor)
{
    if (pulse_width_ns <= 0.0 || v_prop_factor <= 0.0) {
        return 0.0;
    }

    double v_prop = v_prop_factor * SPEED_OF_LIGHT;
    double length = pulse_width_ns * 1e-9 * v_prop / 2.0;

    return length;
}

/**
 * @brief Expected DUT current from TLP.
 *
 * L4 Theorem: For a TLP system with characteristic impedance Z₀:
 *
 * Matched case (R_DUT = Z₀):
 *   V_incident = V_charge / 2
 *   I_DUT = V_charge / (2 × Z₀)
 *
 * Mismatched case (R_DUT ≠ Z₀):
 *   Reflection coefficient: Γ = (R_DUT - Z₀) / (R_DUT + Z₀)
 *   V_DUT = V_incident × (1 + Γ)
 *   I_DUT = V_charge / (Z₀ + R_DUT)
 *
 * For ESD protection devices (typically low R_on in breakdown):
 *   R_DUT ≈ 1-5 Ω → Γ ≈ (1-50)/(1+50) ≈ -0.96
 *   → Most voltage is reflected, current is enhanced.
 */
double esd_tlp_expected_current(double v_charge_kv,
                                 double r_dut_ohm,
                                 double z0_ohm)
{
    if (z0_ohm <= 0.0) return 0.0;

    double v_charge = v_charge_kv * 1000.0;  /* kV → V */

    /* Matched source: V_incident = V_charge / 2.
     * Current: I_DUT = V_charge / (Z₀ + R_DUT) */
    double i_dut = v_charge / (z0_ohm + r_dut_ohm);

    return i_dut;
}

/**
 * @brief Energy delivered by a single TLP pulse.
 *
 * L4 Theorem:
 *
 *   E = V_DUT × I_DUT × τ_pulse
 *
 * For a 100 ns TLP pulse at 30 A, 15 V:
 *   E = 15 × 30 × 100e-9 = 45 µJ
 *
 * Compare with real HBM ESD:
 *   HBM 8kV: E = ½CV² = ½×100e-12×8000² = 3.2 mJ
 *
 * The TLP pulse delivers ~70× less energy, which is why
 * TLP can be stepped up incrementally to find the failure
 * threshold without catastrophic destruction.
 */
double esd_tlp_pulse_energy(double v_dut_v,
                              double i_dut_a,
                              double pulse_width_ns)
{
    return v_dut_v * i_dut_a * pulse_width_ns * 1e-3;  /* W·ns → µJ */
}

/* ─── TLP I-V Curve Analysis ─────────────────────────────────── */

/**
 * @brief Add a measurement point to a TLP curve.
 */
int esd_tlp_curve_add_point(esd_tlp_curve_t *curve,
                             const esd_tlp_point_t *point)
{
    if (!curve || !point) return -1;

    size_t new_count = curve->num_points + 1;
    esd_tlp_point_t *new_points = (esd_tlp_point_t *)realloc(
        curve->points, new_count * sizeof(esd_tlp_point_t));
    if (!new_points) return -1;

    curve->points = new_points;
    curve->points[curve->num_points] = *point;
    curve->num_points = new_count;

    return 0;
}

/**
 * @brief Extract TLP I-V parameters.
 *
 * L5 Algorithm: Automated extraction of standard TLP parameters
 * from measurement data.
 *
 * The algorithm processes the TLP I-V data to find:
 *
 *   1. Trigger point (V_t1, I_t1):
 *      First point where |I| > I_leakage × 10
 *      Marks the onset of avalanche/snapback.
 *
 *   2. Holding point (V_h, I_h):
 *      After snapback, the minimum |V| in the post-trigger region.
 *      Only meaningful if device exhibits snapback.
 *
 *   3. On-resistance (R_on):
 *      Slope ΔV/ΔI in the linear on-state region.
 *      Computed via least-squares linear regression over
 *      the post-trigger data points.
 *
 *   4. Failure point (I_t2, V_t2):
 *      First point where leakage increases by >10× over
 *      the pre-stress baseline, indicating thermal damage.
 *
 *   5. Leakage evolution:
 *      Pre-pulse and post-pulse leakage at each stress level
 *      reveals progressive degradation before hard failure.
 */
int esd_tlp_extract_params(esd_tlp_curve_t *curve)
{
    if (!curve || curve->num_points < 3) return -1;

    size_t n = curve->num_points;
    esd_tlp_point_t *pts = curve->points;

    /* Find the trigger point (first significant conduction) */
    /* I_leakage baseline from first few points */
    double i_baseline = 0.0;
    size_t baseline_count = (n > 5) ? 3 : 1;
    for (size_t i = 0; i < baseline_count && i < n; i++) {
        i_baseline += fabs(pts[i].i_dut_a);
    }
    i_baseline /= (double)baseline_count;
    if (i_baseline < 1e-9) i_baseline = 1e-6;

    /* Trigger: I > 10× baseline */
    size_t trig_idx = 0;
    for (size_t i = 0; i < n; i++) {
        if (fabs(pts[i].i_dut_a) > 10.0 * i_baseline) {
            trig_idx = i;
            break;
        }
    }

    if (trig_idx < n) {
        curve->v_t1_v = fabs(pts[trig_idx].v_dut_v);
        curve->i_t1_a = fabs(pts[trig_idx].i_dut_a);
    }

    /* Detect snapback: voltage drops after trigger while current rises */
    curve->has_snapback = 0;
    curve->v_h_v = curve->v_t1_v;
    curve->i_h_a = curve->i_t1_a;

    for (size_t i = trig_idx; i < n; i++) {
        if (i > trig_idx &&
            fabs(pts[i].v_dut_v) < fabs(pts[i-1].v_dut_v) &&
            fabs(pts[i].i_dut_a) > fabs(pts[i-1].i_dut_a)) {
            curve->has_snapback = 1;
            curve->v_h_v = fabs(pts[i].v_dut_v);
            curve->i_h_a = fabs(pts[i].i_dut_a);
            break;
        }
    }

    /* Compute R_on via linear regression in on-state region */
    /* Use points from trigger to failure (or to end) */
    double sum_x = 0.0, sum_y = 0.0, sum_xy = 0.0, sum_xx = 0.0;
    size_t on_count = 0;

    for (size_t i = trig_idx; i < n; i++) {
        double x = fabs(pts[i].i_dut_a);   /* Current */
        double y = fabs(pts[i].v_dut_v);   /* Voltage */
        sum_x  += x;
        sum_y  += y;
        sum_xy += x * y;
        sum_xx += x * x;
        on_count++;
    }

    if (on_count >= 2 && sum_xx * on_count > sum_x * sum_x) {
        double slope = (on_count * sum_xy - sum_x * sum_y) /
                       (on_count * sum_xx - sum_x * sum_x);
        curve->r_on_ohm = slope;
    } else {
        /* Fallback: simple ΔV/ΔI between first and last on-state points */
        double di = fabs(pts[n-1].i_dut_a) - fabs(pts[trig_idx].i_dut_a);
        double dv = fabs(pts[n-1].v_dut_v) - fabs(pts[trig_idx].v_dut_v);
        curve->r_on_ohm = (fabs(di) > 1e-9) ? dv / di : 1.0;
    }

    /* Find failure point: post-pulse leakage > 10× pre-pulse leakage */
    /* First establish baseline pre-leakage from non-failed points */
    double baseline_leakage = 0.0;
    size_t leak_baseline_count = 0;
    for (size_t i = 0; i < n && leak_baseline_count < 3; i++) {
        if (pts[i].leakage_pre_na > 0.0) {
            baseline_leakage += pts[i].leakage_pre_na;
            leak_baseline_count++;
        }
    }
    if (leak_baseline_count > 0) {
        baseline_leakage /= (double)leak_baseline_count;
    } else {
        baseline_leakage = 1.0;  /* 1 nA default */
    }

    curve->i_t2_a = 0.0;
    curve->v_t2_v = 0.0;
    for (size_t i = trig_idx; i < n; i++) {
        if (pts[i].leakage_post_na > 10.0 * baseline_leakage &&
            pts[i].leakage_post_na > 100.0) {  /* >100 nA */
            curve->i_t2_a = fabs(pts[i].i_dut_a);
            curve->v_t2_v = fabs(pts[i].v_dut_v);
            curve->v_leakage_at_fail_v = pts[i].v_dut_v;
            break;
        }
    }

    /* If no clear failure, use last point */
    if (curve->i_t2_a <= 0.0 && n > 0) {
        curve->i_t2_a = fabs(pts[n-1].i_dut_a);
        curve->v_t2_v = fabs(pts[n-1].v_dut_v);
    }

    return 0;
}

/**
 * @brief Detect snapback in TLP I-V curve.
 *
 * L2 Core Concept: Snapback behavior is characteristic of
 * certain ESD protection structures (e.g., grounded-gate NMOS,
 * SCR-based devices).
 *
 * In a snapback device:
 *   - Below V_t1: high impedance (off state)
 *   - At V_t1: avalanche breakdown begins
 *   - After triggering: voltage drops to V_h < V_t1 (negative resistance)
 *   - Current increases while voltage decreases → S-shaped I-V curve
 *
 * Snapback is beneficial for ESD protection because:
 *   - Lower holding voltage → lower power dissipation
 *   - Once triggered, stays on until current drops below holding current
 *
 * But snapback can cause latch-up issues in some circuits.
 */
int esd_tlp_detect_snapback(const esd_tlp_curve_t *curve)
{
    if (!curve || curve->num_points < 3) return 0;

    esd_tlp_point_t *pts = curve->points;
    size_t n = curve->num_points;

    /* Look for negative resistance: consecutive points where
     * current increases but voltage decreases */
    for (size_t i = 1; i < n; i++) {
        double di = fabs(pts[i].i_dut_a) - fabs(pts[i-1].i_dut_a);
        double dv = fabs(pts[i].v_dut_v) - fabs(pts[i-1].v_dut_v);

        if (di > 0.01 && dv < -0.1) {
            /* Current up, voltage down → negative resistance */
            return 1;
        }
    }

    return 0;
}

/**
 * @brief TLP Figure of Merit (FOM).
 *
 * L5 Algorithm: The FOM combines the three most important
 * protection device metrics into a single number:
 *
 *   FOM = I_t2 / (C_j × V_h)
 *
 * Units: [A / (pF × V)] = [A/(pF·V)]
 *
 * Higher FOM = better overall protection device:
 *   - High I_t2: survives higher ESD current
 *   - Low C_j: less signal degradation
 *   - Low V_h: clamps to lower voltage during ESD
 *
 * Typical FOM values:
 *   - Basic TVS: 0.5-2 A/(pF·V)
 *   - Advanced TVS: 5-15 A/(pF·V)
 *   - SCR-based: 20-50 A/(pF·V)
 *
 * The FOM highlights the fundamental trade-off in ESD protection:
 * device area ↑ → I_t2 ↑ (more current handling)
 * device area ↑ → C_j ↑ (more signal loading)
 *
 * FOM captures whether a device achieves good current handling
 * without excessive capacitance.
 */
double esd_tlp_figure_of_merit(const esd_tlp_curve_t *curve,
                                double c_j_pf)
{
    if (!curve || c_j_pf <= 0.0) return 0.0;

    double v_use = curve->has_snapback ? curve->v_h_v : curve->v_t1_v;
    if (v_use <= 0.0) v_use = 1.0;

    return curve->i_t2_a / (c_j_pf * v_use);
}

/* ─── SEED Analysis ───────────────────────────────────────────── */

/**
 * @brief System-Efficient ESD Design (SEED) simulation.
 *
 * L8 Advanced Topic: SEED is a co-design methodology developed
 * by the Industry Council on ESD Target Levels (White Paper 3,
 * 2010-2016).
 *
 * Key insight: System-level ESD current divides between parallel
 * paths according to their impedances:
 *
 *   I_total = I_onchip + I_offchip
 *
 * The current division is determined by the relative impedances:
 *
 *   I_onchip / I_offchip = Z_offchip / Z_onchip
 *
 * where:
 *   Z_onchip  = R_onchip_clamp + jωL_pcb
 *   Z_offchip = R_offchip_clamp + jωL_offchip
 *
 * SEED optimizes the PCB impedance between the TVS and the IC
 * to ensure both protection elements operate within their
 * safe operating areas (SOA).
 *
 * For the simplified DC model (valid for ~100 ns TLP-like pulses
 * where inductive effects are small):
 *
 *   I_onchip  = I_total × R_offchip / (R_onchip + R_offchip + R_trace)
 *   I_offchip = I_total - I_onchip
 *   V_node = V_onchip + I_onchip × R_trace
 */
int esd_seed_analyze(const esd_seed_params_t *seed,
                      esd_seed_result_t *result)
{
    if (!seed || !result) return -1;

    memset(result, 0, sizeof(*result));

    double r_onchip  = seed->r_onchip_clamp_ohm;
    double r_offchip = seed->r_offchip_tvs_ohm;
    double r_trace   = seed->r_pcb_trace_ohm;
    double i_total   = seed->i_esd_total_a;

    if (r_onchip <= 0.0 && r_offchip <= 0.0) {
        /* Degenerate case: equal split */
        result->i_onchip_a  = i_total / 2.0;
        result->i_offchip_a = i_total / 2.0;
    } else {
        /* Current divider: off-chip TVS takes path of least impedance */
        double g_onchip  = (r_onchip > 0.0) ? 1.0 / r_onchip : 0.0;
        double g_offchip = (r_offchip > 0.0) ? 1.0 / r_offchip : 0.0;
        double g_total   = g_onchip + g_offchip;

        if (g_total > 0.0) {
            result->i_onchip_a  = i_total * g_onchip / g_total;
            result->i_offchip_a = i_total * g_offchip / g_total;
        } else {
            result->i_onchip_a  = 0.0;
            result->i_offchip_a = i_total;
        }
    }

    /* Node voltage (protected IC input pin) */
    result->v_node_v = seed->v_onchip_clamp_v +
                        result->i_onchip_a * r_trace;

    /* Safety margins */
    if (seed->i_onchip_max_a > 0.0) {
        result->onchip_margin_pct =
            (1.0 - result->i_onchip_a / seed->i_onchip_max_a) * 100.0;
        result->onchip_safe = (result->i_onchip_a <= seed->i_onchip_max_a) ? 1 : 0;
    } else {
        result->onchip_margin_pct = -100.0;
        result->onchip_safe = 0;
    }

    if (seed->i_offchip_max_a > 0.0) {
        result->offchip_margin_pct =
            (1.0 - result->i_offchip_a / seed->i_offchip_max_a) * 100.0;
        result->offchip_safe = (result->i_offchip_a <= seed->i_offchip_max_a) ? 1 : 0;
    } else {
        result->offchip_margin_pct = -100.0;
        result->offchip_safe = 0;
    }

    result->overall_safe = (result->onchip_safe && result->offchip_safe) ? 1 : 0;

    return 0;
}

/**
 * @brief Optimal SEED impedance.
 *
 * L8: The optimal series impedance balances the current sharing
 * between on-chip and off-chip protection.
 *
 *   R_opt ≈ (V_offchip_clamp - V_onchip_clamp) / I_onchip_target
 *
 * If the off-chip TVS clamps at a lower voltage than the on-chip
 * clamp, the series impedance should be chosen to ensure the
 * on-chip clamp sees a safe current level.
 */
double esd_seed_optimal_impedance(double v_clamp_diff, double i_esd_total)
{
    if (i_esd_total <= 0.0) return 10.0;

    /* Simple heuristic: series R drops voltage to limit on-chip current */
    double r_opt = fabs(v_clamp_diff) / (i_esd_total * 0.3);

    /* Practical bounds */
    if (r_opt < 0.5) r_opt = 0.5;
    if (r_opt > 50.0) r_opt = 50.0;

    return r_opt;
}

/* ─── HMM (Human Metal Model) ────────────────────────────────── */

/**
 * @brief HMM equivalent current.
 *
 * L8 Advanced Topic: The Human Metal Model (HMM) is a hybrid
 * test method standardized in ANSI/ESD SP5.6-2010.
 *
 * HMM uses a standard IEC 61000-4-2 ESD gun, but the discharge
 * is applied to a 50 Ω environment (instead of the standard
 * 2 Ω calibration target). This allows correlation between
 * system-level IEC testing and device-level TLP measurements:
 *
 *   I_HMM = V_gun / (R_gun + Z₀) = V_gun / (330 + 50) ≈ V_gun / 380
 *
 * For V_gun = 8 kV: I_HMM ≈ 21 A into 50 Ω
 *
 * Compare with standard IEC into 2 Ω target:
 *   I_standard = 8 kV / (330 + 2) ≈ 24 A (but with overshoot ~30A)
 *
 * The HMM bridges the gap between TLP (50 Ω, rectangular pulse)
 * and IEC gun (330/150 pF, complex ns pulse), enabling better
 * correlation between device-level characterization and
 * system-level ESD testing.
 */
double esd_hmm_equivalent_current(double v_gun_kv)
{
    double v_volts = v_gun_kv * 1000.0;
    return v_volts / (330.0 + 50.0);  /* IEC gun into 50 Ω */
}

/* ─── Memory Management ─────────────────────────────────────── */

void esd_tlp_curve_free(esd_tlp_curve_t *curve)
{
    if (curve) {
        free(curve->points);
        free(curve->leakage_pre_na);
        free(curve->leakage_post_na);
        memset(curve, 0, sizeof(*curve));
    }
}
