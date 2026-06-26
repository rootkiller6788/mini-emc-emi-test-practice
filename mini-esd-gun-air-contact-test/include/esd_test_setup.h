/**
 * @file esd_test_setup.h
 * @brief ESD test setup configuration per IEC 61000-4-2
 *
 * Implements the physical test setup including:
 *   - Ground reference plane (GRP)
 *   - Horizontal coupling plane (HCP)
 *   - Vertical coupling plane (VCP)
 *   - 470 kΩ bleed resistors
 *   - Equipment under test (EUT) positioning
 *
 * Reference: IEC 61000-4-2:2008 Section 7 (Test setup)
 *
 * Knowledge coverage:
 *   L1 - Definitions: GRP, HCP, VCP, coupling discharge
 *   L2 - Core Concepts: Indirect ESD coupling, field coupling
 *   L6 - Canonical Problems: Standard compliance test setup
 */

#ifndef ESD_TEST_SETUP_H
#define ESD_TEST_SETUP_H

#include "esd_waveform.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── IEC 61000-4-2 Test Setup Geometry ───────────────────────────
 *
 * L1 Definition: The standard ESD test setup consists of:
 *
 * Ground Reference Plane (GRP):
 *   - Minimum 1.6 m × 0.8 m copper or aluminum sheet
 *   - Connected to protective earth
 *   - Extends at least 0.5 m beyond EUT on all sides
 *
 * Horizontal Coupling Plane (HCP):
 *   - 1.6 m × 0.8 m metal sheet
 *   - Placed on 0.5 mm insulating support on GRP
 *   - Connected to GRP via two 470 kΩ bleed resistors
 *
 * Vertical Coupling Plane (VCP):
 *   - 0.5 m × 0.5 m metal sheet
 *   - Placed 0.1 m from EUT front face
 *   - Connected to GRP via two 470 kΩ bleed resistors
 */

/** ESD discharge application point type */
typedef enum {
    ESD_POINT_EUT_DIRECT = 0,     /**< Direct discharge to EUT */
    ESD_POINT_HCP_INDIRECT = 1,   /**< Indirect discharge to HCP */
    ESD_POINT_VCP_INDIRECT = 2,   /**< Indirect discharge to VCP */
    ESD_POINT_GRP_DIRECT = 3      /**< Direct discharge to GRP */
} esd_discharge_point_t;

/** EUT operating condition during test */
typedef enum {
    EUT_MODE_NORMAL = 0,          /**< Normal operating mode */
    EUT_MODE_STANDBY = 1,         /**< Standby / sleep mode */
    EUT_MODE_MAX_LOAD = 2,        /**< Maximum load condition */
    EUT_MODE_IDLE = 3             /**< Idle / minimum activity */
} esd_eut_mode_t;

/** ESD test result per discharge */
typedef enum {
    ESD_RESULT_PASS_A = 0,        /**< Normal performance within spec */
    ESD_RESULT_PASS_B = 1,        /**< Temporary degradation, self-recoverable */
    ESD_RESULT_PASS_C = 2,        /**< Temporary degradation, needs operator intervention */
    ESD_RESULT_FAIL_D = 3         /**< Permanent damage or system crash */
} esd_test_result_t;

/**
 * @brief Physical test setup geometry
 *
 * L1 Definition: Standard IEC 61000-4-2 table-top test setup dimensions.
 */
typedef struct {
    double grp_width_m;     /**< Ground reference plane width [m] (min 0.8) */
    double grp_length_m;    /**< Ground reference plane length [m] (min 1.6) */
    double hcp_width_m;     /**< Horizontal coupling plane width [m] */
    double hcp_length_m;    /**< Horizontal coupling plane length [m] */
    double hcp_height_m;    /**< HCP height above GRP [m] (0.8 m standard) */
    double vcp_width_m;     /**< Vertical coupling plane width [m] (0.5 m) */
    double vcp_height_m;    /**< Vertical coupling plane height [m] (0.5 m) */
    double vcp_distance_m;  /**< VCP distance from EUT front [m] (0.1 m) */
    double insulator_thickness_mm; /**< Insulator thickness [mm] (0.5 mm) */
    double bleed_resistor_kohm;    /**< Bleed resistor value [kΩ] (470 kΩ) */
} esd_test_geometry_t;

/**
 * @brief A single ESD discharge test point definition
 */
