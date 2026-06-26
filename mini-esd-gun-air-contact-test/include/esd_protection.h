/**
 * @file esd_protection.h
 * @brief ESD protection device modeling and selection
 *
 * Implements models for common ESD protection components:
 *   - TVS diodes (Transient Voltage Suppressor)
 *   - Varistors (MOV - Metal Oxide Varistor)
 *   - Polymer ESD suppressors
 *   - RC filters for ESD
 *   - Ferrite beads for ESD suppression
 *
 * Reference: ANSI/ESDA/JEDEC JS-001 (HBM), IEC 61000-4-2
 *
 * Knowledge coverage:
 *   L1 - Definitions: TVS clamping voltage, breakdown voltage, leakage
 *   L2 - Core Concepts: ESD protection window, snapback, latch-up
 *   L4 - Fundamental Laws: Power dissipation limits, thermal failure
 *   L5 - Algorithms: TVS selection algorithm
 */

#ifndef ESD_PROTECTION_H
#define ESD_PROTECTION_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── ESD Protection Device Types ──────────────────────────────────
 *
 * L1 Definition: ESD protection devices are nonlinear components
 * that provide a low-impedance path to ground during an ESD event
 * while maintaining high impedance during normal operation.
 */

/** ESD protection device category */
typedef enum {
    ESD_PROT_TVS_UNIDIR = 0,     /**< Unidirectional TVS diode */
    ESD_PROT_TVS_BIDIR = 1,      /**< Bidirectional TVS diode */
    ESD_PROT_VARISTOR = 2,       /**< Metal Oxide Varistor (MOV) */
    ESD_PROT_POLYMER = 3,        /**< Polymer voltage suppressor */
    ESD_PROT_GAS_DISCHARGE = 4,  /**< Gas discharge tube (GDT) */
    ESD_PROT_RC_FILTER = 5,      /**< RC low-pass filter */
    ESD_PROT_COMMON_MODE = 6,    /**< Common-mode choke */
    ESD_PROT_SCHOTTKY = 7        /**< Schottky diode clamp to rails */
} esd_protection_type_t;

/**
 * @brief TVS diode electrical parameters
 *
 * L1 Definition: Key TVS diode parameters per datasheet.
 *
 * V_RWM  = Reverse Working Maximum voltage (standoff)
 * V_BR   = Breakdown voltage (diode starts conducting)
 * V_C    = Clamping voltage (at specified peak pulse current I_PP)
 * I_PP   = Peak pulse current rating
 * P_PK   = Peak pulse power rating
 * C_j    = Junction capacitance (important for signal integrity)
 */
typedef struct {
    double vrwm_v;          /**< Reverse working max voltage [V] */
    double vbr_min_v;       /**< Minimum breakdown voltage [V] */
    double vbr_max_v;       /**< Maximum breakdown voltage [V] */
    double vc_v;            /**< Clamping voltage at Ipp [V] */
    double ipp_a;           /**< Peak pulse current rating [A] */
    double ppk_w;           /**< Peak pulse power (8/20 µs) [W] */
    double cj_pf;           /**< Junction capacitance [pF] */
    double leakage_na;      /**< Reverse leakage current [nA] */
    double dynamic_r_ohm;   /**< Dynamic resistance in breakdown [Ω] */
    esd_protection_type_t subtype; /**< Unidirectional or bidirectional */
} esd_tvs_params_t;

/**
 * @brief Varistor (MOV) electrical parameters
 *
 * L2 Concept: Varistors follow the power-law I-V characteristic:
 *   I = k · V^α
 *
 * where α is the nonlinearity exponent (typically 20-60 for ZnO varistors).
 * For SiC varistors, α ≈ 3-7.
 */
typedef struct {
    double v1ma_v;          /**< Voltage at 1 mA reference current [V] */
    double vc_v;            /**< Clamping voltage at rated current [V] */
    double energy_rating_j; /**< Maximum energy absorption [J] */
    double peak_current_a;  /**< Peak current rating (8/20 µs) [A] */
    double capacitance_pf;  /**< Typical capacitance [pF] */
    double alpha;           /**< Nonlinearity exponent (20-60 for ZnO) */
    double k_constant;      /**< I-V multiplier constant */
} esd_varistor_params_t;

