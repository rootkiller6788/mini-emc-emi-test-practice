/**
 * @file esd_waveform.h
 * @brief ESD current waveform parameters per IEC 61000-4-2
 *
 * Implements the standard ESD gun contact/air discharge current
 * waveform model. Reference: IEC 61000-4-2:2008, Section 6.1.
 *
 * Knowledge coverage:
 *   L1 - Definitions: ESD test levels, waveform parameters
 *   L4 - Fundamental Laws: IEC 61000-4-2 waveform equation
 *
 * Course mapping: MIT 6.630 EM Waves (pulse propagation),
 *                 ETH 227-0455 (high-frequency EM)
 */

#ifndef ESD_WAVEFORM_H
#define ESD_WAVEFORM_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── IEC 61000-4-2 Standard Test Levels ────────────────────────────
 *
 * L1 Definition: ESD immunity test levels per IEC 61000-4-2:2008
 *
 * Contact discharge levels (preferred method):
 *   Level 1: 2 kV  → Peak current 7.5 A
 *   Level 2: 4 kV  → Peak current 15 A
 *   Level 3: 6 kV  → Peak current 22.5 A
 *   Level 4: 8 kV  → Peak current 30 A
 *
 * Air discharge levels:
 *   Level 1: 2 kV, Level 2: 4 kV, Level 3: 8 kV, Level 4: 15 kV
 */

/** ESD discharge type */
typedef enum {
    ESD_DISCHARGE_CONTACT = 0,  /**< Contact discharge (direct metal contact) */
    ESD_DISCHARGE_AIR     = 1,  /**< Air discharge (spark through air gap) */
    ESD_DISCHARGE_INDIRECT = 2  /**< Indirect discharge (to coupling plane) */
} esd_discharge_type_t;

/** IEC 61000-4-2 test severity levels */
typedef enum {
    ESD_LEVEL_1 = 0,  /**< Level 1: 2 kV contact / 2 kV air */
    ESD_LEVEL_2 = 1,  /**< Level 2: 4 kV contact / 4 kV air */
    ESD_LEVEL_3 = 2,  /**< Level 3: 6 kV contact / 8 kV air */
    ESD_LEVEL_4 = 3,  /**< Level 4: 8 kV contact / 15 kV air */
    ESD_LEVEL_X = 4   /**< Special/custom level (user-defined voltage) */
} esd_test_level_t;

/**
 * @brief ESD waveform specification per IEC 61000-4-2
 *
 * Defines the four key time-domain parameters of the ESD current
 * pulse that characterize a standard-compliant discharge.
 */
typedef struct {
    double voltage_kv;          /**< Charging voltage in kV */
    double peak_current_a;      /**< First peak current Ip in amperes */
    double rise_time_ns;        /**< Rise time tr (0.7-1.0 ns for contact) */
    double current_at_30ns_a;   /**< Current at t = 30 ns */
    double current_at_60ns_a;   /**< Current at t = 60 ns */
    esd_discharge_type_t type;  /**< Contact or air discharge */
    esd_test_level_t level;     /**< Severity level 1-4 */
} esd_waveform_spec_t;

/**
 * @brief Sampled ESD current waveform
 *
 * Represents a measured or simulated ESD current pulse as a
 * time-series of I(t) values. Used for waveform analysis,
 * parameter extraction, and compliance checking.
 */
typedef struct {
    double *time_ns;       /**< Array of time points [ns] */
    double *current_a;     /**< Array of current values [A] */
    size_t num_samples;    /**< Number of time samples */
    double sample_rate_ghz; /**< Sampling rate in GHz */
} esd_waveform_data_t;

/**
 * @brief IEC 61000-4-2 compliance check result
 */
typedef struct {
    int passes_rise_time;          /**< 1 if rise time within spec */
    int passes_peak_current;       /**< 1 if peak current within spec */
    int passes_current_30ns;       /**< 1 if I(30ns) within spec */
    int passes_current_60ns;       /**< 1 if I(60ns) within spec */
    int overall_pass;              /**< 1 if all parameters pass */
    double measured_rise_time_ns;
    double measured_peak_current_a;
    double measured_current_30ns;
    double measured_current_60ns;
    double rise_time_tolerance_pct;
    double peak_current_tolerance_pct;
} esd_compliance_result_t;

