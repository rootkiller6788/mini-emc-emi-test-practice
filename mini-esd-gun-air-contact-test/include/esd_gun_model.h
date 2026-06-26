/**
 * @file esd_gun_model.h
 * @brief ESD gun equivalent circuit model and RLC network analysis
 *
 * Implements the IEC 61000-4-2 ESD simulator equivalent circuit,
 * including lumped-element RLC models, parasitic extraction,
 * and transmission line effects inside the gun.
 *
 * Reference: IEC 61000-4-2:2008 Annex B (ESD generator calibration)
 *
 * Knowledge coverage:
 *   L1 - Definitions: ESD gun energy storage capacitor, discharge resistor
 *   L2 - Core Concepts: RLC resonant discharge, parasitic inductance effects
 *   L3 - Math Structures: RLC differential equations, state-space form
 *   L4 - Fundamental Laws: Energy conservation, Kirchhoff's laws
 */

#ifndef ESD_GUN_MODEL_H
#define ESD_GUN_MODEL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── ESD Gun Equivalent Circuit Elements ──────────────────────────
 *
 * L1 Definition: The IEC 61000-4-2 ESD simulator consists of:
 *
 *   C_s  = 150 pF  (energy storage capacitor)
 *   R_d  = 330 Ω   (discharge resistor)
 *
 * Plus parasitic elements:
 *   L_par = 0.05-2.0 µH (parasitic inductance of discharge path)
 *   C_par = 1-10 pF      (parasitic capacitance to ground)
 *   R_arc = arc resistance (nonlinear, varies during discharge)
 *
 * The classic simplified model is a series RLC circuit:
 *   C_s → R_d → L_par → R_load
 *
 * L2 Concept: The parasitic inductance L_par is critical because
 * it determines the rise time of the discharge current. Even small
 * inductance values (nH range) significantly affect the ns-scale
 * waveform shape.
 */

/** ESD gun model type */
typedef enum {
    ESD_GUN_MODEL_SIMPLE_RC = 0,     /**< Simplified RC model (150pF + 330Ω) */
    ESD_GUN_MODEL_SERIES_RLC = 1,    /**< Series RLC with parasitic L */
    ESD_GUN_MODEL_FULL_PARASITIC = 2, /**< Full model with all parasitics */
    ESD_GUN_MODEL_TLP_EQUIVALENT = 3 /**< TLP-equivalent model */
} esd_gun_model_type_t;

/**
 * @brief ESD gun simplified RC model parameters
 *
 * L1 Definition: Human Body Model (HBM) equivalent circuit.
 * The HBM standard (ANSI/ESDA/JEDEC JS-001) uses:
 *   C_body = 100 pF, R_body = 1500 Ω (different from IEC gun!)
 *
 * Note: IEC 61000-4-2 gun uses 150 pF / 330 Ω, which differs
 * from the classic HBM (100 pF / 1500 Ω) used for device-level testing.
 */
typedef struct {
    double cs_pf;    /**< Energy storage capacitance [pF], nominal 150 pF */
    double rd_ohm;   /**< Discharge resistance [Ω], nominal 330 Ω */
} esd_gun_rc_params_t;

/**
 * @brief ESD gun series RLC model parameters
 *
 * L3 Mathematical Structure: Series RLC circuit
 *
 * The discharge current i(t) satisfies the second-order ODE:
 *
 *   L * d²i/dt² + R * di/dt + i/C = 0
 *
 * with initial conditions:
 *   v_c(0) = V_charge, i(0) = 0
 *
 * Three damping regimes (L2 Concept):
 *   - Underdamped:  R < 2√(L/C)  → oscillatory with ringing
 *   - Critically damped: R = 2√(L/C)  → fastest non-oscillatory
 *   - Overdamped:   R > 2√(L/C)  → slow monotonic decay
 */
