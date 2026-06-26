/**
 * @file cm_dm_filter.h
 * @brief Common-Mode / Differential-Mode EMI Filter — Core Definitions & API
 *
 * This module covers the theory, modeling, design, and analysis of
 * common-mode (CM) and differential-mode (DM) electromagnetic interference
 * (EMI) filters, as used in power electronics, motor drives, communication
 * interfaces, and any system requiring conducted emission compliance.
 *
 * Reference Textbooks:
 *  - Paul, C.R. "Introduction to Electromagnetic Compatibility" (2006)
 *  - Ott, H.W. "Electromagnetic Compatibility Engineering" (2009)
 *  - Weston, D. "Electromagnetic Compatibility: Methods, Analysis, Circuits" (2017)
 *
 * Knowledge Coverage (see SKILL.md):
 *  L1: CM/DM voltage/current definitions, CMRR, insertion loss, cutoff freq
 *  L2: CM choke principle, filter topologies, Y/X capacitor roles
 *  L3: Complex impedance, transfer functions, s-domain analysis
 *  L4: Faraday's law (flux add/cancel), Kirchhoff in CM/DM circuits
 *  L5: Filter design algorithm, component selection, impedance matching
 *  L6: CM choke saturation, cross-coupling, resonance damping
 *  L7: AC-DC PSU filtering, motor drive CM filtering, USB/Ethernet CM chokes
 *  L8: Active CM filtering, integrated EMI filter, stochastic noise model
 *  L9: GaN/SiC wide-bandgap EMI, AI-based filter optimization
 */

#ifndef CM_DM_FILTER_H
#define CM_DM_FILTER_H

#include <stddef.h>
#include <stdint.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * L1: DEFINITIONS — Core data types for CM/DM filter analysis
 * ======================================================================== */

/** @brief Measurement point for a three-wire (L, N, PE) system */
typedef enum {
    MEAS_POINT_LINE = 0,      /**< Phase/Line conductor */
    MEAS_POINT_NEUTRAL = 1,   /**< Neutral conductor */
    MEAS_POINT_PE = 2,        /**< Protective Earth */
    MEAS_POINT_L1 = 3,        /**< Three-phase L1 */
    MEAS_POINT_L2 = 4,        /**< Three-phase L2 */
    MEAS_POINT_L3 = 5         /**< Three-phase L3 */
} cm_dm_meas_point_t;

/** @brief Time-domain voltage/current sample at a measurement point */
typedef struct {
    double voltage;           /**< Instantaneous voltage [V] */
    double current;           /**< Instantaneous current [A] */
    double time;              /**< Sample time [s] */
} cm_dm_sample_t;

/**
 * @brief Common-mode quantities derived from line measurements.
 *
 * L1 Definition: Common-mode (CM) voltage is the average of the voltages
 * on all conductors with respect to the reference plane (PE/GND).
 *
 *    V_cm = (V_L + V_N) / 2    (single-phase)
 *    I_cm = I_L + I_N           (sum returns via PE/ground)
 */
typedef struct {
    double voltage_cm;        /**< CM voltage [V] */
    double current_cm;        /**< CM current [A] */
    double impedance_cm;      /**< CM path impedance [Ω] */
    double frequency;         /**< Frequency of interest [Hz] */
} cm_quantities_t;

/**
 * @brief Differential-mode quantities derived from line measurements.
 *
 * L1 Definition: Differential-mode (DM) voltage is the voltage between
 * the line and neutral conductors.
 *
 *    V_dm = V_L - V_N
 *    I_dm = (I_L - I_N) / 2
 */
typedef struct {
    double voltage_dm;        /**< DM voltage [V] */
    double current_dm;        /**< DM current [A] */
    double impedance_dm;      /**< DM path impedance [Ω] */
    double frequency;         /**< Frequency of interest [Hz] */
} dm_quantities_t;

/**
 * @brief CM/DM decomposition result from raw line measurements.
 *
 * Given V_L, V_N and I_L, I_N, we decompose into CM and DM components.
 * This is the fundamental mathematical operation underlying all CM/DM
 * filter analysis (L1-L3).
 */
typedef struct {
    cm_quantities_t cm;       /**< Extracted CM quantities */
    dm_quantities_t dm;       /**< Extracted DM quantities */
    double v_line;            /**< Raw line voltage [V] */
    double v_neutral;         /**< Raw neutral voltage [V] */
    double i_line;            /**< Raw line current [A] */
    double i_neutral;         /**< Raw neutral current [A] */
} cm_dm_decomposition_t;

