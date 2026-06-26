/* shielding_material.c -- Material property library for EMI shielding design
 *
 * L1: Definitions - 11 material entries with measured properties
 * L2: Core Concepts - Skin depth, wave impedance
 *
 * Reference: Paul (2006) "Introduction to EMC" Ch.7
 *            Ott (2009) "Electromagnetic Compatibility Engineering" Ch.6
 *            MIL-STD-285 / IEEE-299 shielding test data
 *            ASM Handbook Vol.2: Properties of Metals
 */

#include <math.h>
#include <string.h>
#include "shielding_gasket.h"

/* Internal material database.
 * Conductivity values from NIST and ASM Handbook.
 * Permeability values from manufacturer datasheets (MuShield, Magnetic Shield Corp).
 * All values at 20 deg C unless otherwise noted. */
static const shielding_material_t material_db[] = {
    { MATERIAL_COPPER, "Copper (annealed)",
      5.80e7, 1.0, 1.0, 8960.0, 0.025, 3.0, 1.0 },
    { MATERIAL_ALUMINUM, "Aluminum 6061-T6",
      2.50e7, 1.0, 1.0, 2700.0, 0.5, 6.0, 0.7 },
    { MATERIAL_STEEL_1008, "Steel 1008 (low carbon)",
      5.60e6, 1000.0, 1.0, 7870.0, 0.5, 3.0, 0.4 },
    { MATERIAL_MU_METAL, "Mu-Metal (80% Ni, 15% Fe)",
      1.82e6, 20000.0, 1.0, 8750.0, 0.1, 1.5, 15.0 },
    { MATERIAL_SILVER_EPOXY, "Silver-filled epoxy",
      1.00e5, 1.0, 4.5, 2500.0, 0.025, 0.5, 5.0 },
    { MATERIAL_NICKEL, "Nickel (electrolytic)",
      1.43e7, 600.0, 1.0, 8908.0, 0.025, 1.0, 3.0 },
    { MATERIAL_TIN_PLATED, "Tin-plated steel",
      5.00e6, 1.0, 1.0, 7870.0, 0.5, 2.0, 0.5 },
    { MATERIAL_STAINLESS_304, "Stainless Steel 304",
      1.39e6, 1.02, 1.0, 8000.0, 0.5, 4.0, 1.2 },
    { MATERIAL_BRASS, "Brass (70Cu/30Zn)",
      1.56e7, 1.0, 1.0, 8530.0, 0.1, 3.0, 1.5 },
    { MATERIAL_ZINC, "Zinc (die-cast)",
      1.69e7, 1.0, 1.0, 7140.0, 1.0, 5.0, 0.6 },
    { MATERIAL_CONDUCTIVE_PAINT, "Conductive paint (Ni/Cu)",
      1.00e4, 1.0, 3.0, 1500.0, 0.025, 0.15, 0.3 },
};

#define MATERIAL_DB_SIZE (sizeof(material_db) / sizeof(material_db[0]))

int shielding_material_get(shielding_material_id_t id, shielding_material_t *mat) {
    if (!mat) return -1;
    for (size_t i = 0; i < MATERIAL_DB_SIZE; i++) {
        if (material_db[i].id == id) {
            *mat = material_db[i];
            return 0;
        }
    }
    return -1;
}

double shielding_skin_depth(const shielding_material_t *material, double freq_hz) {
    /* L4: Skin depth formula
     * delta = 1 / sqrt(pi * f * mu_0 * mu_r * sigma) [m]
     *
     * Derivation: From Maxwell's equations in a good conductor,
     * the propagation constant gamma = (1+j)/delta where
     * delta = sqrt(2/(omega*mu*sigma)).
     * Simplifying: delta = 1/sqrt(pi*f*mu_0*mu_r*sigma)
     *
     * Reference: Paul Eq.7.13; Jackson "Classical Electrodynamics" Ch.7.7
     */
    if (!material || freq_hz <= 0.0) return -1.0;

    double sigma = material->conductivity;
    double mu = MU0 * material->relative_permeability;

    if (sigma <= 0.0 || mu <= 0.0) return -1.0;

    double delta = 1.0 / sqrt(M_PI * freq_hz * mu * sigma);
    return delta;
}

