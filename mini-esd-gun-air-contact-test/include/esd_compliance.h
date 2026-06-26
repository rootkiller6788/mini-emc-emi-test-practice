/**
 * @file esd_compliance.h
 * @brief ESD compliance verification per IEC 61000-4-2 and related standards
 *
 * Cross-standard compliance checking for:
 *   - IEC 61000-4-2 (system-level ESD immunity)
 *   - ISO 10605 (automotive ESD)
 *   - ANSI/ESDA/JEDEC JS-001 (HBM device-level)
 *   - IEC 61340-3-1 (ESD control program)
 *
 * Knowledge coverage:
 *   L1 - Definitions: Compliance criteria, performance categories
 *   L2 - Core Concepts: System-level vs device-level ESD
 *   L4 - Fundamental Laws: Compliance margin analysis
 *   L7 - Applications: Automotive ESD (ISO 10605), medical ESD
 */

#ifndef ESD_COMPLIANCE_H
#define ESD_COMPLIANCE_H

#include "esd_waveform.h"
#include "esd_test_setup.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── ESD Standards Overview ───────────────────────────────────────
 *
 * L1 Definition: ESD testing standards comparison
 *
 * Standard         | Scope          | Cap   | R     | Max V   | Waveform
 * ──────────────────────────────────────────────────────────────────
 * IEC 61000-4-2    | System-level   | 150pF | 330Ω  | 8kV/15kV| ns pulse
 * ISO 10605        | Automotive     | 150pF | 330Ω  | 25kV    | ns pulse
 *                                          | 330pF | 330Ω  | 25kV
 *                                          | 150pF | 2000Ω | 25kV
 * ANSI/ESDA JS-001 | HBM device     | 100pF | 1500Ω | 8kV     | 150ns τ
 * JEDEC JESD22-A114| HBM device     | 100pF | 1500Ω | 8kV     | same
 * IEC 61340-3-1    | ESD control    | 150pF | 330Ω  | —       | process
 */

/** ESD standard identifier */
typedef enum {
    ESD_STD_IEC_61000_4_2 = 0,  /**< IEC 61000-4-2: Generic EMC immunity */
    ESD_STD_ISO_10605 = 1,       /**< ISO 10605: Road vehicles ESD */
    ESD_STD_ANSI_ESDA_JS001 = 2, /**< ANSI/ESDA/JEDEC JS-001: HBM device */
    ESD_STD_JEDEC_JESD22_A114 = 3, /**< JEDEC HBM */
    ESD_STD_IEC_61340_3_1 = 4,   /**< IEC 61340-3-1: ESD control program */
    ESD_STD_MIL_STD_883 = 5,     /**< MIL-STD-883 Method 3015.7: HBM */
    ESD_STD_AEC_Q100 = 6,        /**< AEC-Q100-002: Automotive HBM */
    ESD_STD_GR_1089_CORE = 7     /**< GR-1089-CORE: Telecom ESD */
} esd_standard_id_t;

/**
 * @brief Standard-specific ESD circuit parameters
 *
 * Different standards use different R-C combinations to simulate
 * different ESD source models.
 *
 * L2 Core Concept: The Human Body Model (HBM), Machine Model (MM),
 * and Charged Device Model (CDM) represent different ESD scenarios.
 *
 *   HBM: Standing person → 100 pF, 1500 Ω (device-level)
 *   MM: Machine/tool → 200 pF, 0 Ω (discontinued, absorbed by HBM/CDM)
 *   CDM: Device self-discharge → device capacitance, low R
 *   IEC gun: Hand-held tool → 150 pF, 330 Ω (system-level)
 */
typedef struct {
    esd_standard_id_t standard;  /**< Standard identifier */
    double cs_pf;                 /**< Storage capacitance [pF] */
    double rd_ohm;                /**< Discharge resistance [Ω] */
    double max_voltage_kv;        /**< Maximum test voltage [kV] */
    const char *name;             /**< Standard name */
    const char *scope;            /**< Application scope */
} esd_standard_params_t;

/**
 * @brief HBM device classification per ANSI/ESDA/JEDEC JS-001
 *
 * L1 Definition: HBM sensitivity classification.
 *
 * Class 0:  < 250 V   (ultra-sensitive)
 * Class 1A: 250-500 V
 * Class 1B: 500-1000 V
 * Class 1C: 1000-2000 V
 * Class 2:  2000-4000 V
 * Class 3A: 4000-8000 V
 * Class 3B: > 8000 V
 */