/* ========================================================================
 * L2: CORE CONCEPTS — Filter topology and stage definitions
 * ======================================================================== */

/** @brief Filter topology family */
typedef enum {
    FILTER_TOPOLOGY_L_SERIES = 0,   /**< Single series inductor */
    FILTER_TOPOLOGY_C_SHUNT = 1,    /**< Single shunt capacitor */
    FILTER_TOPOLOGY_LC = 2,         /**< L-C low-pass */
    FILTER_TOPOLOGY_CL = 3,         /**< C-L low-pass */
    FILTER_TOPOLOGY_PI = 4,         /**< π-filter: C-L-C */
    FILTER_TOPOLOGY_T = 5,          /**< T-filter: L-C-L */
    FILTER_TOPOLOGY_MULTISTAGE = 6, /**< Cascaded multiple stages */
    FILTER_TOPOLOGY_DOUBLE_PI = 7,  /**< Double π-section */
    FILTER_TOPOLOGY_CUSTOM = 8      /**< User-defined topology */
} filter_topology_t;

/** @brief Filter element type */
typedef enum {
    FILTER_ELEM_CM_CHOKE = 0,       /**< Common-mode choke */
    FILTER_ELEM_DM_INDUCTOR = 1,    /**< DM series inductor */
    FILTER_ELEM_X_CAPACITOR = 2,    /**< X-cap (line-to-neutral) */
    FILTER_ELEM_Y_CAPACITOR = 3,    /**< Y-cap (line-to-PE, safety) */
    FILTER_ELEM_FERRITE_BEAD = 4,   /**< Ferrite bead on wire */
    FILTER_ELEM_RESISTOR = 5,       /**< Damping resistor */
    FILTER_ELEM_TVS = 6,            /**< Transient voltage suppressor */
    FILTER_ELEM_FEEDTHROUGH = 7     /**< Feedthrough capacitor */
} filter_elem_type_t;

/**
 * @brief A single filter element with its parasitic model.
 *
 * Every real component has parasitic elements that limit its high-frequency
 * performance. An inductor has parallel winding capacitance (EPC) and series
 * resistance (ESR). A capacitor has equivalent series inductance (ESL) and
 * equivalent series resistance (ESR).
 */
typedef struct {
    filter_elem_type_t type;    /**< Element classification */
    double nominal_value;       /**< Nominal L [H] or C [F] or R [Ω] */
    double tolerance_pct;       /**< Component tolerance [%] */
    double esr;                 /**< Equivalent series resistance [Ω] */
    double esl;                 /**< Equivalent series inductance [H] (for C) */
    double epc;                 /**< Equivalent parallel capacitance [F] (for L) */
    double r_parallel;          /**< Parallel leakage resistance [Ω] */
    double self_resonant_freq;  /**< Self-resonant frequency [Hz] */
    double rated_voltage;       /**< Rated voltage [V] */
    double rated_current;       /**< Rated current [A] */
    double temperature_coef;    /**< Temperature coefficient [ppm/°C] */
} filter_element_t;

/**
 * @brief A single filter stage, which may contain multiple elements.
 *
 * A "stage" is one L-C pair or one CM choke with its Y-caps, etc.
 * Multi-stage filters cascade several stages for higher attenuation.
 */
typedef struct {
    filter_topology_t topology;     /**< Stage topology */
    filter_element_t *elements;     /**< Array of elements in this stage */
    size_t num_elements;            /**< Number of elements */
    double cutoff_freq_design;      /**< Designed cutoff frequency [Hz] */
    double insertion_loss_target;   /**< Target insertion loss [dB] at target freq */
    double target_frequency;        /**< Frequency for insertion loss target [Hz] */
    int is_cm_stage;                /**< Nonzero if this stage handles CM */
    int is_dm_stage;                /**< Nonzero if this stage handles DM */
} filter_stage_t;

/**
 * @brief Complete multi-stage CM/DM EMI filter design.
 */
