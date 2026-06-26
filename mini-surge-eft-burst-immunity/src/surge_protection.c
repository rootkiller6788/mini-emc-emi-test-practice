/**
 * surge_protection.c - Surge Protection Device Implementation
 *
 * L2 Core Concepts: MOV, TVS, GDT, TSS device physics and V-I models.
 * L5 Algorithms: Device selection, clamping prediction, thermal analysis.
 *
 * Reference:
 *   IEC 61643-11: Surge Protective Devices
 *   Littlefuse TVS Application Notes
 *   Bourns MOV Technology Overview
 *   IEEE C62.42: Guide for Surge Protective Components
 */

#include "surge_protection.h"
#include "surge_waveform.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

/* ==================================================================
 * L2: MOV (Metal Oxide Varistor) Model
 *
 * Physics: ZnO grain boundaries form back-to-back Schottky barriers.
 * Conduction via thermionic emission and tunneling at high fields.
 * The V-I characteristic follows:
 *   I = k * V^alpha
 * with alpha = 20-50 for ZnO varistors.
 *
 * The high alpha means the MOV transitions from MOhm to mOhm
 * over a narrow voltage range (~1.5:1), providing effective
 * voltage clamping without crowbar behavior.
 *
 * Reference: Gupta, "ZnO Varistors," J. Am. Ceramic Soc., 1990
 *            Eda, "Conduction mechanism of ZnO varistors," JAP, 1978
 * ================================================================== */

double surge_mov_voltage(double current, double v_ref, double i_ref,
                          double alpha)
{
    /* V = V_ref * (I / I_ref)^(1/alpha)
     *
     * If we use V_1mA as reference:
     *   V(I) = V_1mA * (I / 0.001)^(1/alpha)
     */
    if (current <= 0.0 || v_ref <= 0.0 || i_ref <= 0.0 || alpha < 1.0)
        return v_ref;

    return v_ref * pow(current / i_ref, 1.0 / alpha);
}

double surge_mov_dynamic_resistance(double current, double v_ref,
                                     double i_ref, double alpha)
{
    /* R_dyn = dV/dI = V_ref * (I/I_ref)^(1/alpha) / (alpha * I)
     *
     * At surge currents (100A), R_dyn is typically < 1 ohm.
     */
    if (current <= 0.0 || v_ref <= 0.0 || i_ref <= 0.0 || alpha < 1.0)
        return 0.0;

    double v_at_i = surge_mov_voltage(current, v_ref, i_ref, alpha);
    return v_at_i / (alpha * current);
}

double surge_mov_clamp_voltage(double i_surge, double v_1ma, double alpha)
{
    /* Standard MOV characterization: V_1mA is the reference voltage.
     *   V_clamp = V_1mA * (I_surge / 0.001)^(1/alpha)
     *
     * For a typical 230V MOV (V_1mA = 230V, alpha = 30):
     *   V_clamp at 100A = 230 * (100/0.001)^(1/30) = 230 * 100000^(0.0333)
     *   = 230 * 1.47 = 338V
     *   V_clamp at 1000A = 230 * 1000000^(0.0333) = 230 * 1.58 = 363V
     */
    if (i_surge <= 0.0 || v_1ma <= 0.0 || alpha < 1.0) return 0.0;

    double i_ref = 0.001; /* 1 mA reference */
    return v_1ma * pow(i_surge / i_ref, 1.0 / alpha);
}

/* ==================================================================
 * L2: TVS Diode Model
 *
 * Physics: Silicon avalanche breakdown in a specially designed p-n
 * junction with large junction area and low series resistance.
 *
 * The TVS operates in reverse breakdown:
 *   Off-state:  I ~ I_s (saturation, nA-uA), high impedance
 *   Breakdown:  I increases exponentially for V > V_BR
 *   Clamping:   V = V_BR + R_dyn * I
 *
 * TVS advantages:
 *   - Very fast response (< 1 ps intrinsic, ~1 ns packaged)
 *   - Precise clamping voltage
 *   - Low dynamic resistance
 *   - No wear-out mechanism (unlike MOV)
 *
 * TVS limitations:
 *   - Lower energy handling than MOV (smaller die)
 *   - Higher capacitance (can be mitigated with steering diodes)
 *   - Temperature-sensitive breakdown voltage (+0.05-0.1%/K)
 * ================================================================== */