typedef struct {
    double cs_pf;      /**< Capacitance [pF] */
    double rd_ohm;     /**< Series resistance [Ω] */
    double l_par_nh;   /**< Parasitic inductance [nH] */
    double r_load_ohm; /**< Load resistance [Ω] */
    double v_charge_kv; /**< Initial charging voltage [kV] */
} esd_gun_rlc_params_t;

/**
 * @brief Full parasitic ESD gun model
 *
 * Includes:
 *   - Mutual inductance to ground plane
 *   - Stray capacitance to ground
 *   - Arc resistance model
 */
typedef struct {
    double cs_pf;          /**< Energy storage capacitor [pF] */
    double rd_ohm;         /**< Discharge resistor [Ω] */
    double l_self_nh;      /**< Self-inductance of discharge path [nH] */
    double l_mutual_nh;    /**< Mutual inductance to ground plane [nH] */
    double c_stray_pf;     /**< Stray capacitance to ground [pF] */
    double r_ground_ohm;   /**< Ground return path resistance [Ω] */
    double l_ground_nh;    /**< Ground return path inductance [nH] */
} esd_gun_full_params_t;

/**
 * @brief Arc resistance models for air discharge
 *
 * L2 Core Concept: During air discharge, the spark gap initially
 * has near-infinite resistance, which collapses to a very low
 * value during the arc phase. Several empirical models exist.
 */
typedef enum {
    ARC_MODEL_ROMWE_WEIZEL = 0, /**< Rompe-Weizel spark resistance law */
    ARC_MODEL_TOEPLER = 1,      /**< Toepler's spark law */
    ARC_MODEL_BARANNIK = 2,     /**< Barannik model */
    ARC_MODEL_CONSTANT = 3      /**< Constant arc resistance approximation */
} arc_model_type_t;

/**
 * @brief Rompe-Weizel spark resistance parameters
 *
 * L3: R_arc(t) = d / sqrt( 2α * ∫₀ᵗ i²(τ) dτ / p )
 *
 * where:
 *   d = gap length [m]
 *   α = spark constant ≈ 0.5-1.0 × 10⁻⁴ m²/(V²·s)
 *   p = pressure (atmospheric ≈ 1 bar)
 *   ∫ i² dτ = accumulated energy integral
 */
typedef struct {
    double gap_length_mm;    /**< Spark gap length [mm] */
    double alpha_coeff;      /**< Rompe-Weizel spark constant [m²/(V²·s)] */
    double pressure_bar;     /**< Gas pressure [bar], ~1 for air at STP */
} arc_rompe_weizel_t;

/**
 * @brief Toepler's spark resistance parameters
 *
 * L3: R_arc(t) = k_T * d / ∫₀ᵗ i(τ) dτ
 *
 * where:
 *   k_T = Toepler constant ≈ 4 × 10⁻⁵ V·s/m
 *   d   = gap length
 *   ∫ i dτ = accumulated charge
 */
typedef struct {
    double gap_length_mm;    /**< Spark gap length [mm] */
    double k_toepler;        /**< Toepler constant [V·s/m], ~4×10⁻⁵ */
} arc_toepler_t;

/**
 * @brief State of the ESD gun discharge at an instant in time
 */
typedef struct {
    double v_cap_kv;         /**< Capacitor voltage [kV] */
    double i_discharge_a;    /**< Discharge current [A] */
    double di_dt_aps;        /**< Time derivative of current [A/ns] */
    double r_arc_ohm;        /**< Instantaneous arc resistance [Ω] */
    double energy_joules;    /**< Energy dissipated so far [J] */
    double charge_nc;        /**< Charge transferred so far [nC] */
} esd_gun_discharge_state_t;

/**
 * @brief Full solution trajectory for an ESD gun discharge
 */
typedef struct {
    double *time_ns;         /**< Time array */
    double *voltage_kv;      /**< Capacitor voltage vs time */
    double *current_a;       /**< Discharge current vs time */
    double *arc_resistance_ohm; /**< Arc resistance vs time */
    size_t num_points;       /**< Number of trajectory points */
} esd_gun_trajectory_t;

