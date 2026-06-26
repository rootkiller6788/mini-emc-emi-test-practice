/**
 * @file filter_design_params.h
 * @brief EMI Filter Design Parameters, Constraints & Optimization
 *
 * Comprehensive filter design specification covering:
 *  - EMC regulatory limits (CISPR 11/22/32, FCC Part 15, MIL-STD-461, DO-160)
 *  - Safety constraints (creepage, clearance, leakage current, Y-cap limits)
 *  - Thermal management (power loss, temperature rise, derating)
 *  - Mechanical constraints (volume, weight, form factor)
 *  - Cost optimization (component selection, BOM cost minimization)
 *  - Reliability analysis (MTBF, component stress, derating per MIL-HDBK-217)
 *  - Standard filter product comparison and selection
 *
 * Reference:
 *  - CISPR 32: Electromagnetic compatibility of multimedia equipment
 *  - FCC Part 15: Radio Frequency Devices
 *  - IEC 60939: Passive filter units for electromagnetic interference suppression
 *  - UL 1283: Standard for Electromagnetic Interference Filters
 *  - IEC 60950-1 / IEC 62368-1: Safety of IT equipment
 */

#ifndef FILTER_DESIGN_PARAMS_H
#define FILTER_DESIGN_PARAMS_H

#include "cm_dm_filter.h"
#include "cm_dm_network.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * L1: EMC regulatory limit database
 * ======================================================================== */

/** @brief Total conducted emission limits (complete band) */
typedef struct {
    emc_standard_t standard;        /**< EMC standard */
    int is_class_a;                 /**< Class A (commercial) vs B (residential) */
    double freq_start_khz;          /**< Lowest frequency in limit band [kHz] */
    double freq_stop_mhz;           /**< Highest frequency in limit band [MHz] */
    size_t num_segments;            /**< Number of limit line segments */
    /* Key limit values at standard reference frequencies */
    double limit_150k_qp_dbuV;      /**< Limit at 150 kHz, quasi-peak [dBµV] */
    double limit_500k_qp_dbuV;      /**< Limit at 500 kHz, quasi-peak [dBµV] */
    double limit_5M_qp_dbuV;        /**< Limit at 5 MHz, quasi-peak [dBµV] */
    double limit_30M_qp_dbuV;       /**< Limit at 30 MHz, quasi-peak [dBµV] */
    double limit_150k_avg_dbuV;     /**< Limit at 150 kHz, average [dBµV] */
    double limit_500k_avg_dbuV;     /**< Limit at 500 kHz, average [dBµV] */
    double limit_5M_avg_dbuV;       /**< Limit at 5 MHz, average [dBµV] */
    double limit_30M_avg_dbuV;      /**< Limit at 30 MHz, average [dBµV] */
    char description[128];          /**< Standard description */
} emc_limit_summary_t;

/* ========================================================================
 * L2: Safety constraints for filter design
 * ======================================================================== */

/** @brief Creepage and clearance requirements per IEC 62368-1 */
typedef struct {
    double operating_voltage_rms;   /**< Operating voltage [V_rms] */
    double transient_overvoltage;   /**< Transient overvoltage category [V] */
    double pollution_degree;        /**< Pollution degree (1-4) */
    double creepage_mm;             /**< Required creepage distance [mm] */
    double clearance_mm;            /**< Required clearance distance [mm] */
    double solid_insulation_kv;     /**< Required solid insulation [kV] */
    int material_group;             /**< CTI material group (I, II, IIIa, IIIb) */
} safety_spacing_t;

/** @brief Y-capacitor safety limits */
typedef struct {
    double max_total_y_cap_nf;      /**< Max total Y-capacitance [nF] */
    double max_leakage_ma;          /**< Max leakage current [mA] */
    double line_freq_hz;            /**< Line frequency [Hz] */
    double line_voltage_rms;        /**< Line voltage [V_rms] */
    int application_type;           /**< 0=ITE, 1=Medical, 2=Industrial */
} y_cap_limits_t;

/** @brief Filter safety compliance summary */
typedef struct {
    safety_spacing_t spacing;       /**< Creepage/clearance results */
    y_cap_limits_t y_cap;           /**< Y-cap limits */
    double bleed_resistor_ohms;     /**< Required bleed resistor [Ω] */
    double discharge_time_ms;       /**< Discharge time [ms] */
    int hi_pot_test_kv;             /**< Required hipot test voltage [kV] */
    int passes_safety;              /**< Nonzero if all safety checks pass */
} safety_compliance_t;