/**
 * @brief Gas Discharge Tube (GDT) parameters
 *
 * L2 Concept: GDTs use a gas-filled gap that ionizes at the spark-over
 * voltage, creating a low-impedance plasma path. They handle very
 * high currents but have slow turn-on time (~100 ns - 1 µs).
 */
typedef struct {
    double dc_sparkover_v;     /**< DC spark-over voltage [V] */
    double impulse_sparkover_v; /**< Impulse spark-over (100V/µs) [V] */
    double holdover_voltage_v;  /**< Arc voltage during conduction [V] */
    double impulse_current_ka;  /**< Impulse current rating (8/20µs) [kA] */
    double capacitance_pf;      /**< Capacitance [pF] */
    double insulation_resistance_gohm; /**< Insulation resistance [GΩ] */
} esd_gdt_params_t;

/**
 * @brief RC ESD filter parameters
 *
 * L5 Algorithm: RC low-pass filter design for ESD suppression.
 * Cutoff frequency: fc = 1/(2πRC)
 * Must be placed before sensitive IC pins.
 */
typedef struct {
    double r_series_ohm;    /**< Series resistance [Ω] */
    double c_shunt_pf;      /**< Shunt capacitance to ground [pF] */
    double fc_mhz;          /**< -3 dB cutoff frequency [MHz] */
    double max_signal_freq_mhz; /**< Maximum signal frequency [MHz] */
} esd_rc_filter_params_t;

/**
 * @brief ESD protection window concept
 *
 * L2 Core Concept: The ESD protection window is the safe operating
 * voltage range where:
 *   - Upper bound: V_max of protected IC (typically Vcc + 10-20%)
 *   - Lower bound: V_signal_max (maximum normal signal voltage)
 *
 * The protection device must:
 *   1. Not conduct at V_signal_max (VRWM > V_signal_max)
 *   2. Clamp below V_ic_max during ESD (Vc @ Ipp < V_ic_max)
 *   3. Survive the ESD current level (Ipp > I_esd_peak)
 */
typedef struct {
    double v_signal_max_v;   /**< Maximum normal signal voltage [V] */
    double v_ic_max_v;       /**< IC maximum rated voltage [V] */
    double v_supply_v;       /**< Supply rail voltage [V] */
    double esd_peak_current_a; /**< Expected ESD peak current [A] */
    double signal_bandwidth_mhz; /**< Signal bandwidth [MHz] */
} esd_protection_window_t;

/**
 * @brief TVS selection result
 */
typedef struct {
    int suitable;              /**< 1 if TVS meets all requirements */
    double vrwm_margin_v;     /**< VRWM margin above V_signal_max [V] */
    double vc_margin_v;       /**< V_ic_max - VC margin [V] */
    double ipp_margin_a;      /**< IPP margin above ESD peak [A] */
    double bandwidth_margin_mhz; /**< Signal bandwidth margin [MHz] */
    double clamp_ratio;       /**< VC / V_signal_max (lower is better) */
    char recommendation[256]; /**< Selection recommendation text */
} esd_tvs_selection_t;

/**
 * @brief ESD protection network
 *
 * L6 Canonical Problem: Multi-stage ESD protection design.
 * Typical configuration: GDT → series R → TVS → ferrite → IC
 */
typedef struct {
    esd_protection_type_t primary_type;    /**< Primary protection type */
    esd_tvs_params_t tvs_primary;          /**< Primary TVS parameters */
    double series_resistance_ohm;          /**< Series current-limiting R */
    double series_inductance_nh;           /**< Series ferrite bead L */
    esd_protection_type_t secondary_type;  /**< Secondary protection type */
    esd_tvs_params_t tvs_secondary;        /**< Secondary/on-chip TVS */
    esd_rc_filter_params_t rc_filter;      /**< Optional RC filter */
    double total_clamp_voltage_v;          /**< Overall clamping voltage */
    double response_time_ns;               /**< Protection response time */
} esd_protection_network_t;