typedef struct {
    int point_id;                  /**< Test point identifier */
    esd_discharge_point_t type;    /**< Direct/indirect point type */
    double x_m;                    /**< X coordinate on EUT/plane [m] */
    double y_m;                    /**< Y coordinate on EUT/plane [m] */
    double z_m;                    /**< Z coordinate / height [m] */
    char description[128];         /**< Human-readable point description */
} esd_test_point_t;

/**
 * @brief ESD test plan for a complete compliance test
 *
 * L6 Canonical Problem: Full IEC 61000-4-2 compliance test execution.
 * Standard test plan includes:
 *   - At least 10 discharges per test point
 *   - At least 1 second between discharges
 *   - Both polarities (+/-)
 *   - All 4 severity levels
 */
typedef struct {
    esd_test_geometry_t geometry;        /**< Test setup geometry */
    size_t num_test_points;              /**< Number of test points */
    esd_test_point_t *test_points;       /**< Array of test points */
    esd_test_level_t levels[4];          /**< Severity levels to test */
    size_t num_levels;                   /**< Number of levels */
    int discharges_per_point;            /**< Discharges per point (min 10) */
    double interval_seconds;            /**< Minimum interval between discharges */
    int test_both_polarities;            /**< 1 = test + and -, 0 = single */
    esd_discharge_type_t discharge_type; /**< Contact or air */
    esd_eut_mode_t eut_mode;             /**< EUT operating mode */
    double ambient_temp_c;               /**< Ambient temperature [°C] */
    double ambient_humidity_pct;         /**< Relative humidity [%] */
    double atmospheric_pressure_hpa;     /**< Atmospheric pressure [hPa] */
} esd_test_plan_t;

/**
 * @brief Single discharge event record
 */
typedef struct {
    int point_id;                /**< Test point ID */
    double voltage_kv;           /**< Applied voltage [kV] */
    esd_discharge_type_t type;   /**< Discharge type */
    int polarity_positive;       /**< 1 = positive, 0 = negative */
    esd_test_result_t result;    /**< Test result */
    double peak_current_a;       /**< Measured peak current [A] */
    double rise_time_ns;         /**< Measured rise time [ns] */
    char notes[256];             /**< Observation notes */
} esd_discharge_record_t;

/**
 * @brief Complete ESD test report data
 */
typedef struct {
    esd_test_plan_t plan;              /**< Test plan executed */
    size_t num_records;               /**< Number of discharge records */
    esd_discharge_record_t *records;   /**< Array of discharge records */
    int overall_pass;                  /**< 1 if EUT passed all tests */
    double test_duration_minutes;     /**< Total test duration */
    char eut_identifier[64];           /**< EUT identification */
    char test_standard[64];            /**< Test standard reference */
    char test_engineer[64];            /**< Test engineer name */
} esd_test_report_t;

/* ─── API: Test setup configuration ──────────────────────────── */

/**
 * @brief Get standard IEC 61000-4-2 table-top test geometry.
 *
 * Returns the nominal dimensions as specified in the standard.
 * L1 Definition: Reference test setup geometry.
 */
esd_test_geometry_t esd_test_geometry_standard(void);

/**
 * @brief Validate if a test geometry meets IEC 61000-4-2 minimum
 * requirements.
 *
 * Checks:
 *   - GRP ≥ 0.8 m × 1.6 m
 *   - HCP height = 0.8 m ± 0.01 m
 *   - Insulator thickness = 0.5 mm ± 0.05 mm
 *   - Bleed resistor = 470 kΩ ± 10%
 *
 * @param geom  Test geometry to validate
 * @return      1 if valid, 0 if not
 */
int esd_test_geometry_is_valid(const esd_test_geometry_t *geom);

/**
 * @brief Create a standard IEC 61000-4-2 compliance test plan.
 *
 * Generates the default test plan with standard test points:
 *   - EUT front face center
 *   - EUT corners
 *   - HCP edges (4 sides)
 *   - VCP center
 *
 * L6 Canonical Problem: Standard compliance test execution.
 *
 * @param discharge_type  Contact or air
 * @param max_level       Maximum severity level (1-4)
 * @param[out] plan       Populated test plan
 * @return                0 on success
 */
int esd_test_plan_standard(esd_discharge_type_t discharge_type,
                            esd_test_level_t max_level,
                            esd_test_plan_t *plan);

