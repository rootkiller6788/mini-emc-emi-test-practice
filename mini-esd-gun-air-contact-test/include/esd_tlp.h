/**
 * @file esd_tlp.h
 * @brief Transmission Line Pulse (TLP) measurement and analysis
 *
 * TLP is the standard method for characterizing ESD protection
 * device behavior under high-current transient conditions.
 * A charged transmission line produces rectangular pulses with
 * well-defined current and voltage levels, enabling quasi-DC
 * I-V characterization under ESD-like conditions.
 *
 * Reference: ANSI/ESD SP5.5.1 (TLP testing), JEDEC JEP157 (TLP guide)
 *
 * Knowledge coverage:
 *   L1 - Definitions: TLP pulse width, rise time, TLP I-V curve
 *   L2 - Core Concepts: Quasi-static I-V characterization
 *   L3 - Math Structures: Transmission line theory, TDR principles
 *   L4 - Fundamental Laws: TLP energy equation, impedance matching
 *   L5 - Algorithms: TLP I-V extraction, leakage measurement
 *   L8 - Advanced Topics: VFTLP (Very Fast TLP), SEED methodology
 */

#ifndef ESD_TLP_H
#define ESD_TLP_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── TLP Fundamentals ─────────────────────────────────────────────
 *
 * L1 Definition: A Transmission Line Pulse (TLP) system consists of:
 *   1. A charged transmission line (typically 50 Ω)
 *   2. A high-voltage switch (mercury-wetted or solid-state relay)
 *   3. A DUT (Device Under Test) at the end of the line
 *   4. Measurement: incident + reflected voltage/current
 *
 * Standard TLP pulse widths: 100 ns (most common), 10 ns, 200 ns
 * Standard rise times: 200 ps - 10 ns
 *
 * L4 Theorem: For a lossless transmission line of length l charged to V₀:
 *   - The delivered pulse has amplitude V₀/2 (if Z₀ = Z_load)
 *   - Pulse duration = 2l / v_prop (round-trip time)
 *   - Delivered energy = V₀² · τ / (4 · Z₀)  (for matched load)
 *
 * L2 Core Concept: TLP provides a rectangular current pulse whose
 * amplitude is proportional to the charging voltage. By stepping
 * the voltage and measuring I, V at the DUT, a quasi-static I-V
 * curve is obtained — this is the "TLP I-V curve".
 */

/** TLP system configuration */
typedef enum {
    TLP_SYSTEM_50OHM = 0,     /**< Standard 50 Ω TLP system */
    TLP_SYSTEM_VFTLP = 1,     /**< Very-Fast TLP (1-10 ns pulse) */
    TLP_SYSTEM_HMM = 2,       /**< Human Metal Model system */
    TLP_SYSTEM_CUSTOM = 3     /**< Custom impedance system */
} esd_tlp_system_type_t;

/**
 * @brief TLP system parameters
 *
 * L1 Definition: Core parameters defining a TLP measurement system.
 */
typedef struct {
    esd_tlp_system_type_t type;   /**< System type */
    double z0_ohm;                /**< Characteristic impedance [Ω] */
    double pulse_width_ns;        /**< Pulse width (FWHM) [ns] */
    double rise_time_ns;          /**< Pulse rise time (10%-90%) [ns] */
    double max_voltage_kv;        /**< Maximum charging voltage [kV] */
    double cable_length_m;        /**< Transmission line length [m] */
    double v_prop_factor;         /**< Velocity factor (0.66 for RG-58) */
} esd_tlp_system_t;

/**
 * @brief Single TLP measurement data point
 *
 * Represents one pulse measurement at a given charging voltage.
 * Averaged over the measurement window (typically 70%-90% of pulse).
 */
typedef struct {
    double v_charge_kv;       /**< Charging voltage [kV] */
    double v_dut_v;           /**< Measured voltage at DUT [V] */
    double i_dut_a;           /**< Measured current through DUT [A] */
    double r_dut_ohm;         /**< Calculated DUT resistance = V/I [Ω] */
    double power_w;           /**< Power dissipated in DUT [W] */
    double v_incident_v;      /**< Incident voltage amplitude [V] */
    double v_reflected_v;     /**< Reflected voltage amplitude [V] */
    double leakage_pre_na;    /**< DC leakage before pulse [nA] */
    double leakage_post_na;   /**< DC leakage after pulse [nA] */
    int failure_flag;         /**< 1 if DUT failure detected */
} esd_tlp_point_t;

/**
 * @brief Complete TLP I-V characteristic
 *
 * L2 Core Concept: The TLP I-V curve reveals key protection
 * device parameters:
 *   - Trigger voltage V_t1 (snapback point)
 *   - Holding voltage V_h (post-snapback)
 *   - On-resistance R_on (slope in on-state)
 *   - Failure current I_t2 (thermal failure point)
 *   - Failure voltage V_t2
 */