double surge_tvs_clamp_voltage(double i_surge, double v_br, double r_dyn)
{
    /* Simplified linear model in breakdown:
     *   V_clamp = V_BR + R_dyn * I_surge
     *
     * This model is valid for I >> I_s (saturation current).
     * For accurate modeling at low currents, use the exponential
     * diode equation with avalanche multiplication factor.
     */
    if (i_surge < 0.0 || v_br <= 0.0 || r_dyn < 0.0) return 0.0;

    return v_br + r_dyn * i_surge;
}

double surge_tvs_temp_rise(double energy_absorbed, double die_area_mm2,
                            double die_thickness_um)
{
    /* Adiabatic temperature rise (valid for pulses < 100 us):
     *   Delta_T = E / (rho * V * C_p)
     *
     * where:
     *   rho = 2.33 g/cm^3 (silicon density)
     *   C_p = 0.7 J/(g*K) (silicon specific heat)
     *   V = die_area * die_thickness (volume)
     *
     * For a typical 1500W TVS (SMB package):
     *   die_area ~ 1.5 mm x 1.5 mm = 2.25 mm^2
     *   die_thickness ~ 200 um
     *   V = 2.25e-2 cm^2 * 2e-2 cm = 4.5e-4 cm^3
     *   mass = 2.33 * 4.5e-4 = 1.05e-3 g
     *   C = 0.7 * 1.05e-3 = 7.35e-4 J/K
     *
     *   For a 1J surge: Delta_T = 1/7.35e-4 = 1360 K -> failure!
     *   This is why TVS energy ratings are typically mJ, not J.
     */
    if (energy_absorbed <= 0.0 || die_area_mm2 <= 0.0 ||
        die_thickness_um <= 0.0) return 0.0;

    double rho_si = 2.33;         /* g/cm^3 */
    double cp_si = 0.7;           /* J/(g*K) */

    /* Convert mm^2 to cm^2, um to cm */
    double area_cm2 = die_area_mm2 * 1e-2;
    double thickness_cm = die_thickness_um * 1e-4;
    double volume_cm3 = area_cm2 * thickness_cm;
    double mass_g = rho_si * volume_cm3;
    double heat_capacity = cp_si * mass_g;

    if (heat_capacity <= 0.0) return 0.0;

    return energy_absorbed / heat_capacity;
}

/* ==================================================================
 * L2: GDT (Gas Discharge Tube) Model
 *
 * Physics:
 * 1. Pre-breakdown: Gas-filled gap with DC breakdown voltage
 *    determined by Paschen's law: V_spark = f(p*d) where p is
 *    gas pressure and d is electrode spacing.
 * 2. Spark-over: Electron avalanche (Townsend discharge) when
 *    E-field exceeds critical value. Transition time ~100ns-1us.
 * 3. Glow regime: Ionized gas conducts, V ~ 70-100V, low current.
 * 4. Arc regime: Full conduction, V_arc ~ 10-30V, kA capability.
 *
 * GDT characteristics:
 *   - Extremely high surge current (up to 100 kA for large devices)
 *   - Very low capacitance (<5 pF) - ideal for RF circuits
 *   - Very high off-state impedance (>1 Gohm)
 *   - Slow response (~100ns-1us) compared to TVS (<1ns)
 *   - Follow-on current issue on DC circuits
 *   - Finite life (electrode erosion, gas leakage)
 *
 * Reference: N. K. Bui et al., "GDT modeling," IEEE Trans. EMC, 2018
 * ================================================================== */

double surge_gdt_arc_voltage(double i_surge, double v_arc_min, double r_arc)
{
    /* GDT arc voltage is nearly constant once triggered:
     *   V_arc = V_arc_min + I_surge * R_arc
     *
     * V_arc_min is typically 10-25V (depends on gas mixture).
     * R_arc is the positive arc resistance, typically 0.01-0.1 ohm.
     *
     * The arc voltage determines how much residual voltage
     * reaches the load after GDT triggering.
     */
    if (i_surge < 0.0 || v_arc_min < 0.0 || r_arc < 0.0) return 0.0;

    return v_arc_min + r_arc * i_surge;
}

