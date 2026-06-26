/**
 * surge_protection.h - Surge Protection Device Modeling and Selection
 *
 * L2 Core Concepts: Protection device operating principles, V-I
 * characteristics, response dynamics, failure modes.
 * L5 Algorithms: Device selection, clamping prediction, energy
 * absorption calculation, thermal analysis.
 *
 * Key device technologies:
 *   MOV - Metal Oxide Varistor (ZnO grain boundaries)
 *   TVS - Transient Voltage Suppressor (avalanche diode)
 *   GDT - Gas Discharge Tube (Townsend discharge)
 *   TSS - Thyristor Surge Suppressor (SCR crowbar)
 *
 * Reference:
 *   IEC 61643-11: Surge Protective Devices
 *   Littlefuse, "TVS Diode Application Note", 2020
 *   Bourns, "MOV Technology Overview", 2019
 *   IEEE C62.42: Guide for Surge Protective Components
 */

#ifndef SURGE_PROTECTION_H
#define SURGE_PROTECTION_H

#include "surge_defs.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================================================================
 * L2: Protection Device Operating Principles
 * ================================================================== */

/**
 * Model MOV (Metal Oxide Varistor) V-I characteristic.
 *
 * The MOV follows a power-law relationship:
 *   I = k * V^alpha
 * where alpha is the non-linearity exponent (typically 20-50 for ZnO).
 *
 * Alternatively expressed as:
 *   V = V_ref * (I / I_ref)^(1/alpha)
 *
 * The high alpha value means the MOV transitions from high impedance
 * to low impedance over a narrow voltage range, providing effective
 * clamping without crowbar behavior.
 *
 * Grain boundary physics: ZnO grains (~10um) with Bi2O3 intergranular
 * layers form back-to-back Schottky barriers. Conduction is via
 * thermionic emission and tunneling.
 *
 * Complexity: O(1)
 * Reference: Gupta, "ZnO Varistors", J. Am. Ceramic Soc., 1990
 */
double surge_mov_voltage(double current, double v_ref, double i_ref,
                          double alpha);

/**
 * Compute MOV dynamic resistance at operating point.
 *
 *   R_dyn = dV/dI = V_ref * (I/I_ref)^(1/alpha) / (alpha * I)
 *
 * This is the small-signal impedance seen by the transient.
 * Lower R_dyn = better clamping.
 *
 * Complexity: O(1)
 */
double surge_mov_dynamic_resistance(double current, double v_ref,
                                     double i_ref, double alpha);

/**
 * Compute MOV clamping voltage for a given surge current.
 *
 *   V_clamp = V_1mA * (I_surge / 0.001)^(1/alpha)
 *
 * This is the key design parameter: the voltage the protected
 * circuit will see during the surge event.
 *
 * Complexity: O(1)
 */
double surge_mov_clamp_voltage(double i_surge, double v_1ma, double alpha);

/**
 * Model TVS diode reverse characteristic.
 *
 * TVS (Transient Voltage Suppressor) is a silicon avalanche diode
 * designed for surge absorption. Three operating regions:
 *
 * 1. Off-state: V < V_BR, I = I_s (saturation current, nA-uA)
 * 2. Breakdown: V ~ V_BR, I increases exponentially (controlled avalanche)
 * 3. Snap-back: Negative resistance region (some TVS types)
 *
 * Simplified model for surge analysis:
 *   V_clamp = V_BR + R_dyn * I_surge
 * where R_dyn is the dynamic resistance in breakdown (~0.1-10 ohm).
 *
 * Complexity: O(1)
 * Reference: Sze & Ng, Physics of Semiconductor Devices, 2007
 */
double surge_tvs_clamp_voltage(double i_surge, double v_br, double r_dyn);

/**
 * Compute TVS junction temperature rise during a surge pulse.
 *
 * Using the adiabatic approximation (valid for pulses < 100 us):
 *   Delta_T = E_absorbed / (C_p * mass)
 *
 * where C_p is silicon specific heat (0.7 J/g*K) and mass is the
 * die mass estimated from device volume.
 *
 * Thermal time constant of typical TVS packages (SMB/DO-214):
 *   tau_thermal ~ 10-100 ms
 * Since surge pulses (8/20us) are much shorter than tau_thermal,
 * the adiabatic approximation is valid.
 *
 * Complexity: O(1)
 */
double surge_tvs_temp_rise(double energy_absorbed, double die_area_mm2,
                            double die_thickness_um);

/**
 * Model GDT (Gas Discharge Tube) spark-over characteristic.
 *
 * GDT operation phases:
 * 1. Pre-breakdown: High impedance (>1 Gohm), C ~ 1-5 pF
 * 2. Spark-over: Voltage reaches V_spark (~90V-1kV), avalanche
 *    breakdown across gas gap, voltage collapses
 * 3. Glow discharge: V ~ 70-100V, current limited (~0.1-1A)
 * 4. Arc discharge: V ~ 10-30V, high current capability (kA)
 *
 * Once triggered, the GDT crowbars to a low arc voltage (~15-25V),
 * providing excellent protection. However, the spark-over time
 * (~100ns-1us) limits effectiveness against very fast transients.
 *
 * The follow-on current after the surge must be below the GDT's
 * extinguishing limit, or the GDT stays in arc mode.
 *
 * Complexity: O(1)
 */
double surge_gdt_arc_voltage(double i_surge, double v_arc_min, double r_arc);

/**
 * Compute GDT spark-over time based on voltage ramp rate.
 *
 *   t_spark = V_spark / (dV/dt)
 *
 * For slow-rising surges, the GDT triggers reliably. For fast
 * transients (EFT, ESD), the spark-over may be delayed or fail
 * entirely, requiring a parallel fast protector (TVS or MOV).
 *
 * Complexity: O(1)
 */
