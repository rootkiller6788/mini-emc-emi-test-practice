/**
 * @file dm_filter_model.h
 * @brief Differential-Mode EMI Filter Models & Analysis
 *
 * Comprehensive differential-mode filter modeling including:
 *  - Filter topology enumeration (L, C, LC, CL, π, T, multi-stage)
 *  - DM inductor modeling (with core loss, saturation, parasitics)
 *  - X-capacitor modeling (with ESL, ESR, safety ratings)
 *  - Transfer function derivation for each topology
 *  - Impedance interactions with LISN and EUT
 *  - Worst-case impedance analysis (0.1Ω / 100Ω per CISPR 17)
 *
 * Reference:
 *  - CISPR 17: Methods of measurement of suppression characteristics
 *  - Paul: "Introduction to EMC" — Chapter 6 (EMI Filters)
 *  - Ozenbaugh, R.L. "EMI Filter Design" (2011)
 */

#ifndef DM_FILTER_MODEL_H
#define DM_FILTER_MODEL_H

#include "cm_dm_filter.h"
#include "cm_choke_model.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * L1: X-capacitor and Y-capacitor definitions
 *
 * X-capacitor: Connected line-to-neutral (L-N) — across the line.
 *   Fails short-circuit → upstream fuse must clear.
 *   Classification: X1 (impulse 4 kV), X2 (2.5 kV), X3 (none)
 *
 * Y-capacitor: Connected line-to-PE (L-GND, N-GND) — to earth.
 *   Fails open-circuit → safety-critical (no risk of shock).
 *   Classification: Y1 (double-insulated, 8 kV), Y2 (5 kV),
 *                    Y3 (none), Y4 (2.5 kV)
 *   Y-cap value limited by leakage current:
 *     I_leak = 2π · f · C · V  ≤ 3.5 mA (IEC 60950 for ITE)
 * ======================================================================== */

typedef enum {
    X_CLASS_X1 = 0,             /**< X1: 4 kV impulse, across-the-line */
    X_CLASS_X2 = 1,             /**< X2: 2.5 kV impulse, general purpose */
    X_CLASS_X3 = 2              /**< X3: no impulse rating, low voltage */
} x_cap_class_t;

typedef enum {
    Y_CLASS_Y1 = 0,             /**< Y1: 8 kV, double insulation */
    Y_CLASS_Y2 = 1,             /**< Y2: 5 kV, basic insulation */
    Y_CLASS_Y3 = 2,             /**< Y3: no impulse, low voltage */
    Y_CLASS_Y4 = 3              /**< Y4: 2.5 kV, basic */
} y_cap_class_t;

/** @brief Safety capacitor with EMC and safety dual ratings */
typedef struct {
    filter_element_t base;      /**< Base filter element */
    x_cap_class_t x_class;      /**< X-class rating (if X-cap) */
    y_cap_class_t y_class;      /**< Y-class rating (if Y-cap) */
    double impulse_voltage;     /**< Impulse withstand voltage [V] */
    double leakage_current;     /**< Leakage current at rated V/f [A] */
    double discharge_constant;  /**< Discharge time constant [s] (R*C < 1s for safety) */
} safety_capacitor_t;

/* ========================================================================
 * L1: DM filter topology-specific parameters
 * ======================================================================== */

/** @brief LC filter stage with explicit topology-element mapping */
typedef struct {
    filter_topology_t topology;     /**< Topology family */
    double l_series;                /**< Series inductance [H] (L or L1) */
    double c_shunt_input;           /**< Input shunt capacitance [F] (π, T) */
    double c_shunt_output;          /**< Output shunt capacitance [F] (π) */
    double l_series2;               /**< Second series inductance [H] (T, double-π) */
    double r_damp;                  /**< Damping resistance [Ω] (critical/optimal) */
    double r_bleed;                 /**< Bleed resistor [Ω] (X-cap discharge) */
} dm_lc_params_t;

/* ========================================================================
 * L2: Source/load impedance for insertion loss measurement
 *
 * CISPR 17 defines the standardized test conditions:
 *   50Ω / 50Ω — standard load
 *   0.1Ω / 100Ω — worst-case (source) for DM filters
 *   100Ω / 0.1Ω — worst-case (load) for DM filters
 *
 * The filter must perform adequately under ALL four combinations.
 * ======================================================================== */