/* ─── TVS Diode I-V Model ─────────────────────────────────────────
 *
 * L3 Mathematical Structure: TVS diode piecewise model
 *
 * Region 1 (VRWM to VBR):  I ≈ I_leakage (near-zero conduction)
 * Region 2 (VBR to VC):    I = I_leakage + (V-VBR)/R_dynamic
 *                           (exponential turn-on)
 * Region 3 (beyond VC):    V ≈ VC + R_dynamic × I
 *                           (linear ohmic region)
 *
 * L4 Theorem: Power dissipation limit
 * For a rectangular pulse of duration τ:
 *   P_avg = P_peak × τ / T_period
 *
 * For single-shot ESD (8 kV contact), τ ≈ 100 ns,
 * I_peak ≈ 30 A, Vclamp ≈ 15 V → P_peak ≈ 450 W
 */

/* ─── API: ESD Protection ────────────────────────────────────── */

/**
 * @brief Initialize default TVS parameters for a typical 5V protection diode.
 *
 * L1 Definition: Representative values for a USB/5V ESD protection TVS.
 */
esd_tvs_params_t esd_tvs_default_5v(void);

/**
 * @brief Initialize default TVS parameters for 3.3V protection.
 */
esd_tvs_params_t esd_tvs_default_3v3(void);

/**
 * @brief Initialize default varistor parameters.
 */
esd_varistor_params_t esd_varistor_default(void);

/**
 * @brief Compute TVS clamping voltage at a given surge current.
 *
 * Uses the dynamic resistance model:
 *   V_clamp(I) = V_BR + R_dyn × I
 *
 * For I < I_BR, returns V_RWM (no clamping).
 *
 * @param tvs      TVS parameters
 * @param current_a Surge current [A]
 * @return         Clamping voltage [V]
 */
double esd_tvs_clamp_voltage(const esd_tvs_params_t *tvs, double current_a);

/**
 * @brief Compute TVS power dissipation during an ESD pulse.
 *
 * P = V_clamp(I) × I
 *
 * @param tvs       TVS parameters
 * @param current_a ESD current [A]
 * @return          Instantaneous power [W]
 */
double esd_tvs_power(const esd_tvs_params_t *tvs, double current_a);

/**
 * @brief Compute TVS junction temperature rise during ESD pulse.
 *
 * ΔT = P_pulse × R_th × (1 - exp(-τ / τ_th))
 *
 * where:
 *   R_th = thermal resistance [°C/W]
 *   τ    = pulse duration [s]
 *   τ_th = thermal time constant [s]
 *
 * L4 Theorem: Adiabatic heating for very short pulses (τ << τ_th).
 * Then ΔT ≈ E_pulse / C_th where C_th = thermal capacitance.
 *
 * @param tvs              TVS parameters
 * @param current_a        Surge current [A]
 * @param pulse_duration_s Pulse duration [s]
 * @param rth_cw           Thermal resistance junction-ambient [°C/W]
 * @param cth_jpc          Thermal capacitance [J/°C]
 * @return                 Junction temperature rise [°C]
 */
double esd_tvs_temp_rise(const esd_tvs_params_t *tvs,
                          double current_a,
                          double pulse_duration_s,
                          double rth_cw,
                          double cth_jpc);

/**
 * @brief Select TVS for a given protection window.
 *
 * L5 Algorithm: TVS selection based on:
 *   1. VRWM > V_signal_max (safety margin 10-20%)
 *   2. VC < V_IC_max during ESD
 *   3. IPP > I_esd_peak
 *   4. C_j small enough for signal bandwidth
 *
 * @param window    Protection window requirements
 * @param tvs       TVS candidate to evaluate
 * @param[out] sel  Selection evaluation result
 * @return          0 on success
 */
