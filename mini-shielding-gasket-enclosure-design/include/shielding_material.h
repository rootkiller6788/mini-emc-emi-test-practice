#ifndef SHIELDING_MATERIAL_H
#define SHIELDING_MATERIAL_H

#include "shielding_gasket.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===================================================================
 * L1: Extended material properties for detailed shielding analysis
 * =================================================================== */

/** Temperature-dependent material properties */
typedef struct {
    shielding_material_t base;
    double temp_coeff_resistivity;    /**< TCR [1/K] for conductivity correction */
    double curie_temperature_c;        /**< Curie temp [deg C] (magnetic materials) */
    double melting_point_c;            /**< Melting point [deg C] */
    double thermal_conductivity_w_mk;  /**< Thermal conductivity [W/(m*K)] */
    double youngs_modulus_gpa;         /**< Young's modulus [GPa] */
    double yield_strength_mpa;         /**< Yield strength [MPa] */
    double corrosion_resistance;       /**< 0=poor to 10=excellent */
    int is_rohs_compliant;             /**< 1 if RoHS compliant */
    int is_reach_compliant;            /**< 1 if REACH compliant */
} material_extended_t;

/** Comparative shielding material grade */
typedef enum {
    GRADE_COMMERCIAL  = 0,  /**< Standard commercial quality */
    GRADE_MILITARY    = 1,  /**< MIL-spec quality */
    GRADE_AEROSPACE   = 2,  /**< Aerospace grade */
    GRADE_MEDICAL     = 3,  /**< Medical/biocompatible grade */
    GRADE_HIGH_TEMP   = 4   /**< High-temperature grade */
} material_grade_t;

/* ===================================================================
 * L2: Material selection and comparison functions
 * =================================================================== */

/**
 * @brief Get extended material properties including mechanical and thermal.
 * Combines EM properties from base database with mechanical/thermal data.
 *
 * @param id       Material identifier
 * @param extended Output: filled extended material struct
 * @return 0 on success, -1 on error
 */
int shielding_material_get_extended(shielding_material_id_t id, material_extended_t *extended);

/**
 * @brief Compute conductivity at elevated temperature.
 * sigma(T) = sigma_20C / (1 + alpha * (T - 20))
 * where alpha = TCR [1/K]
 *
 * @param material    Base material at 20C
 * @param temperature_c  Operating temperature [deg C]
 * @return Conductivity at temperature [S/m]
 */
double shielding_conductivity_at_temperature(const shielding_material_t *material,
                                              double temperature_c);

/**
 * @brief Compute relative permeability at elevated temperature.
 * For ferromagnetic materials, mu_r drops sharply near Curie point.
 * (Simplified linear model below Curie, mu_r ~ 1 above)
 *
 * @param material    Material with base mu_r
 * @param temperature_c  Operating temperature [deg C]
 * @return Relative permeability at temperature
 */
double shielding_permeability_at_temperature(const shielding_material_t *material,
                                              double temperature_c);

/**
 * @brief Compare two materials for a specific shielding application.
 * Returns a relative score (higher = better) based on weighted criteria.
 *
 * @param mat1, mat2  Materials to compare
 * @param freq_hz     Frequency [Hz]
 * @param thickness_m Shield thickness [m]
 * @param field_type  Incident field type
 * @return Positive if mat1 is better, negative if mat2 is better
 */
double shielding_material_compare(const shielding_material_t *mat1,
    const shielding_material_t *mat2, double freq_hz, double thickness_m,
    se_field_type_t field_type);

/**
 * @brief Check if material is ferromagnetic (mu_r >> 1).
 * @param material  Material to check
 * @return 1 if ferromagnetic, 0 if non-magnetic
 */
int shielding_material_is_ferromagnetic(const shielding_material_t *material);

/**
 * @brief Return the best material for a given frequency range.
 * Low freq (< 1kHz): high mu_r materials (mu-metal, steel)
 * Mid freq (1kHz-10MHz): moderate mu_r, high sigma
 * High freq (>10MHz): high sigma (copper, aluminum)
 *
 * @param freq_hz    Frequency of concern [Hz]
 * @param field_type Field type
 * @param result     Output: recommended material
 * @return 0 on success
 */
int shielding_material_recommend_for_frequency(double freq_hz,
    se_field_type_t field_type, shielding_material_t *result);

#ifdef __cplusplus
}
#endif
#endif /* SHIELDING_MATERIAL_H */