/* ========================================================================
 * L2: Thermal management
 * ======================================================================== */

typedef struct {
    double total_power_loss_w;      /**< Total filter power dissipation [W] */
    double ambient_temp_c;          /**< Ambient temperature [°C] */
    double surface_area_cm2;        /**< Effective cooling surface area [cm²] */
    double thermal_resistance_kw;   /**< Case-to-ambient thermal resistance [K/W] */
    double internal_temp_rise_k;    /**< Internal temperature rise [K] */
    double case_temp_c;             /**< Estimated case temperature [°C] */
    double core_hotspot_temp_c;     /**< Core hotspot temperature [°C] */
    double max_core_temp_c;         /**< Maximum allowable core temp [°C] */
    double temp_margin_k;           /**< Temperature margin [K] */
    int passes_thermal;             /**< Nonzero if thermal check passes */
} thermal_analysis_t;

/* ========================================================================
 * L2: Mechanical constraints
 * ======================================================================== */

typedef struct {
    double max_length_mm;           /**< Maximum length [mm] */
    double max_width_mm;            /**< Maximum width [mm] */
    double max_height_mm;           /**< Maximum height [mm] */
    double max_volume_cm3;          /**< Maximum volume [cm³] */
    double max_weight_g;            /**< Maximum weight [g] */
    double pcb_area_mm2;            /**< Available PCB area [mm²] */
    int mounting_type;              /**< 0=PCB, 1=Chassis, 2=DIN rail */
    int connector_type;             /**< 0=Wire, 1=Screw terminal, 2=IEC inlet */
} mechanical_constraints_t;

/* ========================================================================
 * L5: Component database and selection
 * ======================================================================== */

/** @brief Component from standard vendor catalog */
typedef struct {
    char part_number[32];           /**< Manufacturer part number */
    char manufacturer[24];          /**< Manufacturer name */
    filter_elem_type_t type;        /**< Component type */
    double value;                   /**< Nominal value (L[H], C[F], R[Ω]) */
    double tolerance_pct;           /**< Tolerance [%] */
    double rated_voltage_v;         /**< Rated voltage [V] */
    double rated_current_a;         /**< Rated current [A] (if inductor) */
    double esr_ohm;                 /**< ESR [Ω] */
    double esl_nh;                  /**< ESL [nH] (if capacitor) */
    double epc_pf;                  /**< EPC [pF] (if inductor) */
    double self_resonant_mhz;       /**< Self-resonant frequency [MHz] */
    double length_mm;               /**< Length [mm] */
    double width_mm;                /**< Width [mm] */
    double height_mm;               /**< Height [mm] */
    double price_usd_1k;            /**< Price @1k qty [USD] */
    int in_stock;                   /**< Nonzero if in stock */
} component_entry_t;

/** @brief Catalog database */
typedef struct {
    component_entry_t *entries;     /**< Component entries */
    size_t count;                   /**< Number of entries */
    size_t capacity;                /**< Allocated capacity */
} component_database_t;

/* ========================================================================
 * L5: Filter optimization cost function
 * ======================================================================== */

/** @brief Optimization objective weights */
typedef struct {
    double weight_insertion_loss;   /**< Weight for IL performance */
    double weight_cost;             /**< Weight for BOM cost */
    double weight_volume;           /**< Weight for volume */
    double weight_power_loss;       /**< Weight for efficiency */
    double weight_margin;           /**< Weight for EMC margin */
    double weight_cmrr;             /**< Weight for CM rejection */
} optimization_weights_t;

/** @brief Optimization result */
typedef struct {
    cm_dm_filter_t *filter;         /**< Best filter found */
    double fitness;                 /**< Composite fitness score */
    double il_score;                /**< IL sub-score */
    double cost_score;              /**< Cost sub-score */
    double volume_score;            /**< Volume sub-score */
    double loss_score;              /**< Power loss sub-score */
    int iterations_used;            /**< Iterations to convergence */
    int pareto_optimal;             /**< Nonzero if Pareto-optimal */
} optimization_result_t;

/* ========================================================================
 * L7: Standard filter product comparison
 * ======================================================================== */

typedef struct {
    char manufacturer[32];
    char part_number[32];
    double rated_current_a;
    double rated_voltage_v;
    double cm_il_150k_db;
    double dm_il_150k_db;
    double cm_il_10M_db;
    double dm_il_10M_db;
    double leakage_current_ma;
    double length_mm;
    double width_mm;
    double height_mm;
    double price_usd;
    int medical_rated;
} standard_filter_t;