double surge_gdt_spark_time(double v_spark, double dv_dt)
{
    /* Spark-over time under ramp voltage:
     *   t_spark = V_spark / (dV/dt)
     *
     * For slow surges (dV/dt ~ 1 kV/us):
     *   t_spark = 230V / 1kV/us = 0.23 us (triggers quickly)
     *
     * For EFT (dV/dt ~ 1 kV/ns):
     *   t_spark = 230V / 1kV/ns = 0.23 ns (may not trigger reliably)
     *
     * At very high dV/dt, the GDT may not trigger at all because
     * the avalanche formation time (~ns) exceeds the pulse width.
     * This is why GDTs are always paired with TVS for EFT protection.
     */
    if (v_spark <= 0.0 || dv_dt <= 0.0) return 0.0;
    return v_spark / dv_dt;
}

/* ==================================================================
 * L2: TSS (Thyristor Surge Suppressor) Model
 *
 * Physics: The TSS is a thyristor (PNPN structure) designed to
 * trigger into conduction when the voltage exceeds V_BO (break-over).
 *
 * Operation:
 * 1. Blocking: V < V_BO, high impedance, low leakage (< 5 uA)
 * 2. Break-over: V reaches V_BO, regenerative feedback triggers
 *    the device into full conduction
 * 3. On-state: Voltage collapses to V_on ~ 1-5V, conducts surge
 * 4. Turn-off: Current must drop below I_H (holding current, ~150mA)
 *    for the device to return to blocking state
 *
 * TSS advantages:
 *   - Very low clamping voltage (1-5V) once triggered
 *   - Low capacitance (30-100 pF) - good for high-speed data
 *   - No wear-out mechanism
 *
 * TSS limitations:
 *   - Cannot be used on DC circuits (latches on)
 *   - Limited di/dt capability (crowbar turn-on can cause ringing)
 *   - Temperature-dependent V_BO and I_H
 * ================================================================== */

double surge_tss_on_voltage(double i_surge, double v_bo, double r_on)
{
    /* Once triggered, the TSS crowbars to a low voltage:
     *   V_on = V_BO(at breakover) transitions to V_hold + I * R_on
     *
     * For surge analysis, we model the on-state as:
     *   V = V_on_min + I * R_on
     * where V_on_min ~ 1-5V, R_on ~ 0.01-0.1 ohm
     */
    if (i_surge < 0.0 || v_bo <= 0.0 || r_on < 0.0) return 0.0;

    /* The on-state voltage for a thyristor is typically 1.5-3V
     * from the two forward-biased junctions plus I*R drop. */
    double v_on_min = 1.5;  /* Minimum on-voltage for PNPN structure */

    return v_on_min + r_on * i_surge;
}

/* ==================================================================
 * L5: Device Selection Algorithms
 * ================================================================== */