/**
 * @brief Compute minimum number of discharges required for a plan.
 *
 * total = num_points × num_levels × discharges_per_point × polarity_factor
 * where polarity_factor = 2 if test_both_polarities, else 1
 *
 * L5 Algorithm: Test plan sizing calculation.
 */
size_t esd_test_plan_total_discharges(const esd_test_plan_t *plan);

/**
 * @brief Compute estimated test duration in minutes.
 *
 * duration = total_discharges × interval_seconds / 60
 * plus setup and documentation overhead (~15%).
 */
double esd_test_plan_duration(const esd_test_plan_t *plan);

/**
 * @brief Add a test point to an existing plan.
 *
 * Dynamically reallocates the test_points array.
 *
 * @param plan   Test plan to modify
 * @param point  Test point to add
 * @return       0 on success, -1 on allocation error
 */
int esd_test_plan_add_point(esd_test_plan_t *plan,
                             const esd_test_point_t *point);

/**
 * @brief Initialize an empty test report.
 *
 * Allocates records array for the expected number of discharge events.
 */
int esd_test_report_init(const esd_test_plan_t *plan,
                          esd_test_report_t *report,
                          const char *eut_id,
                          const char *engineer);

/**
 * @brief Record a single ESD discharge result.
 */
int esd_test_report_record(esd_test_report_t *report,
                            const esd_discharge_record_t *record);

/**
 * @brief Compute pass/fail statistics from a test report.
 *
 * L5 Algorithm: Statistical summary of ESD test results.
 *
 * @param report          Completed test report
 * @param[out] num_pass_a Count of performance criterion A
 * @param[out] num_pass_b Count of performance criterion B
 * @param[out] num_pass_c Count of performance criterion C
 * @param[out] num_fail   Count of failures
 */
void esd_test_report_statistics(const esd_test_report_t *report,
                                 size_t *num_pass_a,
                                 size_t *num_pass_b,
                                 size_t *num_pass_c,
                                 size_t *num_fail);

/**
 * @brief Determine overall test result from statistics.
 *
 * Per IEC 61000-4-2: If any discharge results in FAIL_D,
 * the EUT fails the immunity test.
 *
 * @param report  Test report
 * @return        1 if overall pass, 0 if any failure
 */
int esd_test_report_overall_pass(const esd_test_report_t *report);

/**
 * @brief Free test plan memory (test_points array).
 */
void esd_test_plan_free(esd_test_plan_t *plan);

/**
 * @brief Free test report memory (records array).
 */
void esd_test_report_free(esd_test_report_t *report);

/**
 * @brief Compute coupling from ESD discharge to nearby trace.
 *
 * L2 Core Concept: ESD current in coupling plane induces voltage
 * in nearby PCB traces through mutual inductance and capacitance.
 *
 * Simplified mutual inductance coupling:
 *   V_induced ≈ M * dI/dt
 *
 * where M is mutual inductance between discharge path and victim trace.
 *
 * @param dI_dt_aps    Rate of change of ESD current [A/ns]
 * @param mutual_l_nh  Mutual inductance [nH]
 * @return             Induced voltage [V]
 */
double esd_coupling_induced_voltage(double dI_dt_aps, double mutual_l_nh);

/**
 * @brief Compute mutual inductance between two parallel conductors.
 *
 * L3: M = (μ₀·l / 2π) * [ln(2l/d) - 1 + d/l]
 *
 * where l = conductor length, d = separation distance.
 *
 * @param length_m          Conductor length [m]
 * @param separation_m      Distance between conductors [m]
 * @return                  Mutual inductance [H]
 */
double esd_mutual_inductance_parallel(double length_m, double separation_m);

/**
 * @brief Compute the attenuation of an ESD pulse through a
 * protection component.
 *
 * L6 Canonical Problem: ESD protection evaluation.
 *
 * @param esd_voltage_kv   Incident ESD voltage [kV]
 * @param clamp_voltage_v  Protection clamp voltage [V]
 * @param series_r_ohm     Series resistance [Ω]
 * @return                 Residual voltage at protected node [V]
 */
double esd_protection_attenuation(double esd_voltage_kv,
                                   double clamp_voltage_v,
                                   double series_r_ohm);

#ifdef __cplusplus
}
#endif

#endif /* ESD_TEST_SETUP_H */