typedef struct {
    esd_tlp_point_t *points;     /**< Array of TLP measurement points */
    size_t num_points;           /**< Number of measurement points */
    esd_tlp_system_t system;     /**< TLP system configuration */

    /* Extracted parameters */
    double v_t1_v;               /**< Trigger voltage [V] */
    double i_t1_a;               /**< Trigger current [A] */
    double v_h_v;                /**< Holding voltage [V] */
    double i_h_a;                /**< Holding current [A] */
    double r_on_ohm;             /**< On-resistance (dynamic) [Ω] */
    double i_t2_a;               /**< Failure current (thermal) [A] */
    double v_t2_v;               /**< Failure voltage [V] */
    double v_leakage_at_fail_v;  /**< Leakage voltage at failure onset */
    int has_snapback;            /**< 1 if device exhibits snapback */

    /* Leakage evolution */
    double *leakage_pre_na;      /**< Pre-pulse leakage array [nA] */
    double *leakage_post_na;     /**< Post-pulse leakage array [nA] */
} esd_tlp_curve_t;

/**
 * @brief VFTLP (Very Fast TLP) parameters
 *
 * L8 Advanced Topic: VFTLP uses very short pulses (1-10 ns) with
 * sub-nanosecond rise times to characterize:
 *   - CDM-like failure modes
 *   - Turn-on time of protection devices
 *   - Transient behavior at the device level
 */
typedef struct {
    double pulse_width_ns;       /**< VFTLP pulse width [ns] (typically 1-10) */
    double rise_time_ps;         /**< Rise time [ps] (typically 50-200) */
    double measurement_window_ps; /**< Averaging window [ps] */
} esd_vftlp_params_t;

/**
 * @brief SEED (System-Efficient ESD Design) methodology parameters
 *
 * L8 Advanced Topic: SEED is a co-design methodology developed by
 * Industry Council on ESD Target Levels. It optimizes system-level
 * ESD protection by matching on-chip and off-chip protection.
 *
 * Key concept: The total ESD current is shared between:
 *   - On-chip protection (clamps, diodes)
 *   - Off-chip protection (TVS, varistors)
 *   - PCB parasitics (traces, vias, planes)
 *
 * The SEED simulation predicts the current distribution and ensures
 * neither protection element is overloaded.
 */
typedef struct {
    double i_esd_total_a;        /**< Total ESD current at system level [A] */
    double r_pcb_trace_ohm;      /**< PCB trace resistance in discharge path */
    double l_pcb_trace_nh;       /**< PCB trace inductance */
    double r_onchip_clamp_ohm;   /**< On-chip clamp dynamic resistance */
    double v_onchip_clamp_v;     /**< On-chip clamp voltage */
    double r_offchip_tvs_ohm;    /**< Off-chip TVS dynamic resistance */
    double v_offchip_clamp_v;    /**< Off-chip TVS clamp voltage */
    double i_onchip_max_a;       /**< Maximum on-chip current capability */
    double i_offchip_max_a;      /**< Maximum off-chip current capability */
} esd_seed_params_t;

/**
 * @brief SEED simulation result
 */
typedef struct {
    double i_onchip_a;           /**< Current through on-chip protection [A] */
    double i_offchip_a;          /**< Current through off-chip protection [A] */
    double v_node_v;             /**< Voltage at the protected node [V] */
    double onchip_margin_pct;    /**< On-chip current margin [%] */
    double offchip_margin_pct;   /**< Off-chip current margin [%] */
    int onchip_safe;             /**< 1 if on-chip within limits */
    int offchip_safe;            /**< 1 if off-chip within limits */
    int overall_safe;            /**< 1 if overall design is safe */
} esd_seed_result_t;

/* ─── API: TLP ───────────────────────────────────────────────── */

/**
 * @brief Initialize standard 100 ns TLP system parameters.
 *
 * Uses typical values for a commercial TLP system (Barth 4002 or similar):
 *   Z₀ = 50 Ω
 *   τ = 100 ns
 *   t_rise = 200 ps
 *   V_max = 4 kV
 */
esd_tlp_system_t esd_tlp_system_standard(void);

/**
 * @brief Initialize VFTLP system parameters.
 */
esd_tlp_system_t esd_tlp_system_vftlp(void);

/**
 * @brief Compute the TLP pulse width from cable length.
 *
 * L4 Theorem: τ_pulse = 2 × l_cable / (v_prop × c)
 * where v_prop is the velocity factor and c = 3×10⁸ m/s.
 *
 * @param cable_length_m   Cable length [m]
 * @param v_prop_factor    Velocity factor (0.66 typical for RG-58)
 * @return                 Pulse width [ns]
 */
double esd_tlp_pulse_width_from_length(double cable_length_m,
                                        double v_prop_factor);

/**
 * @brief Compute required cable length for a desired pulse width.
 */
double esd_tlp_cable_length_for_width(double pulse_width_ns,
                                       double v_prop_factor);

/**
 * @brief Compute the expected DUT current for a matched TLP system.
 *
 * L4 Theorem: For a matched DUT (R_DUT = Z₀):
 *   I_DUT = V_charge / (2 × Z₀)
 *
 * For mismatched DUT:
 *   I_DUT = V_charge / (Z₀ + R_DUT)
 *
 * @param v_charge_kv  Charging voltage [kV]
 * @param r_dut_ohm    DUT resistance [Ω]
 * @param z0_ohm       System impedance [Ω]
 * @return             Expected DUT current [A]
 */