typedef enum {
    HBM_CLASS_0  = 0,   /**< < 250 V */
    HBM_CLASS_1A = 1,   /**< 250 to <500 V */
    HBM_CLASS_1B = 2,   /**< 500 to <1000 V */
    HBM_CLASS_1C = 3,   /**< 1000 to <2000 V */
    HBM_CLASS_2  = 4,   /**< 2000 to <4000 V */
    HBM_CLASS_3A = 5,   /**< 4000 to <8000 V */
    HBM_CLASS_3B = 6    /**< ≥ 8000 V */
} esd_hbm_class_t;

/**
 * @brief ESD compliance margin analysis
 *
 * L4 Theorem: The compliance margin is the ratio between the
 * device's measured failure threshold and the required test level.
 * A margin of 2× or more is typically required for production.
 *
 * Compliance Margin = V_failure / V_test_required
 */
typedef struct {
    double required_level_kv;     /**< Required test level [kV] */
    double failure_threshold_kv;  /**< Measured failure threshold [kV] */
    double margin_ratio;          /**< Compliance margin (failure/required) */
    double margin_db;             /**< Compliance margin [dB] */
    int passes;                   /**< 1 if margin ≥ minimum */
    double min_margin;            /**< Minimum required margin */
} esd_compliance_margin_t;

/**
 * @brief Cross-standard equivalence mapping
 *
 * Maps ESD immunity levels across different standards.
 * L7 Application: Product design must meet multiple standards.
 */
typedef struct {
    double iec_61000_4_2_kv;    /**< IEC system-level equivalent [kV] */
    double hbm_voltage_v;       /**< HBM device-level equivalent [V] */
    double iso_10605_kv;        /**< ISO 10605 automotive equivalent [kV] */
    int equivalent_valid;       /**< 1 if equivalence is well-established */
    char caveat[128];           /**< Important caveats about equivalence */
} esd_cross_standard_map_t;

/**
 * @brief Automotive ESD test parameters (ISO 10605)
 *
 * L7 Application: Automotive electronics face unique ESD challenges:
 *   - Higher voltages (up to 25 kV)
 *   - Both powered and unpowered testing
 *   - In-vehicle vs handling ESD
 *   - Multiple discharge networks (150pF/330Ω, 330pF/330Ω, 150pF/2000Ω)
 */
typedef struct {
    double voltage_kv;           /**< Test voltage [kV] */
    double cs_pf;                /**< Discharge capacitor [pF] */
    double rd_ohm;               /**< Discharge resistor [Ω] */
    int powered_test;            /**< 1 = EUT powered during test */
    int esd_into_connector_pins; /**< 1 = apply ESD to connector pins */
    int esd_into_paint_surface;  /**< 1 = apply ESD to painted surfaces */
    int air_discharge_only;      /**< 1 = air discharge only, 0 = both */
} esd_auto_test_params_t;

/* ─── API: ESD Compliance ────────────────────────────────────── */

/**
 * @brief Get standard parameters for a given ESD standard.
 *
 * L1 Definition: Returns the nominal R, C, and max voltage
 * for the specified standard.
 */
esd_standard_params_t esd_standard_get(esd_standard_id_t standard);

/**
 * @brief Get HBM device classification from failure voltage.
 *
 * @param hbm_voltage_v  HBM failure threshold [V]
 * @return               HBM class (0 through 3B)
 */
esd_hbm_class_t esd_hbm_classify(double hbm_voltage_v);

/**
 * @brief Get the HBM voltage range for a given class.
 *
 * @param hbm_class   HBM class
 * @param[out] v_min  Minimum voltage [V]
 * @param[out] v_max  Maximum voltage [V] (INFINITY for 3B)
 */
void esd_hbm_class_range(esd_hbm_class_t hbm_class,
                          double *v_min, double *v_max);

/**
 * @brief Compute compliance margin.
 *
 * L4 Theorem: Margin analysis for ESD qualification.
 *
 * @param failure_threshold_kv  Device failure voltage [kV]
 * @param required_level_kv     Required test voltage [kV]
 * @param min_margin_ratio      Minimum acceptable margin (e.g., 1.5)
 * @param[out] margin           Compliance margin result
 */