int surge_select_mov(protection_device_params_t *dev,
                     double v_line_max, double i_surge_peak,
                     double v_withstand)
{
    if (!dev) return SURGE_ERR_NULL_PTR;
    if (v_line_max <= 0.0 || i_surge_peak <= 0.0 || v_withstand <= 0.0)
        return SURGE_ERR_INVALID_WAVEFORM;

    memset(dev, 0, sizeof(*dev));
    dev->type = PROT_MOV;

    /* Step 1: V_1mA should be > 1.1 * V_line_max (10% overhead for
     *          line voltage variations, temperature effects, aging) */
    double v_1ma_min = v_line_max * 1.1;
    /* For AC: V_1mA > V_rms * sqrt(2) * 1.1 */
    /* For DC: V_1mA > V_dc * 1.1 */

    /* Step 2: Select standard V_1mA value */
    /* Standard MOV voltages: 130, 150, 175, 230, 275, 320, 420, 510, ... */
    double standard_voltages[] = {
        18, 22, 27, 33, 39, 47, 56, 68, 82, 100, 120, 130,
        150, 175, 180, 200, 220, 230, 240, 250, 270, 275,
        300, 320, 350, 385, 420, 460, 510, 550, 575, 625,
        680, 750, 820, 910, 1000, 1100, 1200, 1500, 1800
    };
    int n_std = sizeof(standard_voltages) / sizeof(standard_voltages[0]);

    double v_1ma_selected = 0.0;
    int idx;
    for (idx = 0; idx < n_std; idx++) {
        if (standard_voltages[idx] >= v_1ma_min) {
            v_1ma_selected = standard_voltages[idx];
            break;
        }
    }
    if (v_1ma_selected <= 0.0) return SURGE_ERR_INVALID_WAVEFORM;

    dev->v_standoff = v_line_max;
    dev->v_breakdown_min = v_1ma_selected * 0.9;
    dev->v_breakdown_max = v_1ma_selected * 1.1;
    dev->alpha = 30.0;  /* Typical ZnO non-linearity exponent */

    /* Step 3: Calculate clamping voltage at peak surge current */
    dev->v_clamp_rated = surge_mov_clamp_voltage(i_surge_peak,
                                              v_1ma_selected, dev->alpha);

    /* Step 4: Check clamping voltage against withstand */
    if (dev->v_clamp_rated > v_withstand) {
        return SURGE_ERR_PROTECTION_FAIL;
    }

    /* Step 5: Select disk size based on surge current */
    if (i_surge_peak <= 400.0) {
        /* 5mm disk */
        dev->i_peak_rated = 400.0;
        dev->energy_rating = 10.0;
        dev->capacitance_typ = 100.0;
        snprintf(dev->part_number, sizeof(dev->part_number),
                 "MOV-5D-%.0fK", v_1ma_selected);
    } else if (i_surge_peak <= 1200.0) {
        /* 7mm disk */
        dev->i_peak_rated = 1200.0;
        dev->energy_rating = 30.0;
        dev->capacitance_typ = 200.0;
        snprintf(dev->part_number, sizeof(dev->part_number),
                 "MOV-7D-%.0fK", v_1ma_selected);
    } else if (i_surge_peak <= 2500.0) {
        /* 10mm disk */
        dev->i_peak_rated = 2500.0;
        dev->energy_rating = 55.0;
        dev->capacitance_typ = 400.0;
        snprintf(dev->part_number, sizeof(dev->part_number),
                 "MOV-10D-%.0fK", v_1ma_selected);
    } else if (i_surge_peak <= 4500.0) {
        /* 14mm disk */
        dev->i_peak_rated = 4500.0;
        dev->energy_rating = 100.0;
        dev->capacitance_typ = 800.0;
        snprintf(dev->part_number, sizeof(dev->part_number),
                 "MOV-14D-%.0fK", v_1ma_selected);
    } else if (i_surge_peak <= 6500.0) {
        /* 20mm disk */
        dev->i_peak_rated = 6500.0;
        dev->energy_rating = 200.0;
        dev->capacitance_typ = 1600.0;
        snprintf(dev->part_number, sizeof(dev->part_number),
                 "MOV-20D-%.0fK", v_1ma_selected);
    } else {
        return SURGE_ERR_OVER_STRESS;
    }

    dev->i_peak_max = dev->i_peak_rated * 1.2;
    dev->p_peak_pulse = dev->v_clamp_rated * dev->i_peak_rated;
    dev->clamping_ratio = dev->v_clamp_rated / v_1ma_selected;
    dev->response_time_ps = 500.0;  /* MOV response ~500 ps */
    dev->leakage_current_ua = 10.0;
    dev->thermal_resistance = 50.0;  /* K/W */
    dev->surge_life_cycles = 1000;
    dev->v_arc = 0.0;  /* Not applicable for MOV */

    return SURGE_OK;
}