/** @brief CISPR 17 standard impedance condition */
typedef enum {
    CISPR17_50_50 = 0,         /**< 50 Ω source, 50 Ω load */
    CISPR17_01_100 = 1,        /**< 0.1 Ω source, 100 Ω load */
    CISPR17_100_01 = 2,        /**< 100 Ω source, 0.1 Ω load */
    CISPR17_CUSTOM = 3         /**< User-specified impedance */
} cispr17_impedance_t;

typedef struct {
    cispr17_impedance_t condition;
    double z_source;            /**< Source impedance [Ω] */
    double z_load;              /**< Load impedance [Ω] */
} cispr17_config_t;

/* ========================================================================
 * L3: DM filter transfer function for each topology
 * ======================================================================== */

/**
 * @brief s-domain transfer function for a single-series L filter.
 *
 * H(s) = Z_L / (Z_L + s*L)
 */
typedef struct {
    double l; double r_load;
    /* Derived: cutoff ω_c = R_load / L */
} transfer_L_series_t;

/**
 * @brief s-domain transfer function for a single-shunt C filter.
 *
 * H(s) = 1 / (1 + s*R_s*C)
 */
typedef struct {
    double c; double r_source;
} transfer_C_shunt_t;

/**
 * @brief s-domain transfer function for LC low-pass filter.
 *
 *        1 / (LC)
 * H(s) = ──────────────────────────
 *        s² + s/(R_load*C) + 1/(LC)
 */
typedef struct {
    double l; double c;
    double r_load; double r_source;
    double omega_n;             /**< Natural frequency [rad/s] */
    double zeta;                /**< Damping ratio */
} transfer_LC_t;

/**
 * @brief s-domain transfer function for π-filter (C1—L—C2).
 *
 *        Z_L / Z_s
 * H(s) = ──────────── * ──────────────────────────
 *        1 + s*Z_L*C₂        1 + s*Z_s*C₁ + s²*L*C₁
 */
typedef struct {
    double l; double c1; double c2;
    double r_load; double r_source;
    double omega_01; double omega_02; /**< Corner frequencies */
} transfer_pi_t;

/**
 * @brief s-domain transfer function for T-filter (L1—C—L2).
 */
typedef struct {
    double l1; double c; double l2;
    double r_load; double r_source;
} transfer_T_t;

/* ========================================================================
 * L5: Filter performance metrics
 * ======================================================================== */

/**
 * @brief Complete DM filter performance evaluation at one frequency.
 */
typedef struct {
    double frequency;               /**< Evaluation frequency [Hz] */
    complex_t z_filter_input;       /**< Filter input impedance [Ω] */
    complex_t z_filter_output;      /**< Filter output impedance [Ω] */
    double insertion_loss_db;       /**< DM insertion loss [dB] */
    double voltage_attenuation;     /**< Vout/Vin ratio */
    double power_loss_w;            /**< Filter power dissipation [W] */
    double phase_margin_deg;        /**< Phase margin at crossover [°] */
    double ripple_db;               /**< Passband ripple [dB] */
} dm_filter_perf_t;

/* ========================================================================
 * L6: DM inductor design
 * ======================================================================== */

/** @brief DM inductor core selection */
typedef struct {
    core_material_id_t material;    /**< Core material */
    double inductance_target;       /**< Target inductance [H] */
    double rated_current_rms;       /**< RMS rated current [A] */
    double peak_current;            /**< Peak current (includes ripple) [A] */
    double ripple_freq;             /**< Ripple frequency [Hz] */
    double max_dc_resistance;       /**< Maximum DC resistance [Ω] */
    double max_volume_cm3;          /**< Maximum volume [cm³] */
    double current_density_amp_mm2; /**< Allowable current density [A/mm²] */
} dm_inductor_spec_t;

/** @brief DM inductor design result */
typedef struct {
    core_description_t core;        /**< Selected core */
    winding_params_t winding;       /**< Designed winding */
    double achieved_inductance;     /**< Achieved inductance [H] */
    double dc_resistance;           /**< DC resistance [Ω] */
    double ac_resistance;           /**< AC resistance at ripple freq [Ω] */
    double saturation_current;      /**< Current at 70% L drop [A] */
    double copper_loss_w;           /**< Copper loss at rated current [W] */
    double core_loss_w;             /**< Core loss at ripple [W] */
    double total_loss_w;            /**< Total loss [W] */
    double temperature_rise_k;      /**< Estimated temp rise [K] */
    double volume_cm3;              /**< Total volume [cm³] */
} dm_inductor_design_t;