int esd_tvs_select(const esd_protection_window_t *window,
                    const esd_tvs_params_t *tvs,
                    esd_tvs_selection_t *sel);

/**
 * @brief Compute the maximum signal frequency that a TVS can protect
 * without excessive signal degradation.
 *
 * L5 Algorithm: Bandwidth estimation from capacitance.
 * f_max ≈ 0.35 / (R_source × C_j)  (for digital signals)
 * f_max ≈ 1 / (2π × R_source × C_j)  (for analog signals, -3dB)
 *
 * @param tvs           TVS parameters
 * @param source_impedance Source impedance [Ω]
 * @param is_digital    1 for digital timing, 0 for analog bandwidth
 * @return              Maximum frequency [MHz]
 */
double esd_tvs_bandwidth_limit(const esd_tvs_params_t *tvs,
                                double source_impedance,
                                int is_digital);

/**
 * @brief Design an RC ESD filter.
 *
 * L5 Algorithm: Given signal bandwidth and ESD protection requirements,
 * compute optimal R and C values.
 *
 * @param max_signal_freq_mhz Maximum signal frequency [MHz]
 * @param source_impedance_ohm Source impedance [Ω]
 * @param esd_level_kv        ESD threat level [kV]
 * @param[out] filter         RC filter parameters
 * @return                    0 on success
 */
int esd_rc_filter_design(double max_signal_freq_mhz,
                          double source_impedance_ohm,
                          double esd_level_kv,
                          esd_rc_filter_params_t *filter);

/**
 * @brief Compute varistor voltage at a given current.
 *
 * L3: V = (I / k)^(1/α)
 *
 * @param varistor  Varistor parameters
 * @param current_a Current [A]
 * @return          Voltage [V]
 */
double esd_varistor_voltage(const esd_varistor_params_t *varistor,
                             double current_a);

/**
 * @brief Compute varistor clamp ratio (Vc @ 30A / V1mA).
 *
 * L6 Canonical Problem: Varistor protection ratio is a key metric
 * for ESD protection effectiveness. Lower ratio = better protection.
 */
double esd_varistor_clamp_ratio(const esd_varistor_params_t *varistor);

/**
 * @brief Compute multi-stage protection network performance.
 *
 * Simulates the response of a cascaded protection network.
 * L6 Canonical Problem: Two-stage (primary + secondary) ESD protection
 * with decoupling impedance between stages.
 *
 * @param network         Protection network definition
 * @param esd_current_a   Incident ESD current [A]
 * @param[out] v_at_ic    Residual voltage at protected IC [V]
 * @param[out] energy_primary_j  Energy absorbed by primary [J]
 * @param[out] energy_secondary_j Energy absorbed by secondary [J]
 * @return                0 on success
 */
int esd_network_simulate(const esd_protection_network_t *network,
                          double esd_current_a,
                          double *v_at_ic,
                          double *energy_primary_j,
                          double *energy_secondary_j);

/**
 * @brief Compare two TVS diodes and return the better one for
 * a given protection window.
 *
 * @param window   Protection window
 * @param tvs_a    Candidate A
 * @param tvs_b    Candidate B
 * @return         1 if A is better, -1 if B is better, 0 if equal
 */
int esd_tvs_compare(const esd_protection_window_t *window,
                     const esd_tvs_params_t *tvs_a,
                     const esd_tvs_params_t *tvs_b);

/**
 * @brief Calculate the required PCB trace width for ESD current
 * handling without excessive voltage drop or heating.
 *
 * L7 Application: PCB layout for ESD protection.
 *
 * @param peak_current_a  Peak ESD current [A]
 * @param copper_weight_oz Copper weight [oz]
 * @param allowed_drop_v   Allowed voltage drop [V]
 * @param trace_length_mm  Trace length [mm]
 * @return                 Minimum trace width [mm]
 */
double esd_pcb_trace_width(double peak_current_a,
                            double copper_weight_oz,
                            double allowed_drop_v,
                            double trace_length_mm);

#ifdef __cplusplus
}
#endif

#endif /* ESD_PROTECTION_H */