typedef struct {
    standard_filter_t *filters;     /**< Array of candidate filters */
    size_t count;                   /**< Number of candidates */
    int best_index;                 /**< Index of best match */
    double best_score;              /**< Best match score */
} filter_selection_t;

/* ========================================================================
 * L8: Reliability analysis (MIL-HDBK-217F based)
 * ======================================================================== */

typedef struct {
    double mtbf_hours;              /**< Mean Time Between Failures [hours] */
    double failure_rate_fit;        /**< Failure rate [FIT = failures/10⁹ hrs] */
    double reliability_5yr;         /**< Reliability at 5 years */
    double reliability_10yr;        /**< Reliability at 10 years */
    double capacitor_failure_rate;  /**< Capacitor contribution [FIT] */
    double inductor_failure_rate;   /**< Inductor contribution [FIT] */
    double resistor_failure_rate;   /**< Resistor contribution [FIT] */
    double pcb_failure_rate;        /**< PCB/connector contribution [FIT] */
    double dominant_stressor[3];    /**< Top 3 stress factors */
} reliability_report_t;

/* ========================================================================
 * L8: EMI filter impedance interaction analysis
 * ======================================================================== */

/**
 * @brief Filter input-output impedance interaction.
 *
 * L8 Advanced: The filter input impedance Z_in(f) interacts with the
 * source impedance Z_S(f), potentially causing oscillations or noise
 * amplification at certain frequencies.
 *
 * Middlebrook stability criterion:
 *   |Z_in| >> |Z_S|  at all frequencies where |T| >= 1
 *
 * Oscillation risk when |Z_in| ≈ |Z_S| with phase difference > 180°.
 */
typedef struct {
    complex_t *z_in;             /**< Filter input impedance vs freq */
    complex_t *z_out;            /**< Filter output impedance vs freq */
    double *frequencies;         /**< Frequency points [Hz] */
    size_t num_points;           /**< Number of points */
    double max_z_ratio;          /**< Max |Z_in|/|Z_S| */
    double min_z_ratio;          /**< Min |Z_in|/|Z_S| */
    double oscillation_risk;     /**< Oscillation risk factor [0-1] */
    int middlebrook_pass;        /**< Nonzero if Middlebrook criterion met */
} impedance_interaction_t;

/* ========================================================================
 * L9: Active EMI filter parameters
 * ======================================================================== */

/**
 * @brief Active EMI filter (AEF) configuration.
 *
 * L9 Research: Active filters use op-amps to sense noise and inject
 * cancelling current, achieving high IL with small components.
 * Used in high-power-density converters (GaN/SiC).
 */
typedef struct {
    double gain_bandwidth_mhz;      /**< Op-amp GBW [MHz] */
    double slew_rate_v_us;          /**< Slew rate [V/µs] */
    double sense_gain;              /**< Noise sensing gain */
    double injection_gain;          /**< Cancellation injection gain */
    double bandwidth_hz;            /**< Effective AEF bandwidth [Hz] */
    double max_cancel_voltage;      /**< Max cancel voltage [V] */
    double power_consumption_mw;    /**< AEF power consumption [mW] */
} active_emi_filter_t;

/* ========================================================================
 * API FUNCTIONS
 * ======================================================================== */

/**
 * @brief Get complete EMC limit summary for a given standard.
 *
 * Returns all relevant limit values for quick reference and
 * for filter attenuation targeting.
 */
emc_limit_summary_t emc_limit_summary_get(emc_standard_t standard,
                                           int is_class_a);

/**
 * @brief Compute required safety spacings per IEC 62368-1.
 *
 * Based on operating voltage, overvoltage category, pollution degree,
 * and CTI material group.
 *
 * @param voltage_rms   Operating voltage [V_rms]
 * @param pollution_deg Pollution degree (1-4)
 * @param material_grp  CTI material group (0=I, 1=II, 2=IIIa, 3=IIIb)
 * @param ovc           Overvoltage category (I-IV → 1-4)
 * @return Safety spacing requirements
 */
safety_spacing_t safety_spacing_calculate(double voltage_rms,
                                           int pollution_deg,
                                           int material_grp,
                                           int ovc);

/**
 * @brief Compute Y-capacitor constraints for safety compliance.
 *
 * L2: Y-cap total value limited by allowable leakage current.
 *
 * ITE (IEC 62368-1):   3.5 mA (fixed), 0.75 mA (portable)
 * Medical (IEC 60601): 0.5 mA (normal), 0.1 mA (single fault)
 *
 * @param application 0=ITE fixed, 1=ITE portable, 2=Medical
 */