double shielding_wave_impedance(const shielding_material_t *material, double freq_hz) {
    /* L2: Wave impedance in a conducting medium
     *
     * For a good conductor (sigma >> omega*epsilon):
     *   |eta| = sqrt(2*pi*f*mu / sigma) [ohm]
     *
     * For a lossless dielectric (sigma ~ 0):
     *   eta = sqrt(mu/epsilon) = 377/sqrt(epsilon_r) [ohm]
     *
     * This determines the reflection coefficient at the
     * air-conductor interface: Gamma = (eta - eta_0)/(eta + eta_0)
     *
     * Reference: Paul Eq.7.18; Cheng "Field and Wave EM" Ch.8
     */
    if (!material || freq_hz <= 0.0) return -1.0;

    double sigma = material->conductivity;
    double mu = MU0 * material->relative_permeability;
    double eps = EPSILON0 * material->relative_permittivity;

    if (sigma <= 0.0) {
        /* Lossless dielectric approximation */
        if (eps <= 0.0) return -1.0;
        return sqrt(mu / eps);
    }

    /* Check if good conductor: sigma >> omega*epsilon */
    double omega = 2.0 * M_PI * freq_hz;
    if (sigma > 10.0 * omega * eps) {
        /* Good conductor approximation */
        return sqrt(2.0 * M_PI * freq_hz * mu / sigma);
    }

    /* General case: complex wave impedance magnitude */
    double tan_delta = sigma / (omega * eps);
    double magnitude = sqrt(mu / eps) / pow(1.0 + tan_delta*tan_delta, 0.25);
    return magnitude;
}

#include "shielding_material.h"
int shielding_material_get_extended(shielding_material_id_t id, material_extended_t *extended) {
    if (!extended) return -1;
    if (shielding_material_get(id, &extended->base) != 0) return -1;
    switch (id) {
        case MATERIAL_COPPER:
            extended->temp_coeff_resistivity = 3.9e-3;
            extended->curie_temperature_c = -1;
            extended->melting_point_c = 1085.0;
            extended->thermal_conductivity_w_mk = 401.0;
            extended->youngs_modulus_gpa = 117.0;
            extended->yield_strength_mpa = 70.0;
            extended->corrosion_resistance = 7.0;
            extended->is_rohs_compliant = 1;
            extended->is_reach_compliant = 1;
            break;
        case MATERIAL_ALUMINUM:
            extended->temp_coeff_resistivity = 3.9e-3;
            extended->curie_temperature_c = -1;
            extended->melting_point_c = 660.0;
            extended->thermal_conductivity_w_mk = 167.0;
            extended->youngs_modulus_gpa = 69.0;
            extended->yield_strength_mpa = 276.0;
            extended->corrosion_resistance = 8.0;
            extended->is_rohs_compliant = 1;
            extended->is_reach_compliant = 1;
            break;
        case MATERIAL_STEEL_1008:
            extended->temp_coeff_resistivity = 6.5e-3;
            extended->curie_temperature_c = 770.0;
            extended->melting_point_c = 1530.0;
            extended->thermal_conductivity_w_mk = 52.0;
            extended->youngs_modulus_gpa = 207.0;
            extended->yield_strength_mpa = 250.0;
            extended->corrosion_resistance = 3.0;
            extended->is_rohs_compliant = 1;
            extended->is_reach_compliant = 1;
            break;
        case MATERIAL_MU_METAL:
            extended->temp_coeff_resistivity = 1.0e-3;
            extended->curie_temperature_c = 460.0;
            extended->melting_point_c = 1450.0;
            extended->thermal_conductivity_w_mk = 29.0;
            extended->youngs_modulus_gpa = 200.0;
            extended->yield_strength_mpa = 350.0;
            extended->corrosion_resistance = 5.0;
            extended->is_rohs_compliant = 1;
            extended->is_reach_compliant = 1;
            break;
        case MATERIAL_NICKEL:
            extended->temp_coeff_resistivity = 6.0e-3;
            extended->curie_temperature_c = 358.0;
            extended->melting_point_c = 1455.0;
            extended->thermal_conductivity_w_mk = 90.0;
            extended->youngs_modulus_gpa = 207.0;
            extended->yield_strength_mpa = 148.0;
            extended->corrosion_resistance = 7.0;
            extended->is_rohs_compliant = 1;
            extended->is_reach_compliant = 1;
            break;
        case MATERIAL_STAINLESS_304:
            extended->temp_coeff_resistivity = 1.0e-3;
            extended->curie_temperature_c = -1;
            extended->melting_point_c = 1450.0;
            extended->thermal_conductivity_w_mk = 16.0;
            extended->youngs_modulus_gpa = 193.0;
            extended->yield_strength_mpa = 215.0;
            extended->corrosion_resistance = 9.0;
            extended->is_rohs_compliant = 1;
            extended->is_reach_compliant = 1;
            break;
        case MATERIAL_BRASS:
            extended->temp_coeff_resistivity = 1.5e-3;
            extended->curie_temperature_c = -1;
            extended->melting_point_c = 940.0;
            extended->thermal_conductivity_w_mk = 120.0;
            extended->youngs_modulus_gpa = 105.0;
            extended->yield_strength_mpa = 140.0;
            extended->corrosion_resistance = 6.0;
            extended->is_rohs_compliant = 1;
            extended->is_reach_compliant = 1;
            break;
        case MATERIAL_ZINC:
            extended->temp_coeff_resistivity = 3.7e-3;
            extended->curie_temperature_c = -1;
            extended->melting_point_c = 420.0;
            extended->thermal_conductivity_w_mk = 116.0;
            extended->youngs_modulus_gpa = 96.0;
            extended->yield_strength_mpa = 80.0;
            extended->corrosion_resistance = 5.0;
            extended->is_rohs_compliant = 1;
            extended->is_reach_compliant = 1;
            break;
        default: return -1;
    }
    return 0;
}