/* ========================================================================
 * L7: Standard filter product families
 * ======================================================================== */

/** @brief Off-the-shelf filter product data */
typedef struct {
    char part_number[32];           /**< Manufacturer P/N */
    char manufacturer[32];          /**< Manufacturer name */
    double rated_current_a;         /**< Rated current [A] */
    double rated_voltage_v;         /**< Rated voltage [V] */
    double cm_il_at_150k_db;        /**< CM IL at 150 kHz [dB] */
    double dm_il_at_150k_db;        /**< DM IL at 150 kHz [dB] */
    double cm_il_at_1M_db;          /**< CM IL at 1 MHz [dB] */
    double dm_il_at_1M_db;          /**< DM IL at 1 MHz [dB] */
    double leakage_current_ma;      /**< Max leakage current [mA] */
    double power_loss_w;            /**< No-load power loss [W] */
    double dimensions_mm[3];        /**< L×W×H [mm] */
    double weight_g;                /**< Weight [g] */
    int is_medical_grade;           /**< Medical grade (IEC 60601) */
} commercial_filter_t;

/* ========================================================================
 * API FUNCTIONS
 * ======================================================================== */

/**
 * @brief Compute DM LC filter parameters from user specification.
 *
 * Given a desired cutoff frequency and topology, calculate the L and C
 * values that provide the target attenuation.
 *
 * @param topology   Filter topology
 * @param f_cutoff   Desired cutoff frequency [Hz]
 * @param r_source   Source impedance [Ω]
 * @param r_load     Load impedance [Ω]
 * @param params     Output LC parameters [non-NULL]
 * @return 0 on success, -1 if no feasible solution
 */
int dm_lc_params_calculate(filter_topology_t topology,
                            double f_cutoff, double r_source, double r_load,
                            dm_lc_params_t *params);

/**
 * @brief Compute DM filter transfer function at a complex frequency.
 *
 * L3: Evaluates H(s) for the given topology with all parasitics.
 *
 * @param topology Filter topology
 * @param params   LC parameters
 * @param freq     Frequency [Hz]
 * @param r_source Source impedance [Ω]
 * @param r_load   Load impedance [Ω]
 * @return H(jω) evaluated at given frequency
 */
complex_t dm_transfer_function(filter_topology_t topology,
                                const dm_lc_params_t *params,
                                double freq, double r_source, double r_load);

/**
 * @brief Compute DM filter insertion loss under CISPR 17 conditions.
 *
 * L2: IL is measured with standardized source/load impedances.
 * Worst-case IL is the minimum of 50/50, 0.1/100, and 100/0.1 conditions.
 *
 * @param params LC parameters
 * @param config CISPR 17 impedance configuration
 * @param freq   Frequency [Hz]
 * @return Insertion loss [dB]
 */
double dm_insertion_loss_cispr(const dm_lc_params_t *params,
                                cispr17_config_t config, double freq);

/**
 * @brief Compute worst-case DM insertion loss across all CISPR 17 conditions.
 *
 * Returns the minimum IL (worst attenuation) across 50/50, 0.1/100, 100/0.1.
 *
 * @param params LC parameters
 * @param freq   Frequency [Hz]
 * @param worst_cond Output: which condition was worst
 * @return Minimum insertion loss [dB]
 */
double dm_il_worst_case(const dm_lc_params_t *params, double freq,
                         cispr17_impedance_t *worst_cond);

/**
 * @brief Create a safety capacitor element.
 *
 * X-cap: L-N, fail-short (safe to short line to neutral across fuse).
 * Y-cap: L-PE or N-PE, fail-open (safe against electric shock).
 *
 * @param type         FILTER_ELEM_X_CAPACITOR or FILTER_ELEM_Y_CAPACITOR
 * @param capacitance  Nominal capacitance [F]
 * @param x_class      X-class rating (NULL if Y-cap)
 * @param y_class      Y-class rating (NULL if X-cap)
 * @param voltage      Rated voltage [V]
 */
