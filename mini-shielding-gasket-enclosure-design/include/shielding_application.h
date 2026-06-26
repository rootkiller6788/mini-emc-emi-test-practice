#ifndef SHIELDING_APPLICATION_H
#define SHIELDING_APPLICATION_H

#include "shielding_gasket.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===================================================================
 * L7: Application-specific shielding requirements
 * =================================================================== */

/** Industry-specific SE requirements */
typedef struct {
    const char *industry;
    const char *standard;
    double se_30mhz_db;
    double se_100mhz_db;
    double se_1ghz_db;
    double se_10ghz_db;
    int environmental_class;
    int ip_rating_min;
} industry_requirement_t;

typedef enum {
    INDUSTRY_AUTOMOTIVE  = 0,
    INDUSTRY_MEDICAL     = 1,
    INDUSTRY_AEROSPACE   = 2,
    INDUSTRY_TELECOM_5G  = 3,
    INDUSTRY_CONSUMER    = 4,
    INDUSTRY_INDUSTRIAL  = 5,
    INDUSTRY_MILITARY    = 6,
    INDUSTRY_RAILWAY     = 7,
    INDUSTRY_MARINE      = 8,
    INDUSTRY_IOT_EDGE    = 9
} industry_sector_t;

/* ===================================================================
 * L7/L8: Application design functions
 * =================================================================== */

/**
 * @brief Get standard industry SE requirements.
 * @param sector  Industry sector
 * @param req     Output: requirement specification
 * @return 0 on success
 */
int shielding_industry_requirements(industry_sector_t sector, industry_requirement_t *req);

/**
 * @brief Check if an enclosure design meets industry requirements.
 * @param enclosure  Enclosure design
 * @param req        Industry requirements
 * @return 0 if compliant, 1 if marginal, 2 if non-compliant
 */
int shielding_check_compliance(const enclosure_spec_t *enclosure,
    const industry_requirement_t *req);

/**
 * @brief Combined thermal-EMI enclosure optimization.
 * Balances shielding effectiveness against thermal cooling needs.
 * Higher SE typically means less ventilation -> higher internal temp.
 *
 * @param target_se_db    Target SE [dB]
 * @param max_freq_hz     Max frequency [Hz]
 * @param internal_heat_w Internal heat dissipation [W]
 * @param max_temp_rise_c Maximum allowed temperature rise [deg C]
 * @param result          Output: optimized enclosure
 * @return 0 on success
 */
int shielding_thermal_emc_co_design(double target_se_db, double max_freq_hz,
    double internal_heat_w, double max_temp_rise_c, enclosure_spec_t *result);

/**
 * @brief Estimate the ventilation area needed for a given heat load.
 * Uses natural convection model for passively cooled enclosures.
 *
 * Q = h * A * delta_T where h ~ 5 W/(m^2*K) for natural convection
 *
 * @param internal_heat_w   Heat dissipation [W]
 * @param ambient_temp_c    Ambient temperature [deg C]
 * @param max_internal_temp_c  Max internal temperature [deg C]
 * @return Required ventilation area [m^2]
 */
double shielding_ventilation_area_needed(double internal_heat_w,
    double ambient_temp_c, double max_internal_temp_c);

/**
 * @brief Compute thermal resistance of enclosure wall.
 * R_th = t / (k * A)  [K/W]
 *
 * @param enclosure  Enclosure specification
 * @return Thermal resistance [K/W]
 */
double shielding_enclosure_thermal_resistance(const enclosure_spec_t *enclosure);

#ifdef __cplusplus
}
#endif
#endif /* SHIELDING_APPLICATION_H */