void esd_compliance_margin_compute(double failure_threshold_kv,
                                    double required_level_kv,
                                    double min_margin_ratio,
                                    esd_compliance_margin_t *margin);

/**
 * @brief Convert HBM device-level voltage to approximate IEC system-level.
 *
 * L2 Core Concept: The relationship between HBM and IEC ESD is not
 * a simple linear mapping because:
 *   - Different capacitance: 100 pF vs 150 pF
 *   - Different resistance: 1500 Ω vs 330 Ω
 *   - Different waveforms: 150 ns exponential vs 1 ns pulse
 *
 * Empirical equivalence (Paul, 2006):
 *   IEC_2kV ≈ HBM_8kV in terms of peak current (7.5 A)
 *   IEC_4kV ≈ HBM_15kV (but HBM max is typically 8kV)
 *
 * @param hbm_voltage_v  HBM voltage [V]
 * @return               Approximate IEC 61000-4-2 equivalent [kV]
 */
double esd_hbm_to_iec_equivalent(double hbm_voltage_v);

/**
 * @brief Convert IEC level to approximate HBM equivalent.
 */
double esd_iec_to_hbm_equivalent(double iec_kv);

/**
 * @brief Get cross-standard equivalence mapping.
 *
 * Provides approximate mappings between IEC system-level,
 * HBM device-level, and ISO 10605 automotive ESD.
 *
 * @param iec_level_kv  IEC 61000-4-2 test level [kV]
 * @param[out] mapping  Cross-standard equivalence
 */
void esd_cross_standard_map(double iec_level_kv,
                             esd_cross_standard_map_t *mapping);

/**
 * @brief Get automotive ESD test parameters for a given severity.
 *
 * ISO 10605 defines test severities up to 25 kV for various
 * discharge networks.
 *
 * @param voltage_kv       Test voltage [kV]
 * @param discharge_network 0=150pF/330Ω, 1=330pF/330Ω, 2=150pF/2000Ω
 * @param[out] params      Automotive test parameters
 * @return                 0 on success
 */
int esd_auto_test_params(double voltage_kv,
                          int discharge_network,
                          esd_auto_test_params_t *params);

/**
 * @brief Check if a given ESD test plan satisfies IEC 61000-4-2
 * requirements for a target immunity level.
 *
 * L6 Canonical Problem: Full compliance verification.
 *
 * Checks:
 *   - Correct test levels applied
 *   - Sufficient number of discharges
 *   - Proper test setup geometry
 *   - Environmental conditions within spec
 *   - Both polarities tested
 *
 * @param plan           Test plan to verify
 * @param target_level   Target immunity level
 * @return               1 if plan meets requirements, 0 otherwise
 */
int esd_compliance_verify_plan(const esd_test_plan_t *plan,
                                esd_test_level_t target_level);

/**
 * @brief Compute the required number of test points for a given
 * EUT size per IEC 61000-4-2 guidelines.
 *
 * L5 Algorithm: Test point estimation.
 *
 * @param eut_width_m   EUT width [m]
 * @param eut_height_m  EUT height [m]
 * @param eut_depth_m   EUT depth [m]
 * @return              Recommended number of test points
 */
int esd_compliance_num_test_points(double eut_width_m,
                                    double eut_height_m,
                                    double eut_depth_m);

/**
 * @brief Medical device ESD immunity requirements.
 *
 * L7 Application: IEC 60601-1-2 (medical electrical equipment)
 * requires ESD immunity at 8 kV contact / 15 kV air plus
 * additional requirements for life-support equipment.
 *
 * @param is_life_support  1 if device is life-support
 * @param[out] contact_level_kv  Required contact discharge level [kV]
 * @param[out] air_level_kv      Required air discharge level [kV]
 * @param[out] additional_reqs   Additional requirements description
 */
void esd_medical_requirements(int is_life_support,
                               double *contact_level_kv,
                               double *air_level_kv,
                               char *additional_reqs);

/**
 * @brief Summary string of ESD standard.
 */
int esd_standard_print(const esd_standard_params_t *params,
                        char *buffer, size_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* ESD_COMPLIANCE_H */