typedef struct {
    filter_stage_t *stages;         /**< Array of filter stages */
    size_t num_stages;              /**< Number of stages */
    double cm_insertion_loss;       /**< Total CM IL [dB] at target frequency */
    double dm_insertion_loss;       /**< Total DM IL [dB] at target frequency */
    double cm_cutoff_freq;          /**< Overall CM cutoff frequency [Hz] */
    double dm_cutoff_freq;          /**< Overall DM cutoff frequency [Hz] */
    double rated_voltage;           /**< System rated voltage [V] */
    double rated_current;           /**< System rated current [A] */
    double power_loss;              /**< Estimated total power loss [W] */
    double cmrr_estimate;           /**< Estimated CMRR [dB] */
    int compliance_margin_db;       /**< Margin to EMC limit [dB] */
} cm_dm_filter_t;

/* ========================================================================
 * L1: Common-Mode Rejection Ratio
 *
 * CMRR = 20 * log10( |A_dm| / |A_cm| )  [dB]
 *
 * where A_dm is the differential-mode gain and A_cm is the
 * common-mode gain. In an ideal differential system, A_cm → 0,
 * so CMRR → ∞. Practical CM chokes achieve CMRR of 30-80 dB.
 * ======================================================================== */
typedef struct {
    double cmrr_db;             /**< CMRR in decibels */
    double a_dm;                /**< Differential-mode gain */
    double a_cm;                /**< Common-mode gain */
    double frequency;           /**< Measurement frequency [Hz] */
    double cm_impedance;        /**< CM choke CM impedance [Ω] */
    double dm_impedance;        /**< CM choke DM impedance [Ω] */
} cmrr_result_t;

/* ========================================================================
 * L1: Insertion Loss — the primary figure of merit for EMI filters
 *
 * IL = 10 * log10( P_without_filter / P_with_filter )    [dB]
 *    = 20 * log10( |V_without| / |V_with| )             [dB]
 *
 * Insertion loss measures how much the filter attenuates noise
 * at a given frequency. Higher IL = better filtering.
 * ======================================================================== */
typedef struct {
    double il_db;               /**< Insertion loss [dB] */
    double frequency;           /**< Measurement frequency [Hz] */
    double z_source;            /**< Source impedance [Ω] */
    double z_load;              /**< Load impedance [Ω] */
    double v_without;           /**< Voltage amplitude without filter */
    double v_with;              /**< Voltage amplitude with filter */
    int is_cm;                  /**< Nonzero if this is CM IL */
} insertion_loss_result_t;

/* ========================================================================
 * L2: Complex frequency response of a filter network
 * ======================================================================== */

/** @brief Complex number representation */
typedef struct {
    double real;
    double imag;
} complex_t;

/** @brief Transfer function evaluated at a complex frequency s = σ + jω */
typedef struct {
    complex_t s;                /**< Complex frequency point */
    complex_t h;                /**< H(s) = Vout(s)/Vin(s) */
    double magnitude;           /**< |H(s)| */
    double phase_deg;           /**< arg(H(s)) [degrees] */
    double group_delay;         /**< -dφ/dω [seconds] */
} transfer_function_point_t;

/** @brief Bode plot data (frequency sweep of magnitude and phase) */
typedef struct {
    double *frequencies;        /**< Frequency points array [Hz] */
    double *magnitude_db;       /**< |H(f)| [dB] */
    double *phase_deg;          /**< arg(H(f)) [degrees] */
    size_t num_points;          /**< Number of frequency points */
    double f_start;             /**< Start frequency [Hz] */
    double f_end;               /**< End frequency [Hz] */
} bode_plot_t;

/* ========================================================================
 * L4: Faraday's Law applied to CM choke
 *
 * For a CM choke wound on a toroidal core with N turns:
 *   CM flux: Φ_cm = N * L * (μ_0 * μ_r * A_e / l_e) * I_cm
 *           Both windings produce flux in the same direction → adds
 *   DM flux: Φ_dm ≈ N * L_leak * I_dm
 *           Flux cancels → only leakage inductance opposes DM current
 *
 * Core saturation: B_max = V_cm_max / (2 * π * f * N * A_e)
 * ======================================================================== */

/** @brief Ferrite core material parameters */
typedef struct {
    char material_code[16];     /**< Material designation (e.g., "N87", "3F3") */
    double mu_i;                /**< Initial relative permeability (unitless) */
    double b_sat;               /**< Saturation flux density [T] */
    double b_rem;               /**< Remanent flux density [T] */
    double h_coercivity;        /**< Coercivity [A/m] */
    double resistivity;         /**< Electrical resistivity [Ω·m] */
    double curie_temp;          /**< Curie temperature [°C] */
    double density;             /**< Material density [kg/m³] */
    double freq_range_max;      /**< Maximum usable frequency [Hz] */
    double tan_delta_typical;   /**< Typical loss tangent at 100 kHz */
} core_material_t;