double esd_tlp_expected_current(double v_charge_kv,
                                 double r_dut_ohm,
                                 double z0_ohm);

/**
 * @brief Compute the energy delivered by a single TLP pulse.
 *
 * E = V_DUT × I_DUT × τ_pulse
 *
 * @param v_dut_v         DUT voltage [V]
 * @param i_dut_a         DUT current [A]
 * @param pulse_width_ns  Pulse width [ns]
 * @return                Energy [µJ]
 */
double esd_tlp_pulse_energy(double v_dut_v,
                              double i_dut_a,
                              double pulse_width_ns);

/**
 * @brief Add a TLP measurement point to a TLP curve.
 *
 * Dynamically extends the points array.
 */
int esd_tlp_curve_add_point(esd_tlp_curve_t *curve,
                             const esd_tlp_point_t *point);

/**
 * @brief Extract TLP I-V parameters from measurement data.
 *
 * L5 Algorithm: Automated extraction of V_t1, V_h, R_on, I_t2, V_t2
 * from the TLP I-V data set.
 *
 * Algorithm steps:
 *   1. Sort points by current
 *   2. Detect trigger point (abrupt voltage drop = snapback)
 *   3. Fit linear region for R_on (least squares)
 *   4. Detect failure point (leakage increase > 10× baseline)
 *
 * @param curve  TLP curve with measurement points
 * @return       0 on success, -1 if insufficient data
 */
int esd_tlp_extract_params(esd_tlp_curve_t *curve);

/**
 * @brief Detect snapback in TLP I-V curve.
 *
 * L2 Core Concept: Snapback (also called "snapback breakdown"
 * or "negative resistance region") is the abrupt voltage drop
 * when a protection device triggers into its low-impedance state.
 *
 * Detection criteria:
 *   V(I_k) > V(I_{k+1})  while I_k < I_{k+1}
 *
 * @param curve  TLP curve
 * @return       1 if snapback detected, 0 otherwise
 */
int esd_tlp_detect_snapback(const esd_tlp_curve_t *curve);

/**
 * @brief Compute the figure of merit (FOM) for a TLP-characterized
 * protection device.
 *
 * L5 Algorithm: FOM = I_t2 / (C_j × V_h)
 * Higher FOM = better protection (more current handling per unit
 * capacitance and clamping voltage).
 *
 * @param curve       TLP curve with extracted parameters
 * @param c_j_pf      Device junction capacitance [pF]
 * @return            Figure of merit [A/(pF·V)]
 */
double esd_tlp_figure_of_merit(const esd_tlp_curve_t *curve,
                                double c_j_pf);

/**
 * @brief Perform SEED analysis for a system-level ESD protection design.
 *
 * L8 Advanced Topic: System-Efficient ESD Design (SEED) simulation.
 *
 * The key insight is that ESD current divides between parallel paths
 * according to their impedances. SEED models this current division
 * and verifies that each protection element stays within its safe
 * operating area.
 *
 * @param seed     SEED design parameters
 * @param[out] result  Simulation result
 * @return         0 on success
 */
int esd_seed_analyze(const esd_seed_params_t *seed,
                      esd_seed_result_t *result);

/**
 * @brief Compute optimum PCB trace impedance for SEED current balancing.
 *
 * L8: The series impedance between off-chip TVS and on-chip clamp
 * determines current sharing. SEED optimizes this impedance.
 *
 * @param v_clamp_diff  Difference between off-chip and on-chip clamp voltages [V]
 * @param i_esd_total   Total ESD current [A]
 * @return              Recommended series resistance [Ω]
 */
double esd_seed_optimal_impedance(double v_clamp_diff, double i_esd_total);

/**
 * @brief Free TLP curve memory.
 */
void esd_tlp_curve_free(esd_tlp_curve_t *curve);

/**
 * @brief Human Metal Model (HMM) parameters.
 *
 * L8: HMM is a hybrid method that uses an IEC 61000-4-2 gun
 * connected to a 50 Ω environment for device-level testing.
 */
typedef struct {
    double v_gun_kv;           /**< ESD gun voltage [kV] */
    double current_50ohm_a;    /**< Current into 50 Ω load [A] */
    double peak_current_a;     /**< Measured peak current [A] */
    double rise_time_ns;       /**< Measured rise time [ns] */
} esd_hmm_measurement_t;

/**
 * @brief Compute equivalent HMM current from IEC gun voltage.
 *
 * L8: HMM current (into 50 Ω) ≈ V_gun × (50 / (330 + 50)) ≈ 0.132 × V_gun
 *
 * @param v_gun_kv  ESD gun voltage [kV]
 * @return          Expected current into 50 Ω [A]
 */
double esd_hmm_equivalent_current(double v_gun_kv);

#ifdef __cplusplus
}
#endif

#endif /* ESD_TLP_H */