double surge_gdt_spark_time(double v_spark, double dv_dt);

/**
 * Model TSS (Thyristor Surge Suppressor) break-over behavior.
 *
 * TSS is a thyristor-based crowbar device:
 * 1. Blocking state: High impedance, V < V_BO (break-over voltage)
 * 2. Break-over: V reaches V_BO, device triggers into conduction
 * 3. On-state: V drops to ~1-5V, conducts high current
 * 4. Hold: Stays on until current drops below I_H (holding current)
 *
 * Advantages: Very low clamping voltage, low capacitance (~30-100pF)
 * Disadvantages: Can latch on DC circuits (need I < I_H to reset)
 *
 * Complexity: O(1)
 */
double surge_tss_on_voltage(double i_surge, double v_bo, double r_on);

/* ==================================================================
 * L5: Device Selection Algorithms
 * ================================================================== */

/**
 * Select appropriate MOV for given surge protection requirement.
 *
 * Algorithm:
 * 1. Calculate required V_1mA = V_line_max * 1.1 (10% margin)
 * 2. Select next higher standard V_1mA rating
 * 3. Calculate clamp voltage at peak surge current
 * 4. Check if clamp voltage < V_withstand of protected circuit
 * 5. Calculate energy per pulse and compare to device rating
 * 6. Apply derating factors (temperature, aging, multiple pulses)
 *
 * Returns SURGE_OK if a suitable device exists, fills device params.
 *
 * Complexity: O(1) for fixed device database lookup
 */
int surge_select_mov(protection_device_params_t *dev,
                     double v_line_max, double i_surge_peak,
                     double v_withstand);

/**
 * Select appropriate TVS for given surge protection requirement.
 *
 * Algorithm:
 * 1. V_standoff > V_line_max (must not conduct in normal operation)
 * 2. V_BR min > V_line_max + ripple
 * 3. V_clamp_max @ I_pp < V_withstand
 * 4. P_pp rating > P_surge_pulse
 * 5. Check junction temperature rise < T_j_max
 *
 * Complexity: O(1)
 */
int surge_select_tvs(protection_device_params_t *dev,
                     double v_line_max, double i_surge_peak,
                     double v_withstand, int is_bidirectional);

/**
 * Select appropriate GDT for primary surge protection.
 *
 * Algorithm:
 * 1. V_spark_min > V_line_peak (must not trigger on normal voltage)
 * 2. V_spark_max < V_withstand (must trigger before damage)
 * 3. I_surge_max > I_surge_peak (with derating for multi-pulse)
 * 4. Arc voltage < acceptable let-through
 * 5. Check follow-on current extinguishing capability
 *
 * Complexity: O(1)
 */
int surge_select_gdt(protection_device_params_t *dev,
                     double v_line_max, double i_surge_peak,
                     double v_withstand);

/**
 * Validate a protection device against a surge waveform.
 *
 * Checks:
 * - Peak pulse current < I_pp rated
 * - Peak pulse power < P_pp rated
 * - Energy per pulse < Energy rating
 * - V_clamp < V_withstand
 * - Junction temperature rise < 175 C (for silicon devices)
 *
 * Returns SURGE_OK if device passes all checks.
 * Returns SURGE_ERR_OVER_STRESS if any check fails.
 *
 * Complexity: O(1)
 */
int surge_validate_device(const protection_device_params_t *dev,
                          const surge_waveform_params_t *wf,
                          double v_withstand);

/**
 * Compute protection margin (ratio of device rating to stress).
 *
 *   margin = min(I_rated/I_peak, E_rated/E_pulse, V_withstand/V_clamp)
 *
 * A margin > 1.5 is recommended for commercial products.
 * A margin > 2.0 is recommended for industrial/mission-critical.
 *
 * Complexity: O(1)
 */
double surge_protection_margin(const protection_device_params_t *dev,
                                const surge_waveform_params_t *wf,
                                double v_withstand);

/* ==================================================================
 * L2: Standard Device Database Entries
 * ================================================================== */

/**
 * Get standard MOV device parameters for a given V_1mA rating.
 *
 * Provides typical parameters for common MOV sizes (5mm, 7mm, 10mm,
 * 14mm, 20mm disk diameters). Each size has different energy and
 * current handling capability.
 *
 * Complexity: O(1)
 */
int surge_mov_standard(protection_device_params_t *dev,
                       double v_1ma, int disk_diameter_mm);

/**
 * Get standard TVS device parameters.
 *
 * Covers common TVS families:
 * - 600W series (SMA package, 10/1000us)
 * - 1500W series (SMB package)
 * - 3000W series (SMC package)
 * - 5000W series (DO-201 package)
 *
 * Complexity: O(1)
 */
int surge_tvs_standard(protection_device_params_t *dev,
                       double v_standoff, int power_rating_w,
                       int is_bidirectional);

/**
 * Get standard GDT device parameters.
 *
 * Common GDT types:
 * - 2-electrode (line-to-ground)
 * - 3-electrode (balanced, line-to-line-to-ground)
 * - DC spark-over: 90V, 150V, 230V, 350V, 470V, 600V
 * - Surge rating: 5kA, 10kA, 20kA (8/20us)
 *
 * Complexity: O(1)
 */
int surge_gdt_standard(protection_device_params_t *dev,
                       double v_spark, int i_surge_ka, int num_electrodes);

#ifdef __cplusplus
}
#endif

#endif /* SURGE_PROTECTION_H */