/* ─── Mathematical model of ESD current waveform ───────────────────
 *
 * L4 Theorem: IEC 61000-4-2 analytical waveform model
 *
 * The standard ESD contact discharge current can be approximated by
 * a sum of two exponentials (Heidler function variant):
 *
 *   I(t) = I1 * (t/τ1)^n / (1 + (t/τ1)^n) * exp(-t/τ2)
 *        + I2 * (t/τ3)^n / (1 + (t/τ3)^n) * exp(-t/τ4)
 *
 * where:
 *   I1, I2  = current amplitudes
 *   τ1, τ3  = rise time constants
 *   τ2, τ4  = decay time constants
 *   n       = order (typically 1.8-2.0)
 *
 * Alternative simpler model (double exponential):
 *   I(t) = I0 * [exp(-t/τ_fall) - exp(-t/τ_rise)]
 */

/** Parameters for the Heidler-type ESD waveform function */
typedef struct {
    double i1_a;      /**< First current amplitude [A] */
    double i2_a;      /**< Second current amplitude [A] */
    double tau1_ns;   /**< Rise time constant 1 [ns] */
    double tau2_ns;   /**< Decay time constant 1 [ns] */
    double tau3_ns;   /**< Rise time constant 2 [ns] */
    double tau4_ns;   /**< Decay time constant 2 [ns] */
    double n_exp;     /**< Exponent n (1.8-2.0 typical) */
} esd_heidler_params_t;

/** Parameters for the double-exponential waveform model */
typedef struct {
    double i0_a;       /**< Amplitude factor [A] */
    double tau_rise_ns; /**< Rise time constant [ns] */
    double tau_fall_ns; /**< Fall time constant [ns] */
} esd_double_exp_params_t;

/* ─── API: Waveform computation ────────────────────────────────── */

/**
 * @brief Get the standard IEC waveform specification for a given level.
 *
 * Returns the nominal waveform parameters as defined in IEC 61000-4-2.
 * L2 Concept: Contact discharge current waveform parameters scale
 * linearly with charging voltage.
 *
 * @param level  Severity level 1-4 (ESD_LEVEL_1 to ESD_LEVEL_4)
 * @param type   ESD_DISCHARGE_CONTACT or ESD_DISCHARGE_AIR
 * @return       Populated esd_waveform_spec_t with standard values
 */
esd_waveform_spec_t esd_waveform_spec_standard(esd_test_level_t level,
                                                esd_discharge_type_t type);

/**
 * @brief Get the voltage in kV for a given severity level and discharge type.
 * L1 Definition: Maps test level enumeration to actual voltage.
 */
double esd_level_voltage_kv(esd_test_level_t level, esd_discharge_type_t type);

/**
 * @brief Get the nominal peak current for a given contact discharge voltage.
 *
 * L4 Theorem: For contact discharge, Ip ≈ V * 3.75 A/kV
 * (derived from IEC 61000-4-2 table: 7.5A@2kV, 15A@4kV, 22.5A@6kV, 30A@8kV)
 *
 * @param voltage_kv  Charging voltage in kV
 * @return            Expected peak current in amperes
 */
double esd_peak_current_from_voltage(double voltage_kv);

/**
 * @brief Compute Heidler-function ESD waveform I(t) at time t.
 *
 * Implements the four-parameter IEC waveform model.
 * L5 Algorithm: Heidler function evaluation with protective
 * small-t handling to avoid division by zero.
 *
 * @param t_ns    Time in nanoseconds
 * @param params  Heidler parameters
 * @return        Current in amperes at time t
 */
double esd_heidler_waveform(double t_ns, const esd_heidler_params_t *params);

/**
 * @brief Compute double-exponential waveform I(t) at time t.
 *
 * I(t) = I0 * [exp(-t/τf) - exp(-t/τr)]
 *
 * @param t_ns    Time in nanoseconds
 * @param params  Double-exponential parameters
 * @return        Current in amperes at time t
 */
double esd_double_exp_waveform(double t_ns,
                                const esd_double_exp_params_t *params);

/**
 * @brief Generate a sampled ESD waveform using Heidler model.
 *
 * Creates a time series from t=0 to t_end with given number of samples.
 * L5 Algorithm: Waveform discretization for numerical analysis.
 *
 * @param spec       Standard waveform specification
 * @param t_end_ns   End time for sampling window [ns]
 * @param num_samples Number of time points
 * @param[out] data  Allocated esd_waveform_data_t (caller frees via
 *                   esd_waveform_data_free)
 * @return           0 on success, -1 on error
 */