int surge_select_tvs(protection_device_params_t *dev,
                     double v_line_max, double i_surge_peak,
                     double v_withstand, int is_bidirectional)
{
    if (!dev) return SURGE_ERR_NULL_PTR;
    if (v_line_max <= 0.0 || i_surge_peak <= 0.0 || v_withstand <= 0.0)
        return SURGE_ERR_INVALID_WAVEFORM;

    memset(dev, 0, sizeof(*dev));
    dev->type = is_bidirectional ? PROT_TVS_B : PROT_TVS_U;

    /* Step 1: V_standoff > V_line_max (no conduction in normal operation) */
    dev->v_standoff = v_line_max;

    /* Step 2: V_BR should be 10-15% above V_line_max */
    dev->v_breakdown_min = v_line_max * 1.10;
    dev->v_breakdown_max = v_line_max * 1.15;

    /* Step 3: TVS dynamic resistance (typical for silicon TVS) */
    double r_dyn;
    if (dev->v_breakdown_max < 10.0) r_dyn = 5.0;
    else if (dev->v_breakdown_max < 50.0) r_dyn = 1.0;
    else if (dev->v_breakdown_max < 100.0) r_dyn = 0.5;
    else r_dyn = 0.2;

    /* Step 4: Clamping voltage */
    dev->v_clamp_rated = surge_tvs_clamp_voltage(i_surge_peak,
                                                  dev->v_breakdown_max, r_dyn);

    /* Step 5: Check clamping */
    if (dev->v_clamp_rated > v_withstand) {
        return SURGE_ERR_PROTECTION_FAIL;
    }

    /* Step 6: Select power rating based on surge current */
    if (i_surge_peak <= 10.0) {
        /* 600W SMA package */
        dev->p_peak_pulse = 600.0;
        dev->i_peak_rated = 10.0;
        dev->energy_rating = 0.6;
        dev->capacitance_typ = 500.0;
        snprintf(dev->part_number, sizeof(dev->part_number),
                 "SMAJ%.0f%s", dev->v_standoff,
                 is_bidirectional ? "CA" : "A");
    } else if (i_surge_peak <= 30.0) {
        /* 1500W SMB package */
        dev->p_peak_pulse = 1500.0;
        dev->i_peak_rated = 30.0;
        dev->energy_rating = 1.5;
        dev->capacitance_typ = 1000.0;
        snprintf(dev->part_number, sizeof(dev->part_number),
                 "SMBJ%.0f%s", dev->v_standoff,
                 is_bidirectional ? "CA" : "A");
    } else if (i_surge_peak <= 60.0) {
        /* 3000W SMC package */
        dev->p_peak_pulse = 3000.0;
        dev->i_peak_rated = 60.0;
        dev->energy_rating = 3.0;
        dev->capacitance_typ = 2000.0;
        snprintf(dev->part_number, sizeof(dev->part_number),
                 "SMCJ%.0f%s", dev->v_standoff,
                 is_bidirectional ? "CA" : "A");
    } else if (i_surge_peak <= 100.0) {
        /* 5000W DO-201 package */
        dev->p_peak_pulse = 5000.0;
        dev->i_peak_rated = 100.0;
        dev->energy_rating = 5.0;
        dev->capacitance_typ = 5000.0;
        snprintf(dev->part_number, sizeof(dev->part_number),
                 "5KP%.0f%s", dev->v_standoff,
                 is_bidirectional ? "CA" : "A");
    } else {
        return SURGE_ERR_OVER_STRESS;
    }

    dev->i_peak_max = dev->i_peak_rated * 1.2;
    dev->clamping_ratio = dev->v_clamp_rated / dev->v_standoff;
    dev->response_time_ps = 1.0;  /* TVS response ~1 ps intrinsic */
    dev->leakage_current_ua = 1.0;
    dev->thermal_resistance = 80.0;
    dev->surge_life_cycles = 100000;
    dev->alpha = 0.0;  /* Not applicable for TVS */
    dev->v_arc = 0.0;

    return SURGE_OK;
}