/** @brief Toroidal core geometry */
typedef struct {
    double od;                  /**< Outer diameter [m] */
    double id;                  /**< Inner diameter [m] */
    double height;              /**< Core height [m] */
    double ae;                  /**< Effective cross-section area [m²] */
    double le;                  /**< Effective magnetic path length [m] */
    double ve;                  /**< Effective volume [m³] */
    double al_value;            /**< Inductance factor [nH/turn²] */
} core_geometry_t;

/**
 * @brief CM choke physical model.
 *
 * A CM choke is a pair of coupled windings on a magnetic core.
 * For CM currents, the magnetic fluxes add → high CM inductance.
 * For DM currents, the fluxes cancel → low DM inductance (only leakage).
 */
typedef struct {
    core_material_t *material;  /**< Core material properties */
    core_geometry_t *geometry;  /**< Core geometry */
    double num_turns;           /**< Number of turns per winding */
    double coupling_coef;       /**< Coupling coefficient k (0 < k ≤ 1) */
    double l_cm;                /**< CM inductance [H] = k * N² * AL */
    double l_dm_leakage;        /**< DM leakage inductance [H] = (1-k) * N² * AL */
    double winding_resistance;  /**< DC winding resistance [Ω] */
    double winding_capacitance; /**< Inter-winding capacitance [F] */
    double core_loss_coef;      /**< Steinmetz coefficient [W/(Hz^x·T^y·m³)] */
    double steinmetz_x;         /**< Steinmetz frequency exponent */
    double steinmetz_y;         /**< Steinmetz flux density exponent */
    double rated_current_dm;    /**< Max DM current before saturation [A] */
    double rated_voltage_cm;    /**< Max CM voltage before saturation [V] */
} cm_choke_model_t;

/* ========================================================================
 * L6: EMC regulatory limits
 * ======================================================================== */

/** @brief EMC standard identifiers */
typedef enum {
    EMC_STD_CISPR11 = 0,        /**< Industrial, scientific, medical equipment */
    EMC_STD_CISPR22,            /**< Information technology equipment (replaced) */
    EMC_STD_CISPR32,            /**< Multimedia equipment */
    EMC_STD_FCC_PART15_A,       /**< FCC Part 15 Class A (commercial) */
    EMC_STD_FCC_PART15_B,       /**< FCC Part 15 Class B (residential) */
    EMC_STD_MIL_STD_461,        /**< Military EMC standard */
    EMC_STD_DO_160,             /**< Aerospace EMC (RTCA DO-160) */
    EMC_STD_CISPR25             /**< Automotive EMC */
} emc_standard_t;

/** @brief Conducted emission limit line segment */
typedef struct {
    double freq_start;          /**< Segment start frequency [Hz] */
    double freq_end;            /**< Segment end frequency [Hz] */
    double limit_dbuV_avg;      /**< Average limit [dBµV] */
    double limit_dbuV_qp;       /**< Quasi-peak limit [dBµV] */
} emc_limit_segment_t;

/** @brief Complete EMC conducted emission limit */
typedef struct {
    emc_standard_t standard;    /**< EMC standard */
    emc_limit_segment_t *segments; /**< Limit line segments */
    size_t num_segments;        /**< Number of segments */
    int is_class_a;             /**< Class A (commercial) vs B (residential) */
} emc_limit_t;

/* ========================================================================
 * L5: Filter design specification (user inputs for design algorithm)
 * ======================================================================== */

typedef struct {
    double v_nominal;           /**< Nominal operating voltage [V] */
    double i_nominal;           /**< Nominal operating current [A] */
    double f_grid;              /**< Grid/line frequency [Hz] (50 or 60) */
    double f_switching;         /**< Converter switching frequency [Hz] */
    double noise_freq_start;    /**< Start of noise band of interest [Hz] */
    double noise_freq_end;      /**< End of noise band of interest [Hz] */
    emc_standard_t standard;    /**< Target EMC standard */
    double design_margin_db;    /**< Additional margin beyond limit [dB] */
    double max_temperature;     /**< Maximum ambient temperature [°C] */
    int is_three_phase;         /**< Single vs three-phase */
    double z_lisn;              /**< LISN impedance [Ω] (typ. 50) */
    double z_source_cm;         /**< Estimated CM source impedance [Ω] */
    double z_source_dm;         /**< Estimated DM source impedance [Ω] */
    double max_volume_cm3;      /**< Maximum filter volume [cm³] */
    double max_cost_usd;        /**< Maximum component cost [USD] */
} filter_design_spec_t;