int esd_waveform_generate_heidler(const esd_waveform_spec_t *spec,
                                   double t_end_ns,
                                   size_t num_samples,
                                   esd_waveform_data_t *data);

/**
 * @brief Generate waveform using double-exponential model.
 */
int esd_waveform_generate_double_exp(const esd_waveform_spec_t *spec,
                                      double t_end_ns,
                                      size_t num_samples,
                                      esd_waveform_data_t *data);

/**
 * @brief Generate the ideal IEC 61000-4-2 waveform with auto-fitted
 * parameters.
 *
 * Uses pre-computed Heidler parameters calibrated to match
 * the IEC standard at each test level.
 */
int esd_waveform_generate_iec(const esd_waveform_spec_t *spec,
                               double t_end_ns,
                               size_t num_samples,
                               esd_waveform_data_t *data);

/**
 * @brief Extract waveform parameters from measured data.
 *
 * L5 Algorithm: Extracts rise time (10%-90%), peak current,
 * and I(30ns), I(60ns) from sampled waveform data.
 *
 * @param data    Sampled waveform data
 * @param[out] result  Extracted parameters
 * @return        0 on success, -1 on error
 */
int esd_waveform_extract_params(const esd_waveform_data_t *data,
                                 esd_waveform_spec_t *result);

/**
 * @brief Check if a measured waveform complies with IEC 61000-4-2.
 *
 * Compares extracted parameters against the standard tolerances:
 *   - Rise time: 0.8 ns ± 25% (0.6-1.0 ns)
 *   - Peak current: ±15% of nominal
 *   - I(30ns): ±30% of nominal
 *   - I(60ns): ±30% of nominal
 *
 * @param measured  Measured waveform parameters
 * @param spec      Expected nominal specification
 * @param[out] result  Compliance check result
 * @return          0 on success, -1 on error
 */
int esd_waveform_check_compliance(const esd_waveform_spec_t *measured,
                                   const esd_waveform_spec_t *spec,
                                   esd_compliance_result_t *result);

/**
 * @brief Compute the total charge delivered by an ESD waveform.
 *
 * Q = ∫ I(t) dt over the sampling window.
 * L3 Theorem: Conservation of charge — the total charge must equal
 * C*V of the discharge capacitor (minus losses).
 *
 * @param data   Sampled waveform
 * @return       Total charge in nanocoulombs [nC]
 */
double esd_waveform_total_charge(const esd_waveform_data_t *data);

/**
 * @brief Compute the energy dissipated in a discharge.
 *
 * E = ∫ I(t)² * R dt ≈ R * ∫ I²(t) dt
 *
 * @param data        Sampled waveform
 * @param resistance  Load resistance [Ω]
 * @return            Energy in microjoules [µJ]
 */
double esd_waveform_energy(const esd_waveform_data_t *data,
                            double resistance);

/**
 * @brief Compute instantaneous power at each time point.
 *
 * P(t) = I(t)² * R
 *
 * @param data        Sampled waveform
 * @param resistance  Load resistance [Ω]
 * @param[out] power  Pre-allocated array of length data->num_samples
 * @return            0 on success, -1 on error
 */
int esd_waveform_power(const esd_waveform_data_t *data,
                        double resistance,
                        double *power);

/**
 * @brief Compute rise time (10% to 90%) from sampled waveform.
 *
 * L5 Algorithm: Linear interpolation between sample points to find
 * accurate 10% and 90% crossing times.
 *
 * @param data  Sampled waveform
 * @return      Rise time in nanoseconds, -1 if not found
 */
double esd_waveform_rise_time(const esd_waveform_data_t *data);

/**
 * @brief Compute full-width at half-maximum (FWHM) of the waveform.
 */
double esd_waveform_fwhm(const esd_waveform_data_t *data);

/**
 * @brief Free waveform data memory.
 */
void esd_waveform_data_free(esd_waveform_data_t *data);

/**
 * @brief Get pre-calibrated Heidler parameters for an IEC standard level.
 *
 * These parameters have been calibrated to match the IEC 61000-4-2
 * waveform specification for each level.
 *
 * @param level  ESD test level
 * @param[out] params  Heidler parameters
 * @return       0 on success, -1 if level invalid
 */
int esd_heidler_params_for_level(esd_test_level_t level,
                                  esd_heidler_params_t *params);

/**
 * @brief Print waveform specification to string (for diagnostics).
 */
int esd_waveform_spec_print(const esd_waveform_spec_t *spec,
                             char *buffer, size_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* ESD_WAVEFORM_H */