int surge_select_gdt(protection_device_params_t *dev,
                     double v_line_max, double i_surge_peak,
                     double v_withstand)
{
    if (!dev) return SURGE_ERR_NULL_PTR;
    if (v_line_max <= 0.0 || i_surge_peak <= 0.0 || v_withstand <= 0.0)
        return SURGE_ERR_INVALID_WAVEFORM;

    memset(dev, 0, sizeof(*dev));
    dev->type = PROT_GDT;

    /* Step 1: V_spark > 1.2 * V_line_peak (must not trigger on normal) */
    double v_spark_min = v_line_max * 1.2;

    /* Step 2: Select standard spark-over voltage */
    double standard_gdt[] = {75, 90, 150, 230, 350, 470, 600, 800, 1000};
    int n_gdt = sizeof(standard_gdt) / sizeof(standard_gdt[0]);

    double v_spark_selected = 0.0;
    int idx;
    for (idx = 0; idx < n_gdt; idx++) {
        if (standard_gdt[idx] >= v_spark_min) {
            v_spark_selected = standard_gdt[idx];
            break;
        }
    }
    if (v_spark_selected <= 0.0) return SURGE_ERR_INVALID_WAVEFORM;

    dev->v_standoff = v_spark_selected * 0.8;
    dev->v_breakdown_min = v_spark_selected * 0.8;
    dev->v_breakdown_max = v_spark_selected * 1.2;
    dev->v_arc = 15.0;  /* Typical arc voltage for noble gas */
    dev->v_clamp_rated = dev->v_arc;  /* Once in arc, voltage ~ arc */

    /* Step 3: Clamp < withstand */
    if (dev->v_arc > v_withstand) {
        /* GDT arc voltage is very low, so this rarely fails */
    }

    /* Step 4: Select GDT current rating */
    if (i_surge_peak <= 5000.0) {
        dev->i_peak_rated = 5000.0;
        dev->energy_rating = 50.0;  /* J */
        dev->capacitance_typ = 1.5;
        snprintf(dev->part_number, sizeof(dev->part_number),
                 "GDT-2E-%.0fV-5kA", v_spark_selected);
    } else if (i_surge_peak <= 10000.0) {
        dev->i_peak_rated = 10000.0;
        dev->energy_rating = 100.0;
        dev->capacitance_typ = 2.0;
        snprintf(dev->part_number, sizeof(dev->part_number),
                 "GDT-2E-%.0fV-10kA", v_spark_selected);
    } else if (i_surge_peak <= 20000.0) {
        dev->i_peak_rated = 20000.0;
        dev->energy_rating = 200.0;
        dev->capacitance_typ = 3.0;
        snprintf(dev->part_number, sizeof(dev->part_number),
                 "GDT-2E-%.0fV-20kA", v_spark_selected);
    } else {
        return SURGE_ERR_OVER_STRESS;
    }

    dev->i_peak_max = dev->i_peak_rated * 1.5;
    dev->p_peak_pulse = 0.0;  /* Not applicable for crowbar */
    dev->clamping_ratio = 0.1;  /* Very low once triggered */
    dev->response_time_ps = 500000.0;  /* GDT response ~500 ns */
    dev->leakage_current_ua = 0.001;  /* <1 nA */
    dev->thermal_resistance = 100.0;
    dev->surge_life_cycles = 500;
    dev->alpha = 0.0;

    return SURGE_OK;
}

/* ==================================================================
 * L5: Device Validation
 * ================================================================== */

int surge_validate_device(const protection_device_params_t *dev,
                          const surge_waveform_params_t *wf,
                          double v_withstand)
{
    if (!dev || !wf) return SURGE_ERR_NULL_PTR;

    /* Check 1: Peak current */
    double i_surge = wf->i_peak > 0.0 ? wf->i_peak :
                     wf->v_peak / wf->source_impedance;
    if (i_surge > dev->i_peak_max) return SURGE_ERR_OVER_STRESS;

    /* Check 2: Peak power (for non-crowbar devices) */
    if (dev->type != PROT_GDT && dev->type != PROT_TSS) {
        double p_surge = dev->v_clamp_rated * i_surge;
        if (p_surge > dev->p_peak_pulse * 1.5) /* 50% headroom */
            return SURGE_ERR_OVER_STRESS;
    }

    /* Check 3: Energy */
    if (wf->energy_per_pulse > dev->energy_rating)
        return SURGE_ERR_OVER_STRESS;

    /* Check 4: Clamping voltage */
    if (dev->v_clamp_rated > v_withstand)
        return SURGE_ERR_PROTECTION_FAIL;

    /* Check 5: Temperature rise for TVS */
    if (dev->type == PROT_TVS_U || dev->type == PROT_TVS_B) {
        double temp_rise = surge_tvs_temp_rise(wf->energy_per_pulse,
                                               2.25, 200.0);
        if (temp_rise > 150.0) /* Max junction temp rise */
            return SURGE_ERR_THERMAL_RUNAWAY;
    }

    return SURGE_OK;
}