/** @brief Filter design result with selected components */
typedef struct {
    cm_dm_filter_t filter;      /**< The designed filter */
    filter_design_spec_t spec;  /**< Input specification */
    double achieved_margin_db;  /**< Achieved margin to EMC limit [dB] */
    double estimated_volume_cm3;/**< Estimated total volume [cm³] */
    double estimated_cost_usd;  /**< Estimated BOM cost [USD] */
    int iterations;             /**< Number of design iterations */
    int converged;              /**< Nonzero if design converged */
} filter_design_result_t;

/* ========================================================================
 * L6: Noise spectrum measurement model
 * ======================================================================== */

typedef struct {
    double *frequencies;        /**< Frequency bins [Hz] */
    double *amplitude_dbuV;     /**< Noise amplitude [dBµV] */
    size_t num_bins;            /**< Number of frequency bins */
    double resolution_bw;       /**< Resolution bandwidth [Hz] */
    int detector_type;          /**< 0=avg, 1=peak, 2=quasi-peak */
    double timestamp;           /**< Measurement timestamp (Unix) */
} noise_spectrum_t;

/* ========================================================================
 * L7: Application-specific filter presets
 * ======================================================================== */

typedef enum {
    APP_AC_DC_SMPS = 0,         /**< Switch-mode power supply */
    APP_MOTOR_DRIVE = 1,        /**< Variable-frequency motor drive */
    APP_USB_INTERFACE = 2,      /**< USB 2.0/3.x data lines */
    APP_ETHERNET_INTERFACE = 3, /**< Ethernet (10/100/1000Base-T) */
    APP_HDMI_INTERFACE = 4,     /**< HDMI/DisplayPort */
    APP_CAN_BUS = 5,            /**< CAN bus automotive */
    APP_INVERTER_SOLAR = 6,     /**< Solar inverter */
    APP_LED_DRIVER = 7,         /**< LED driver */
    APP_MEDICAL_DEVICE = 8      /**< Medical-grade PSU (IEC 60601) */
} application_preset_t;

/* ========================================================================
 * API FUNCTIONS
 * ======================================================================== */

/**
 * @brief Decompose line measurements into CM and DM components.
 *
 * L1 Definition implementation.
 * Single-phase: V_cm = (V_L + V_N)/2, V_dm = V_L - V_N
 *               I_cm = I_L + I_N,       I_dm = (I_L - I_N)/2
 *
 * @param v_line    Line voltage [V]
 * @param v_neutral Neutral voltage [V]
 * @param i_line    Line current [A]
 * @param i_neutral Neutral current [A]
 * @param freq      Frequency [Hz]
 * @param result    Output decomposition [non-NULL]
 * @return 0 on success, -1 on invalid input
 * Complexity: O(1)
 */
int cm_dm_decompose(double v_line, double v_neutral,
                    double i_line, double i_neutral,
                    double freq, cm_dm_decomposition_t *result);

/**
 * @brief Three-phase CM/DM decomposition using Clarke transform.
 *
 * For three-phase systems, CM voltage is the zero-sequence component:
 *   V_cm = (V_a + V_b + V_c) / 3
 * DM voltages are derived from the α-β (Clarke) transform.
 *
 * @param va Phase A voltage [V]
 * @param vb Phase B voltage [V]
 * @param vc Phase C voltage [V]
 * @param ia Phase A current [A]
 * @param ib Phase B current [A]
 * @param ic Phase C current [A]
 * @param freq Frequency [Hz]
 * @param cm_out CM quantities output [non-NULL]
 * @param dm_alpha DM alpha-axis quantity output [non-NULL]
 * @param dm_beta  DM beta-axis quantity output [non-NULL]
 * @return 0 on success, -1 on error
 * Complexity: O(1)
 */
int cm_dm_decompose_3phase(double va, double vb, double vc,
                           double ia, double ib, double ic,
                           double freq,
                           cm_quantities_t *cm_out,
                           dm_quantities_t *dm_alpha,
                           dm_quantities_t *dm_beta);