safety_capacitor_t *safety_capacitor_create(filter_elem_type_t type,
                                             double capacitance,
                                             x_cap_class_t *x_class,
                                             y_cap_class_t *y_class,
                                             double voltage);

/**
 * @brief Calculate Y-capacitor leakage current.
 *
 * L1: I_leak = 2π · f_line · C_y · V_line [A_rms]
 *
 * IEC 60950-1 limits:
 *   Stationary ITE: 3.5 mA
 *   Portable ITE:   0.75 mA
 * IEC 60601-1 (medical):
 *   Patient-connected: 0.5 mA (NC) / 0.1 mA (SFC)
 *   Non-patient:       0.5 mA (NC) / 1.0 mA (SFC)
 *
 * @return Leakage current [A_rms]
 */
double y_cap_leakage_current(double capacitance, double freq,
                              double voltage_rms);

/**
 * @brief Calculate maximum Y-capacitance allowed by leakage limit.
 *
 * C_y_max = I_leak_max / (2π · f_line · V_line)
 */
double y_cap_max_for_leakage(double i_leak_max_ma, double freq,
                              double voltage_rms);

/**
 * @brief Design a DM inductor from specification.
 *
 * L5 Algorithm:
 * 1. Calculate required inductance from cutoff frequency
 * 2. Compute required energy storage: E = ½ L·I²_pk
 * 3. Select core based on area product: A_p = L·I_pk·I_rms / (K·J_max·B_max)
 * 4. Calculate turns: N = L·I_pk / (B_max·A_e)
 * 5. Calculate wire gauge for I_rms
 * 6. Check window fill
 * 7. Estimate losses and temperature rise
 * 8. Iterate if constraints violated
 */
dm_inductor_design_t *dm_inductor_design(const dm_inductor_spec_t *spec);

/**
 * @brief Free DM inductor design result.
 */
void dm_inductor_design_free(dm_inductor_design_t *design);

/**
 * @brief Compute the input and output impedance of a DM filter.
 *
 * L3: The filter input impedance Z_in(f) depends on load Z_L and
 * filter topology. This is critical for source-filter interaction.
 *
 * @param topology Filter topology
 * @param params   LC parameters
 * @param freq     Frequency [Hz]
 * @param r_load   Load impedance [Ω] (output side)
 * @param z_in     Output: filter input impedance
 * @param z_out    Output: filter output impedance
 */
void dm_filter_impedances(filter_topology_t topology,
                           const dm_lc_params_t *params,
                           double freq, double r_load,
                           complex_t *z_in, complex_t *z_out);

/**
 * @brief Evaluate DM filter performance comprehensively at one frequency.
 *
 * Combines insertion loss, impedances, phase, and power loss.
 */
dm_filter_perf_t dm_filter_evaluate(const dm_lc_params_t *params,
                                     double freq,
                                     double r_source, double r_load);

/**
 * @brief Compute X-capacitor bleed resistor.
 *
 * Safety requirement: X-cap must discharge to <60V within 1 second
 * after disconnection (IEC 62368-1, replacing 60950).
 *
 * R_bleed ≤ 1s / (C · ln(V_peak / 60))
 *
 * @param capacitance X-capacitance [F]
 * @param v_peak      Peak operating voltage [V]
 * @param time_sec    Maximum discharge time [s] (typ. 1.0)
 * @return Maximum bleed resistance [Ω]
 */
double x_cap_bleed_resistor(double capacitance, double v_peak,
                             double time_sec);

/**
 * @brief Calculate DM filter component stress (voltage and current).
 *
 * Ensures components are rated for worst-case operating conditions.
 *
 * @param params  LC parameters
 * @param v_in_rms Input voltage [V_rms]
 * @param i_load_rms Load current [A_rms]
 * @param v_c_max  Output: max capacitor voltage [V]
 * @param i_l_max  Output: max inductor current [A]
 */
void dm_component_stress(const dm_lc_params_t *params,
                          double v_in_rms, double i_load_rms,
                          double *v_c_max, double *i_l_max);

/**
 * @brief Free a safety capacitor.
 */
void safety_capacitor_free(safety_capacitor_t *cap);

#ifdef __cplusplus
}
#endif

#endif /* DM_FILTER_MODEL_H */
