#ifndef SHIELDING_MEASUREMENT_H
#define SHIELDING_MEASUREMENT_H

#include "shielding_gasket.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===================================================================
 * L1: Measurement standard identifiers
 * =================================================================== */

typedef enum {
    STANDARD_MIL_STD_285 = 0,
    STANDARD_IEEE_299    = 1,
    STANDARD_IEC_61000_4_21 = 2,
    STANDARD_SAE_ARP_1705 = 3,
    STANDARD_EN_50147_1  = 4
} measurement_standard_t;

typedef enum {
    TEST_METHOD_OPEN_FIELD   = 0,
    TEST_METHOD_SHIELDED_ROOM = 1,
    TEST_METHOD_REVERB_CHAMBER = 2,
    TEST_METHOD_GTEM_CELL     = 3,
    TEST_METHOD_TRIAXIAL      = 4,
    TEST_METHOD_NEAR_FIELD_PROBE = 5
} test_method_t;

/** Measurement setup configuration */
typedef struct {
    measurement_standard_t standard;
    test_method_t method;
    double tx_antenna_gain_dbi;
    double rx_antenna_gain_dbi;
    double antenna_separation_m;
    double tx_power_dbm;
    double rx_noise_floor_dbm;
    double cable_loss_db;
    double preamp_gain_db;
    double dynamic_range_db;
    double calibration_offset_db;
} measurement_setup_t;

/* ===================================================================
 * L5: Measurement uncertainty analysis
 * =================================================================== */

/**
 * @brief Compute combined standard uncertainty for SE measurement.
 * Sources: antenna calibration, cable loss, site imperfections,
 *          receiver linearity, alignment error.
 *
 * @param setup  Measurement setup description
 * @param freq_hz  Frequency [Hz]
 * @return Combined standard uncertainty [dB] (k=1)
 */
double shielding_measurement_uncertainty(const measurement_setup_t *setup, double freq_hz);

/**
 * @brief Estimate the minimum reliably measurable SE.
 * SE_min = noise_floor - max_signal + system_gain + margin
 *
 * @param setup  Measurement setup
 * @return Minimum measurable SE [dB]
 */
double shielding_minimum_measurable_se(const measurement_setup_t *setup);

/**
 * @brief Estimate the maximum reliably measurable SE.
 * Limited by system leakage through cables, connectors, seams.
 *
 * @param setup  Measurement setup
 * @param freq_hz  Frequency [Hz]
 * @return Maximum measurable SE [dB]
 */
double shielding_maximum_measurable_se(const measurement_setup_t *setup, double freq_hz);

/**
 * @brief Check if a given SE value can be measured with specified confidence.
 * @param setup      Measurement setup
 * @param target_se_db  Target SE to verify [dB]
 * @param freq_hz    Frequency [Hz]
 * @param confidence_level  0.68=1sigma, 0.95=2sigma, 0.99=3sigma
 * @return 1 if measurable, 0 if out of range
 */
int shielding_is_se_measurable(const measurement_setup_t *setup,
    double target_se_db, double freq_hz, double confidence_level);

/**
 * @brief Build a default measurement setup per IEEE-299.
 * @param setup  Output: filled setup struct
 */
void measurement_setup_ieee_299_default(measurement_setup_t *setup);

#ifdef __cplusplus
}
#endif
#endif /* SHIELDING_MEASUREMENT_H */