/**
 * @brief Calculate Common-Mode Rejection Ratio (CMRR).
 *
 * L1: CMRR = 20*log10(|Z_cm|/|Z_dm|) for a CM choke
 * A perfect CM choke has infinite CMRR (only CM impedance, zero DM).
 *
 * @param z_cm   CM impedance magnitude [Ω]
 * @param z_dm   DM impedance magnitude [Ω]
 * @param freq   Frequency [Hz]
 * @return CMRR result [dynamically allocated, caller frees]
 * Complexity: O(1)
 */
cmrr_result_t *cmrr_calculate(double z_cm, double z_dm, double freq);

/**
 * @brief Free CMRR result.
 */
void cmrr_free(cmrr_result_t *r);

/**
 * @brief Calculate insertion loss for a CM/DM filter.
 *
 * L1: IL = 20*log10(|V_wo| / |V_w|)
 *
 * @param z_source     Source impedance [Ω]
 * @param z_load       Load impedance [Ω]
 * @param z_filter_cm  CM filter impedance (series) [Ω]
 * @param z_filter_dm  DM filter impedance (series) [Ω]
 * @param freq         Frequency [Hz]
 * @param is_cm        Nonzero for CM IL, zero for DM IL
 * @return Insertion loss result [dynamically allocated]
 * Complexity: O(1)
 */
insertion_loss_result_t *insertion_loss_calc(double z_source, double z_load,
                                             double z_filter_cm,
                                             double z_filter_dm,
                                             double freq, int is_cm);

/**
 * @brief Free insertion loss result.
 */
void insertion_loss_free(insertion_loss_result_t *r);

/**
 * @brief Create a filter element with full parasitic model.
 *
 * @return Allocated filter_element_t [caller must free]
 */
filter_element_t *filter_element_create(filter_elem_type_t type,
                                         double nominal, double tol_pct);

/**
 * @brief Set parasitic parameters for a filter element.
 *
 * L2: Every real component has parasitics. For an inductor:
 *   Z_L(s) = s*L + ESR + 1/(s*EPC)  (parallel resonance)
 * For a capacitor:
 *   Z_C(s) = 1/(s*C) + ESR + s*ESL  (series resonance)
 */
void filter_element_set_parasitics(filter_element_t *elem,
                                   double esr, double esl,
                                   double epc, double r_parallel);

/**
 * @brief Compute the complex impedance of a filter element at frequency f.
 *
 * Z_elem(jω) = R + jωL + 1/(jωC) with all parasitics included.
 */
complex_t filter_element_impedance(const filter_element_t *elem, double freq);

/**
 * @brief Free a filter element.
 */
void filter_element_free(filter_element_t *elem);

/**
 * @brief Create a CM choke model from core material and geometry.
 *
 * L4: Faraday's law on a toroidal core:
 *   L = N² * μ₀ * μ_r * A_e / l_e = N² * AL (nH)
 *
 * @param mat  Core material parameters
 * @param geom Core geometry parameters
 * @param n    Number of turns per winding
 * @param k    Coupling coefficient (typ. 0.98-0.999)
 * @return CM choke model [dynamically allocated]
 */
cm_choke_model_t *cm_choke_create(const core_material_t *mat,
                                   const core_geometry_t *geom,
                                   double n, double k);

/**
 * @brief Calculate CM choke CM impedance at given frequency.
 *
 * Includes winding resistance, core loss, and inter-winding capacitance.
 * Z_cm(jω) = jωL_cm + R_winding + R_core_loss(jω) || (1/jω*C_winding)
 */
complex_t cm_choke_cm_impedance(const cm_choke_model_t *choke, double freq);

/**
 * @brief Calculate CM choke DM impedance (leakage inductance only).
 *
 * Z_dm(jω) = jω*L_leakage + R_winding
 */
complex_t cm_choke_dm_impedance(const cm_choke_model_t *choke, double freq);

/**
 * @brief Check if CM choke will saturate under given conditions.
 *
 * L6: B_max = V_cm_max / (2π * f * N * A_e)
 * If B_max > B_sat, the core saturates → CM inductance collapses → filter fails.
 *
 * @return Nonzero if saturation occurs, 0 if safe
 */
int cm_choke_check_saturation(const cm_choke_model_t *choke,
                              double v_cm_max, double freq);

/**
 * @brief Calculate core loss using Steinmetz equation.
 *
 * P_core = k * f^x * B^y * V_e  [W]
 */
double cm_choke_core_loss(const cm_choke_model_t *choke,
                          double b_peak, double freq);