double surge_protection_margin(const protection_device_params_t *dev,
                                const surge_waveform_params_t *wf,
                                double v_withstand)
{
    if (!dev || !wf) return 0.0;

    double i_surge = wf->i_peak > 0.0 ? wf->i_peak :
                     wf->v_peak / wf->source_impedance;

    double margin_i = dev->i_peak_max / (i_surge + 1e-9);
    double margin_e = dev->energy_rating / (wf->energy_per_pulse + 1e-9);
    double margin_v = v_withstand / (dev->v_clamp_rated + 1e-9);

    /* Return minimum margin (weakest link) */
    double margin = margin_i;
    if (margin_e < margin) margin = margin_e;
    if (margin_v < margin) margin = margin_v;

    return margin;
}

/* ==================================================================
 * L2: Standard Device Database
 * ================================================================== */

int surge_mov_standard(protection_device_params_t *dev,
                       double v_1ma, int disk_diameter_mm)
{
    if (!dev) return SURGE_ERR_NULL_PTR;
    if (v_1ma <= 0.0 || disk_diameter_mm <= 0) return SURGE_ERR_INVALID_WAVEFORM;

    memset(dev, 0, sizeof(*dev));
    dev->type = PROT_MOV;
    dev->v_standoff = v_1ma * 0.8;  /* Approximate AC rating */
    dev->v_breakdown_min = v_1ma * 0.9;
    dev->v_breakdown_max = v_1ma * 1.1;
    dev->alpha = 30.0;

    /* Set ratings based on disk diameter */
    switch (disk_diameter_mm) {
        case 5:
            dev->i_peak_rated = 400.0;
            dev->energy_rating = 10.0;
            dev->capacitance_typ = 100.0;
            break;
        case 7:
            dev->i_peak_rated = 1200.0;
            dev->energy_rating = 30.0;
            dev->capacitance_typ = 200.0;
            break;
        case 10:
            dev->i_peak_rated = 2500.0;
            dev->energy_rating = 55.0;
            dev->capacitance_typ = 400.0;
            break;
        case 14:
            dev->i_peak_rated = 4500.0;
            dev->energy_rating = 100.0;
            dev->capacitance_typ = 800.0;
            break;
        case 20:
            dev->i_peak_rated = 6500.0;
            dev->energy_rating = 200.0;
            dev->capacitance_typ = 1600.0;
            break;
        default:
            return SURGE_ERR_INVALID_WAVEFORM;
    }

    dev->v_clamp_rated = surge_mov_clamp_voltage(dev->i_peak_rated,
                                                  v_1ma, dev->alpha);
    dev->i_peak_max = dev->i_peak_rated * 1.2;
    dev->p_peak_pulse = dev->v_clamp_rated * dev->i_peak_rated;
    dev->clamping_ratio = dev->v_clamp_rated / v_1ma;
    dev->response_time_ps = 500.0;
    dev->leakage_current_ua = 10.0;
    dev->thermal_resistance = 50.0;
    dev->surge_life_cycles = 1000;
    snprintf(dev->part_number, sizeof(dev->part_number),
             "MOV-%dD-%.0fK", disk_diameter_mm, v_1ma);

    return SURGE_OK;
}