/* ─── API: ESD gun model computation ──────────────────────────── */

/**
 * @brief Initialize standard IEC 61000-4-2 gun RC parameters.
 *
 * Returns the nominal values: C_s = 150 pF, R_d = 330 Ω.
 * L1 Definition: Reference values for the ESD simulator.
 */
esd_gun_rc_params_t esd_gun_rc_default(void);

/**
 * @brief Initialize standard RLC parameters for IEC 61000-4-2 gun.
 *
 * Uses typical parasitic inductance values for a compliant gun:
 * L_par ≈ 0.05-0.10 µH for contact discharge.
 */
esd_gun_rlc_params_t esd_gun_rlc_default(double v_charge_kv);

/**
 * @brief Initialize full parasitic model parameters.
 */
esd_gun_full_params_t esd_gun_full_default(void);

/**
 * @brief Compute natural frequency and damping factor of RLC circuit.
 *
 * L3 Mathematical Structure:
 *   ω₀ = 1/√(LC)          (undamped natural frequency)
 *   ζ  = R/(2) * √(C/L)   (damping ratio)
 *   ω_d = ω₀√(1-ζ²)       (damped frequency, if underdamped)
 *
 * @param params   RLC parameters
 * @param[out] omega0  Natural frequency ω₀ [rad/s]
 * @param[out] zeta    Damping ratio ζ
 * @param[out] omega_d Damped frequency [rad/s]
 */
void esd_gun_rlc_natural(const esd_gun_rlc_params_t *params,
                          double *omega0, double *zeta, double *omega_d);

/**
 * @brief Classify damping regime of the RLC circuit.
 *
 * @return  0 = underdamped, 1 = critically damped, 2 = overdamped
 */
int esd_gun_rlc_damping_regime(const esd_gun_rlc_params_t *params);

/**
 * @brief Compute the theoretical peak current for an RLC discharge.
 *
 * L4 Theorem: For an underdamped series RLC:
 *   I_peak ≈ V₀ / √(L/C) * exp(-ζ·arccos(ζ) / √(1-ζ²))
 *
 * For critically damped:
 *   I_peak = V₀ / (e·R) ≈ V₀ / (2.718·R)
 *
 * @param params  RLC parameters
 * @return        Theoretical peak current [A]
 */
double esd_gun_rlc_peak_current(const esd_gun_rlc_params_t *params);

/**
 * @brief Compute theoretical rise time for RLC discharge.
 *
 * Approximate 10%-90% rise time:
 *   t_rise ≈ 2.2 * L / R   (for overdamped)
 *   t_rise ≈ π / (2·ω₀)    (for underdamped, neglecting damping)
 *
 * @param params  RLC parameters
 * @return        Estimated rise time [ns]
 */
double esd_gun_rlc_rise_time(const esd_gun_rlc_params_t *params);

/**
 * @brief Simulate RLC discharge using fourth-order Runge-Kutta (RK4).
 *
 * L5 Algorithm: RK4 integration of the coupled first-order ODEs:
 *   dVc/dt = -I/C
 *   dI/dt  = (Vc - I·R_total) / L
 *
 * @param params   RLC circuit parameters
 * @param dt_ns    Time step [ns]
 * @param t_end_ns  Simulation end time [ns]
 * @param[out] traj  Output trajectory (caller frees via esd_gun_trajectory_free)
 * @return         0 on success, -1 on error
 */
int esd_gun_simulate_rlc_rk4(const esd_gun_rlc_params_t *params,
                              double dt_ns,
                              double t_end_ns,
                              esd_gun_trajectory_t *traj);

/**
 * @brief Simulate using analytical solution (when arc resistance is
 * constant or linearized).
 *
 * Uses the closed-form solution for series RLC discharge.
 * Applicable when arc resistance can be treated as constant.
 */
int esd_gun_simulate_rlc_analytical(const esd_gun_rlc_params_t *params,
                                     double t_end_ns,
                                     size_t num_points,
                                     esd_gun_trajectory_t *traj);