/**
 * @brief Free CM choke model.
 */
void cm_choke_free(cm_choke_model_t *choke);

/**
 * @brief Design a complete CM/DM EMI filter.
 *
 * L5 Algorithm: Iterative filter design.
 * 1. Measure/estimate noise spectrum
 * 2. Determine required attenuation (noise - limit + margin)
 * 3. Select filter order: 40 dB/dec per stage
 * 4. Calculate cutoff frequency: f_c = f_noise / 10^(IL_req/40)
 * 5. Choose L and C values meeting f_c = 1/(2π√(LC))
 * 6. Verify CM choke doesn't saturate under DM load current
 * 7. Iterate to meet volume/cost constraints
 *
 * @return Filter design result [dynamically allocated]
 */
filter_design_result_t *filter_design_emi(const filter_design_spec_t *spec,
                                           const noise_spectrum_t *noise);

/**
 * @brief Free filter design result.
 */
void filter_design_result_free(filter_design_result_t *result);

/* ========================================================================
 * L2: Filter transfer function computation
 * ======================================================================== */

/**
 * @brief Compute transfer function H(s) of an LC filter stage.
 *
 * For an LC low-pass filter with source impedance Z_s and load impedance Z_L:
 *
 * Π-filter (C1—L—C2):
 *   H(s) = 1 / (1 + s*L/Z_L + s²*L*C2) * Z_L/(Z_L + Z_s*...)
 *
 * This is computed exactly using s-domain nodal analysis.
 */
transfer_function_point_t filter_stage_transfer(const filter_stage_t *stage,
                                                 double frequency,
                                                 double z_source,
                                                 double z_load);

/**
 * @brief Compute complete Bode plot for a filter stage.
 *
 * L3: Frequency response analysis via complex transfer function evaluation.
 *
 * @param stage    Filter stage
 * @param f_start  Start frequency [Hz]
 * @param f_end    End frequency [Hz]
 * @param num_pts  Number of logarithmically-spaced points
 * @param z_source Source impedance [Ω]
 * @param z_load   Load impedance [Ω]
 * @return Bode plot data [dynamically allocated]
 * Complexity: O(N) where N = num_pts
 */
bode_plot_t *filter_stage_bode(const filter_stage_t *stage,
                                double f_start, double f_end,
                                size_t num_pts,
                                double z_source, double z_load);

/**
 * @brief Free Bode plot data.
 */
void bode_plot_free(bode_plot_t *bp);

/**
 * @brief Compute cascaded transfer function of multi-stage filter.
 *
 * H_total(s) = H₁(s) * H₂(s) * ... * Hₙ(s)
 *
 * Each stage is computed independently and multiplied. This assumes
 * the stages are properly buffered/isolated (valid when output impedance
 * of stage n ≪ input impedance of stage n+1).
 */
transfer_function_point_t filter_cascade_transfer(const cm_dm_filter_t *filter,
                                                   double frequency,
                                                   double z_source,
                                                   double z_load);

/* ========================================================================
 * L6: Matching network and resonance damping
 * ======================================================================== */

/**
 * @brief Calculate optimal damping resistor for an LC filter.
 *
 * L6 Canonical Problem: Undamped LC filters have a resonance peak at
 * f_res = 1/(2π√(LC)) that can amplify noise instead of attenuating it.
 *
 * Critical damping: R_damp = 2 * √(L/C)
 * Optimal damping (series R-C damper): R_opt = √(L/C)
 *
 * @param l Inductance [H]
 * @param c Capacitance [F]
 * @return Optimal damping resistance [Ω]
 */
double filter_optimal_damping_resistance(double l, double c);

/**
 * @brief Compute resonance frequency of an LC pair.
 *
 * f_res = 1 / (2π * √(L*C))
 */
double filter_resonance_frequency(double l, double c);

/**
 * @brief Compute quality factor Q of an LC filter.
 *
 * Q = (1/R) * √(L/C)  for series RLC
 * Q = R * √(C/L)       for parallel RLC
 *
 * High Q → sharp resonance → potential noise amplification.
 */
double filter_quality_factor(double l, double c, double r, int is_parallel);

/* ========================================================================
 * L1: Utility — complex number arithmetic
 * ======================================================================== */

complex_t complex_add(complex_t a, complex_t b);
complex_t complex_sub(complex_t a, complex_t b);
complex_t complex_mul(complex_t a, complex_t b);
complex_t complex_div(complex_t a, complex_t b);
double complex_mag(complex_t z);
double complex_phase_deg(complex_t z);
complex_t complex_from_polar(double mag, double phase_rad);
complex_t complex_conjugate(complex_t z);