y_cap_limits_t y_cap_limits_calculate(int application,
                                       double line_voltage_rms,
                                       double line_freq_hz);

/**
 * @brief Run complete safety compliance check on a filter design.
 *
 * Checks: creepage/clearance, Y-cap leakage, bleed resistor, hipot voltage.
 */
safety_compliance_t safety_compliance_check(const cm_dm_filter_t *filter,
                                             const filter_design_spec_t *spec);

/**
 * @brief Perform thermal analysis of a filter design.
 *
 * L2: Estimates power loss, temperature rise, and thermal margins.
 * Uses a simple lumped thermal model: T_junction = T_ambient + R_th × P_loss.
 */
thermal_analysis_t thermal_analysis(const cm_dm_filter_t *filter,
                                     double ambient_temp_c,
                                     double surface_area_cm2);

/**
 * @brief Create a component database.
 */
component_database_t *component_database_create(void);

/**
 * @brief Add a component to the database.
 */
int component_database_add(component_database_t *db,
                            const component_entry_t *entry);

/**
 * @brief Find components matching criteria.
 *
 * @param type  Component type
 * @param value Target value
 * @param tol   Tolerance window [fraction] (e.g., 0.2 = ±20%)
 * @param n_out Output: number of matches
 * @return Array of matching component indices [caller frees]
 */
size_t *component_database_find(const component_database_t *db,
                                 filter_elem_type_t type,
                                 double value, double tol,
                                 size_t *n_out);

/**
 * @brief Free component database.
 */
void component_database_free(component_database_t *db);

/**
 * @brief Optimize filter design using a composite cost function.
 *
 * L5: Multi-objective optimization balancing IL, cost, volume, efficiency.
 * Uses a weighted-sum scalarization approach.
 *
 * @param spec     Design specification
 * @param db       Component database (NULL = use built-in defaults)
 * @param weights  Optimization weights
 * @param noise    Noise spectrum (NULL = use theoretical estimates)
 * @return Optimization result [dynamically allocated]
 */
optimization_result_t *filter_optimize(const filter_design_spec_t *spec,
                                        const component_database_t *db,
                                        const optimization_weights_t *weights,
                                        const noise_spectrum_t *noise);

/**
 * @brief Free optimization result.
 */
void optimization_result_free(optimization_result_t *result);

/**
 * @brief Select best standard (off-the-shelf) filter for an application.
 *
 * L7: Compares commercial EMI filters from major manufacturers
 * (Schaffner, TDK-Lambda, KEMET, Murata, etc.) against requirements.
 */
filter_selection_t *standard_filter_select(const filter_design_spec_t *spec);

/**
 * @brief Free filter selection data.
 */
void filter_selection_free(filter_selection_t *sel);

/**
 * @brief Compute reliability prediction per MIL-HDBK-217F.
 *
 * L8: Parts count reliability prediction based on component stress
 * ratios, quality levels, and environment.
 *
 * @param filter      The filter design
 * @param ambient_c   Ambient temperature [°C]
 * @param environment 0=Ground Benign, 1=Ground Fixed, 2=Naval Sheltered, 3=Airborne
 */
reliability_report_t reliability_predict(const cm_dm_filter_t *filter,
                                          double ambient_c,
                                          int environment);

/**
 * @brief Analyze impedance interaction between filter and source/load.
 *
 * L8: Checks for potential instability due to impedance mismatch.
 * Implements Middlebrook's stability criterion.
 */
impedance_interaction_t *impedance_interaction_analyze(
    const cm_dm_filter_t *filter,
    double *z_source_vs_freq,
    double *z_load_vs_freq,
    const double *frequencies,
    size_t num_points);

/**
 * @brief Free impedance interaction data.
 */
void impedance_interaction_free(impedance_interaction_t *ia);

/**
 * @brief Design an active EMI filter for high-power-density applications.
 *
 * L9: Active EMI filtering for GaN/SiC converters.
 *
 * Sensing: Capacitive voltage divider or Rogowski coil
 * Injection: Current injection transformer or op-amp with push-pull
 *
 * @param max_freq     Maximum noise frequency to cancel [Hz]
 * @param max_v_noise  Maximum CM noise voltage to cancel [V]
 * @param gbw_mhz      Available op-amp GBW [MHz]
 * @return Active EMI filter parameters
 */
active_emi_filter_t active_emi_filter_design(double max_freq,
                                              double max_v_noise,
                                              double gbw_mhz);

#ifdef __cplusplus
}
#endif

#endif /* FILTER_DESIGN_PARAMS_H */