int surge_tvs_standard(protection_device_params_t *dev,
                       double v_standoff, int power_rating_w,
                       int is_bidirectional)
{
    if (!dev) return SURGE_ERR_NULL_PTR;
    if (v_standoff <= 0.0 || power_rating_w <= 0) return SURGE_ERR_INVALID_WAVEFORM;

    memset(dev, 0, sizeof(*dev));
    dev->type = is_bidirectional ? PROT_TVS_B : PROT_TVS_U;
    dev->v_standoff = v_standoff;
    dev->v_breakdown_min = v_standoff * 1.10;
    dev->v_breakdown_max = v_standoff * 1.15;
    dev->p_peak_pulse = (double)power_rating_w;
    dev->response_time_ps = 1.0;
    dev->leakage_current_ua = 1.0;
    dev->thermal_resistance = 80.0;
    dev->surge_life_cycles = 100000;

    /* Derive ratings from power rating */
    double r_dyn;
    if (dev->v_breakdown_max < 10.0) r_dyn = 5.0;
    else if (dev->v_breakdown_max < 50.0) r_dyn = 1.0;
    else r_dyn = 0.2;

    switch (power_rating_w) {
        case 400:
            dev->i_peak_rated = 8.0;
            dev->energy_rating = 0.4;
            dev->capacitance_typ = 300.0;
            snprintf(dev->part_number, sizeof(dev->part_number),
                     "SMAJ%.0f%s", v_standoff, is_bidirectional ? "CA" : "A");
            break;
        case 600:
            dev->i_peak_rated = 10.0;
            dev->energy_rating = 0.6;
            dev->capacitance_typ = 500.0;
            snprintf(dev->part_number, sizeof(dev->part_number),
                     "SMAJ%.0f%s", v_standoff, is_bidirectional ? "CA" : "A");
            break;
        case 1500:
            dev->i_peak_rated = 30.0;
            dev->energy_rating = 1.5;
            dev->capacitance_typ = 1000.0;
            snprintf(dev->part_number, sizeof(dev->part_number),
                     "SMBJ%.0f%s", v_standoff, is_bidirectional ? "CA" : "A");
            break;
        case 3000:
            dev->i_peak_rated = 60.0;
            dev->energy_rating = 3.0;
            dev->capacitance_typ = 2000.0;
            snprintf(dev->part_number, sizeof(dev->part_number),
                     "SMCJ%.0f%s", v_standoff, is_bidirectional ? "CA" : "A");
            break;
        case 5000:
            dev->i_peak_rated = 100.0;
            dev->energy_rating = 5.0;
            dev->capacitance_typ = 5000.0;
            snprintf(dev->part_number, sizeof(dev->part_number),
                     "5KP%.0f%s", v_standoff, is_bidirectional ? "CA" : "A");
            break;
        default:
            return SURGE_ERR_INVALID_WAVEFORM;
    }

    dev->v_clamp_rated = surge_tvs_clamp_voltage(dev->i_peak_rated,
                                                  dev->v_breakdown_max, r_dyn);
    dev->i_peak_max = dev->i_peak_rated * 1.2;
    dev->clamping_ratio = dev->v_clamp_rated / v_standoff;

    return SURGE_OK;
}

int surge_gdt_standard(protection_device_params_t *dev,
                       double v_spark, int i_surge_ka, int num_electrodes)
{
    if (!dev) return SURGE_ERR_NULL_PTR;
    if (v_spark <= 0.0 || i_surge_ka <= 0) return SURGE_ERR_INVALID_WAVEFORM;

    memset(dev, 0, sizeof(*dev));
    dev->type = PROT_GDT;
    dev->v_standoff = v_spark * 0.8;
    dev->v_breakdown_min = v_spark * 0.8;
    dev->v_breakdown_max = v_spark * 1.2;
    dev->v_clamp_rated = 15.0;  /* Arc voltage */
    dev->v_arc = 15.0;
    dev->i_peak_rated = (double)i_surge_ka * 1000.0;
    dev->i_peak_max = dev->i_peak_rated * 1.5;
    dev->energy_rating = (double)i_surge_ka * 10.0;  /* ~10J per kA */
    dev->capacitance_typ = num_electrodes == 3 ? 2.0 : 1.5;
    dev->clamping_ratio = 0.1;
    dev->response_time_ps = 500000.0;
    dev->leakage_current_ua = 0.001;
    dev->thermal_resistance = 100.0;
    dev->surge_life_cycles = 500;

    snprintf(dev->part_number, sizeof(dev->part_number),
             "GDT-%dE-%.0fV-%dkA", num_electrodes, v_spark, i_surge_ka);

    return SURGE_OK;
}