/* ========================================================================
 * L7: Application-specific presets
 * ======================================================================== */

/**
 * @brief Get a pre-configured filter design specification for common applications.
 *
 * L7 Applications: Provides reasonable defaults for known use cases such as
 * AC-DC SMPS (65W laptop adapter), motor drive (1HP VFD), USB 3.0, etc.
 *
 * @return Pre-filled design spec [dynamically allocated]
 */
filter_design_spec_t *filter_preset_get(application_preset_t preset);

/**
 * @brief Get EMC limit line for a given standard.
 *
 * Returns the conducted emission limits for CISPR/FCC/MIL standards.
 * The limit line defines the maximum allowed noise amplitude vs frequency.
 */
emc_limit_t *emc_limit_get(emc_standard_t standard, int is_class_a);

/**
 * @brief Check if a noise spectrum passes an EMC limit.
 *
 * @param noise Measured noise spectrum
 * @param limit Applicable EMC limit
 * @return 0 = pass, 1 = fail, -1 = error
 */
int emc_compliance_check(const noise_spectrum_t *noise,
                         const emc_limit_t *limit);

/**
 * @brief Free EMC limit data.
 */
void emc_limit_free(emc_limit_t *limit);

/* ========================================================================
 * L3: s-domain network analysis
 * ======================================================================== */

/**
 * @brief Build the admittance matrix for a filter network.
 *
 * L3: The network is described by Y(s) where I = Y(s)·V.
 * Each element contributes to the nodal admittance matrix.
 *
 * @param elements Array of filter elements
 * @param n_elem   Number of elements
 * @param freq     Frequency [Hz]
 * @param n_nodes  Number of nodes (output)
 * @return Flat array representing n_nodes × n_nodes complex matrix (row-major)
 *         [dynamically allocated, caller frees]
 */
complex_t *build_admittance_matrix(const filter_element_t *elements,
                                    size_t n_elem, double freq,
                                    size_t *n_nodes);

/**
 * @brief Solve the linear system Y·V = I to find node voltages.
 *
 * Uses Gaussian elimination with partial pivoting for complex matrices.
 *
 * @param y_mat Admittance matrix [n×n, row-major]
 * @param i_vec Current source vector [n]
 * @param v_out Solution voltage vector [n] (output)
 * @param n     Matrix size
 * @return 0 on success, -1 on singular matrix
 * Complexity: O(n³)
 */
int solve_complex_linear_system(const complex_t *y_mat,
                                 const complex_t *i_vec,
                                 complex_t *v_out, size_t n);

/* ========================================================================
 * L5: Optimization — find component values to meet EMC limits
 * ======================================================================== */

/**
 * @brief Determine minimum required insertion loss at each frequency.
 *
 * IL_req(f) = Noise(f) - Limit(f) + Margin
 *
 * @return Array of required IL [dB] at each frequency point
 */
double *required_attenuation(const noise_spectrum_t *noise,
                              const emc_limit_t *limit,
                              double margin_db, size_t *n_out);

/**
 * @brief Determine the minimum filter order to achieve required attenuation.
 *
 * Each order provides approximately 20 dB/decade attenuation.
 * For an LC stage: 40 dB/decade.
 *
 * Order = ceil( IL_req_max / (20 * log10(f_noise / f_c)) )
 */
int minimum_filter_order(const double *il_req_db, const double *freqs,
                          size_t n, double f_cutoff);

/**
 * @brief Select L and C values for a filter stage.
 *
 * Constraint: f_c = 1/(2π√(LC)), so infinite L-C combinations exist.
 * We optimize for minimal volume: vol ∝ L*I² + C*V² (energy storage).
 *
 * @param f_c       Cutoff frequency [Hz]
 * @param z_source  Source impedance [Ω]
 * @param z_load    Load impedance [Ω]
 * @param i_rated   Rated current [A]
 * @param v_rated   Rated voltage [V]
 * @param l_out     Output inductance [H]
 * @param c_out     Output capacitance [F]
 */
void filter_select_lc(double f_c, double z_source, double z_load,
                      double i_rated, double v_rated,
                      double *l_out, double *c_out);

#ifdef __cplusplus
}
#endif

#endif /* CM_DM_FILTER_H */