/**
 * @brief Simulate with time-varying arc resistance (Rompe-Weizel).
 *
 * L6 Canonical Problem: Air discharge simulation with nonlinear
 * spark resistance. Uses RK4 with arc resistance updated at each
 * time step based on accumulated energy.
 *
 * @param params   RLC parameters
 * @param arc      Rompe-Weizel arc parameters
 * @param dt_ns    Time step
 * @param t_end_ns Simulation end time
 * @param[out] traj  Output trajectory
 * @return         0 on success
 */
int esd_gun_simulate_with_arc_rw(const esd_gun_rlc_params_t *params,
                                  const arc_rompe_weizel_t *arc,
                                  double dt_ns,
                                  double t_end_ns,
                                  esd_gun_trajectory_t *traj);

/**
 * @brief Simulate with Toepler arc model.
 */
int esd_gun_simulate_with_arc_toepler(const esd_gun_rlc_params_t *params,
                                       const arc_toepler_t *arc,
                                       double dt_ns,
                                       double t_end_ns,
                                       esd_gun_trajectory_t *traj);

/**
 * @brief Compute initial arc resistance from Paschen's law.
 *
 * L4 Theorem: Paschen's law gives the breakdown voltage V_bd as
 * a function of p·d product:
 *
 *   V_bd = B·p·d / ln(A·p·d) - ln(ln(1 + 1/γ))
 *
 * For air at STP:
 *   A ≈ 10.95 (Pa·m)⁻¹, B ≈ 273.8 V/(Pa·m), γ ≈ 0.01-0.1
 *
 * Simplified approximation for air:
 *   V_bd [V] ≈ 3000 + 1000·d [mm] (for d = 1-10 mm at 1 atm)
 *
 * @param gap_length_mm  Electrode gap [mm]
 * @param pressure_bar   Gas pressure [bar]
 * @return               Breakdown voltage [V]
 */
double esd_paschen_breakdown_voltage(double gap_length_mm,
                                      double pressure_bar);

/**
 * @brief Estimate arc length from breakdown voltage.
 *
 * Inverse of Paschen's law approximation.
 * Useful for: given a measured ESD voltage, estimate the
 * equivalent air gap during testing.
 *
 * @param voltage_kv  Discharge voltage [kV]
 * @param pressure_bar Pressure [bar]
 * @return            Estimated gap length [mm]
 */
double esd_voltage_to_gap_length(double voltage_kv, double pressure_bar);

/**
 * @brief Compute stored energy in the ESD gun capacitor.
 *
 * E = ½ C V²
 *
 * Example: 150 pF @ 8 kV → E = ½·150e-12·(8000)² = 4.8 mJ
 *
 * @param cs_pf       Capacitance [pF]
 * @param voltage_kv  Charging voltage [kV]
 * @return            Stored energy [µJ]
 */
double esd_gun_stored_energy(double cs_pf, double voltage_kv);

/**
 * @brief Compute effective impedance of the RLC circuit.
 *
 * Z_eff = √(R² + (ωL - 1/(ωC))²)
 *
 * @param params   RLC parameters
 * @param freq_hz  Frequency of interest [Hz]
 * @return         Impedance magnitude [Ω]
 */
double esd_gun_impedance(const esd_gun_rlc_params_t *params, double freq_hz);

/**
 * @brief Transfer function H(s) of the ESD gun circuit.
 *
 * L3: H(s) = V_out(s) / V_in(s) for the series RLC with resistive load.
 * Evaluates |H(jω)| at given frequency.
 */
double esd_gun_transfer_magnitude(const esd_gun_rlc_params_t *params,
                                   double freq_hz);

/**
 * @brief Free trajectory memory.
 */
void esd_gun_trajectory_free(esd_gun_trajectory_t *traj);

/**
 * @brief Print RLC parameters to string.
 */
int esd_gun_rlc_print(const esd_gun_rlc_params_t *params,
                       char *buffer, size_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* ESD_GUN_MODEL_H */