double shielding_conductivity_at_temperature(const shielding_material_t *material,
                                              double temperature_c) {
    if (!material) return -1.0;
    material_extended_t ext;
    if (shielding_material_get_extended(material->id, &ext) != 0)
        return material->conductivity;
    double alpha = ext.temp_coeff_resistivity;
    double delta_T = temperature_c - 20.0;
    return material->conductivity / (1.0 + alpha * delta_T);
}

double shielding_permeability_at_temperature(const shielding_material_t *material,
                                              double temperature_c) {
    if (!material) return -1.0;
    if (material->relative_permeability <= 1.0) return 1.0;
    material_extended_t ext;
    if (shielding_material_get_extended(material->id, &ext) != 0)
        return material->relative_permeability;
    if (ext.curie_temperature_c > 0.0 && temperature_c >= ext.curie_temperature_c)
        return 1.0;
    return material->relative_permeability;
}

double shielding_material_compare(const shielding_material_t *mat1,
    const shielding_material_t *mat2, double freq_hz, double thickness_m,
    se_field_type_t field_type) {
    if (!mat1 || !mat2 || freq_hz <= 0.0) return 0.0;
    shield_layer_t l1 = {*mat1, thickness_m, 1.0};
    shield_layer_t l2 = {*mat2, thickness_m, 1.0};
    se_result_t r1 = shielding_effectiveness_single_layer(&l1, freq_hz, field_type, 1.0);
    se_result_t r2 = shielding_effectiveness_single_layer(&l2, freq_hz, field_type, 1.0);
    double score_se = r1.total_se_db - r2.total_se_db;
    double score_cost = mat2->cost_factor - mat1->cost_factor;
    double score_weight = mat2->density - mat1->density;
    double score_thk = (mat1->thickness_max_mm - mat2->thickness_max_mm) * 0.1;
    return score_se * 0.5 + score_cost * 0.2 + score_weight * 0.2 + score_thk * 0.1;
}

int shielding_material_is_ferromagnetic(const shielding_material_t *material) {
    if (!material) return 0;
    return material->relative_permeability > 10.0 ? 1 : 0;
}

int shielding_material_recommend_for_frequency(double freq_hz,
    se_field_type_t field_type, shielding_material_t *result) {
    if (!result || freq_hz <= 0.0) return -1;
    shielding_material_id_t rec;
    if (freq_hz < 1.0e3 && field_type == SE_FIELD_NEAR_H)
        rec = MATERIAL_MU_METAL;
    else if (freq_hz < 1.0e6 && field_type == SE_FIELD_NEAR_H)
        rec = MATERIAL_STEEL_1008;
    else if (freq_hz < 10.0e6)
        rec = MATERIAL_NICKEL;
    else if (freq_hz < 1.0e9)
        rec = MATERIAL_COPPER;
    else
        rec = MATERIAL_ALUMINUM;
    return shielding_material_get(rec, result);
}
