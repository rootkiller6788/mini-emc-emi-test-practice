/* shielding_gasket.c -- Conductive Gasket Modeling and Compression Analysis
 * L1: Definitions - 10 gasket material types with manufacturer data
 * L2: Core Concepts - Compression mechanics, transfer impedance
 * L5: Algorithms - Piecewise compression model, life estimation
 * L6: Canonical Problems - Galvanic compatibility analysis
 * Reference: Laird Technologies EMI Shielding Design Guide
 *            Parker Chomerics EMI Shielding Engineering Handbook
 *            IEC 62153-4-3, SAE ARP 1481, MIL-STD-889B
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shielding_gasket.h"

typedef struct {
    gasket_material_t type;
    const char *name;
    double elastic_modulus_mpa;
    double plateau_stress_mpa;
    double densification_strain;
    double densification_modulus_mpa;
    double base_transfer_impedance;
    double compression_exponent;
    double cycles_to_failure_10pct;
    double activation_energy_ev;
    double anodic_index_v;
    double max_compression_percent;
} gasket_class_t;


static const gasket_class_t gasket_db[] = {
    { GASKET_CONDUCTIVE_ELASTOMER, "Conductive Elastomer (Ag/Cu)",
      1.5, 0.08, 0.25, 8.0, 0.005, 2.0, 50000.0, 0.5, 0.15, 50.0 },
    { GASKET_WIRE_MESH, "Wire Mesh (Monel/Sn)",
      10.0, 0.5, 0.15, 50.0, 0.002, 1.8, 200000.0, 0.3, -0.1, 25.0 },
    { GASKET_SPRING_FINGER, "Spring Finger (BeCu)",
      50.0, 1.0, 0.05, 200.0, 0.0005, 1.5, 500000.0, 0.2, 0.0, 15.0 },
    { GASKET_CONDUCTIVE_FABRIC, "Conductive Fabric (Ni/Cu)",
      0.8, 0.05, 0.30, 5.0, 0.010, 2.2, 30000.0, 0.4, 0.1, 50.0 },
    { GASKET_FORM_IN_PLACE, "Form-In-Place Silicone",
      0.5, 0.03, 0.35, 3.0, 0.008, 2.5, 20000.0, 0.55, 0.2, 40.0 },
    { GASKET_METAL_SPIRAL, "Metal Spiral (SS/Sn)",
      20.0, 0.8, 0.10, 80.0, 0.001, 1.6, 300000.0, 0.25, -0.15, 20.0 },
    { GASKET_KNITTED_WIRE, "Knitted Wire (Monel)",
      8.0, 0.4, 0.18, 40.0, 0.003, 1.9, 150000.0, 0.3, -0.1, 30.0 },
    { GASKET_ORIENTED_WIRE, "Oriented Wire (Al)",
      30.0, 1.5, 0.08, 100.0, 0.0008, 1.4, 400000.0, 0.2, -0.7, 12.0 },
    { GASKET_CONDUCTIVE_FOAM, "Conductive Foam (PU/Ni-Cu)",
      0.3, 0.02, 0.40, 2.0, 0.015, 2.8, 10000.0, 0.6, 0.3, 60.0 },
    { GASKET_BERYLLIUM_COPPER, "BeCu Fingerstock",
      120.0, 3.0, 0.03, 500.0, 0.0003, 1.2, 1000000.0, 0.15, 0.05, 10.0 },
};
#define GASKET_DB_SIZE (sizeof(gasket_db)/sizeof(gasket_db[0]))

static const gasket_class_t *find_gasket_class(gasket_material_t type) {
    for (size_t i = 0; i < GASKET_DB_SIZE; i++)
        if (gasket_db[i].type == type) return &gasket_db[i];
    return NULL;
}

int gasket_spec_init(gasket_material_t material, gasket_profile_t profile,
                     double width_mm, gasket_spec_t *spec) {
    if (!spec || width_mm <= 0.0) return -1;
    const gasket_class_t *cls = find_gasket_class(material);
    if (!cls) return -1;
    memset(spec, 0, sizeof(*spec));
    spec->material = material;
    spec->profile = profile;
    double aspect_ratio;
    switch (profile) {
        case GASKET_PROFILE_D_SHAPE:   aspect_ratio = 0.6; break;
        case GASKET_PROFILE_RECTANGLE:  aspect_ratio = 1.0; break;
        case GASKET_PROFILE_O_SHAPE:    aspect_ratio = 1.0; break;
        case GASKET_PROFILE_C_FOLD:     aspect_ratio = 0.3; break;
        case GASKET_PROFILE_KNIFE_EDGE: aspect_ratio = 0.2; break;
        case GASKET_PROFILE_FLAT_WASHER:aspect_ratio = 0.15; break;
        case GASKET_PROFILE_P_SHAPE:    aspect_ratio = 0.7; break;
        default: return -1;
    }
    spec->width_mm = width_mm;
    spec->height_uncompressed_mm = width_mm * aspect_ratio;
    spec->height_compressed_mm = spec->height_uncompressed_mm * 0.7;
    spec->compression_set_percent = 10.0;
    spec->closure_force_n_per_m = cls->elastic_modulus_mpa * width_mm * 0.3;
    spec->volume_resistivity_ohm_cm = cls->base_transfer_impedance / width_mm;
    spec->transfer_impedance_ohm_m = cls->base_transfer_impedance;
    spec->temperature_max_c = 125.0;
    spec->temperature_min_c = -40.0;
    spec->ip_rating = 65;
    spec->environmental_class = 1;
    return 0;
}

int gasket_compute_compression(const gasket_spec_t *spec,
    compression_mode_t mode, double value, gasket_compression_t *state) {
    if (!spec || !state) return -1;
    const gasket_class_t *cls = find_gasket_class(spec->material);
    if (!cls) return -1;
    memset(state, 0, sizeof(*state));
    state->spec = *spec;
    state->mode = mode;
    double h0 = spec->height_uncompressed_mm;
    double strain, force;
    if (mode == COMPRESSION_MODE_FIXED_DEFLECTION) {
        double defl = value;
        if (defl < 0.0 || defl > h0 * cls->max_compression_percent / 100.0) return -1;
        strain = defl / h0;
    } else if (mode == COMPRESSION_MODE_FIXED_FORCE) {
        force = value;
        if (force < 0.0) return -1;
        double lo = 0.0, hi = cls->max_compression_percent / 100.0;
        for (int iter = 0; iter < 50; iter++) {
            strain = (lo + hi) / 2.0;
            double sigma;
            if (strain < 0.10) sigma = cls->elastic_modulus_mpa * strain;
            else if (strain < cls->densification_strain) sigma = cls->plateau_stress_mpa;
            else sigma = cls->plateau_stress_mpa + cls->densification_modulus_mpa * (strain - cls->densification_strain);
            double f_calc = sigma * spec->width_mm * 1000.0;
            if (f_calc < force) lo = strain; else hi = strain;
        }
    } else {
        strain = 0.25;
    }
    double sigma_stress;
    if (strain < 0.10) sigma_stress = cls->elastic_modulus_mpa * strain;
    else if (strain < cls->densification_strain) sigma_stress = cls->plateau_stress_mpa;
    else sigma_stress = cls->plateau_stress_mpa + cls->densification_modulus_mpa * (strain - cls->densification_strain);
    state->compression_percent = strain * 100.0;
    state->deflection_mm = strain * h0;
    state->applied_force_n_per_m = sigma_stress * spec->width_mm * 1000.0;
    double contact_area = spec->width_mm * 1e-3 * sqrt(strain / 0.25);
    if (contact_area < 1e-12) contact_area = 1e-12;
    state->contact_resistance_ohm = spec->volume_resistivity_ohm_cm * 1e-2 / contact_area;
    state->effective_attenuation_db = 0.0;
    return 0;
}

double gasket_transfer_impedance(const gasket_compression_t *state, double freq_hz) {
    if (!state || freq_hz <= 0.0) return -1.0;
    if (state->compression_percent < 5.0) return -1.0;
    const gasket_class_t *cls = find_gasket_class(state->spec.material);
    if (!cls) return -1.0;
    double strain = state->compression_percent / 100.0;
    double Z_T0 = cls->base_transfer_impedance;
    double n = cls->compression_exponent;
    double Z_T_dc = Z_T0 * pow(strain, n);
    double f_c = 1.0e6;
    double Z_T = Z_T_dc * sqrt(1.0 + (freq_hz * freq_hz) / (f_c * f_c));
    return Z_T;
}

double gasket_shielding_effectiveness(const gasket_compression_t *state,
    double freq_hz, double seam_length_m) {
    if (!state || freq_hz <= 0.0 || seam_length_m <= 0.0) return -1.0;
    double Z_T = gasket_transfer_impedance(state, freq_hz);
    if (Z_T <= 0.0) return -1.0;
    double Z_eff = Z_T / seam_length_m;
    double R_load = 50.0;
    double SE = 20.0 * log10(R_load / Z_eff);
    if (SE < 0.0) SE = 0.0;
    return SE;
}

double gasket_service_life_estimate(const gasket_spec_t *spec,
    double daily_cycles, double temp_c, int uv_exposure) {
    if (!spec || daily_cycles < 0.0) return -1.0;
    const gasket_class_t *cls = find_gasket_class(spec->material);
    if (!cls) return -1.0;
    double cycles_per_year = daily_cycles * 365.25;
    if (cycles_per_year <= 0.0) cycles_per_year = 1.0;
    double N_f = cls->cycles_to_failure_10pct;
    double years_fatigue = N_f / cycles_per_year;
    double k_B = 8.617333262145e-5;
    double T_ref = 298.15, T_op = temp_c + 273.15;
    if (T_op < 273.15) T_op = 273.15;
    double Ea = cls->activation_energy_ev;
    double thermal_factor = exp((Ea/k_B) * (1.0/T_ref - 1.0/T_op));
    double years_thermal = 20.0 / thermal_factor;
    double uv_factor;
    switch (uv_exposure) {
        case 0: uv_factor = 1.0; break;
        case 1: uv_factor = 0.8; break;
        case 2: uv_factor = 0.5; break;
        default: uv_factor = 1.0;
    }
    double years_limiting = (years_fatigue < years_thermal) ? years_fatigue : years_thermal;
    return years_limiting * uv_factor;
}

int gasket_galvanic_compatibility(const gasket_spec_t *gasket_spec,
                                   shielding_material_id_t flange_material) {
    if (!gasket_spec) return -1;
    const gasket_class_t *gcls = find_gasket_class(gasket_spec->material);
    if (!gcls) return -1;
    double gasket_potential = gcls->anodic_index_v;
    double flange_potential;
    switch (flange_material) {
        case MATERIAL_COPPER:        flange_potential = 0.0; break;
        case MATERIAL_ALUMINUM:      flange_potential = -0.7; break;
        case MATERIAL_STEEL_1008:    flange_potential = -0.35; break;
        case MATERIAL_MU_METAL:      flange_potential = -0.1; break;
        case MATERIAL_NICKEL:        flange_potential = -0.05; break;
        case MATERIAL_TIN_PLATED:    flange_potential = -0.1; break;
        case MATERIAL_STAINLESS_304: flange_potential = -0.05; break;
        case MATERIAL_BRASS:         flange_potential = -0.15; break;
        case MATERIAL_ZINC:          flange_potential = -1.0; break;
        case MATERIAL_SILVER_EPOXY:  flange_potential = 0.15; break;
        case MATERIAL_CONDUCTIVE_PAINT: flange_potential = 0.0; break;
        default:                     flange_potential = 0.0; break;
    }
    double diff = fabs(gasket_potential - flange_potential);
    if (diff < 0.15) return 0;
    if (diff < 0.25) return 1;
    return 2;
}

/* ===================================================================
 * L5: Gasket Multi-Criteria Decision Analysis
 * =================================================================== */

/**
 * @brief Rank gasket types by multi-criteria score.
 *
 * Criteria with weights:
 *   SE performance:    0.30
 *   Cost:              0.20
 *   Closure force:     0.15
 *   Environmental:     0.15
 *   Service life:      0.10
 *   Galvanic compat:   0.10
 *
 * @param flange_material  Enclosure flange material
 * @param freq_hz          Frequency [Hz]
 * @param seam_length_m    Total gasket length [m]
 * @param env_class        Environmental class (1-4)
 * @param rankings         Output: array of material IDs ranked best to worst
 * @param num_ranked       Number of entries filled (max 10)
 * @return 0 on success
 */
int gasket_rank_by_criteria(shielding_material_id_t flange_material,
    double freq_hz, double seam_length_m, int env_class,
    gasket_material_t *rankings, int *num_ranked) {
    if (!rankings || !num_ranked || freq_hz <= 0.0) return -1;

    gasket_material_t all[] = {
        GASKET_CONDUCTIVE_ELASTOMER, GASKET_WIRE_MESH, GASKET_SPRING_FINGER,
        GASKET_CONDUCTIVE_FABRIC, GASKET_FORM_IN_PLACE, GASKET_METAL_SPIRAL,
        GASKET_KNITTED_WIRE, GASKET_ORIENTED_WIRE, GASKET_CONDUCTIVE_FOAM,
        GASKET_BERYLLIUM_COPPER,
    };
    int n = sizeof(all) / sizeof(all[0]);
    double scores[10];

    for (int i = 0; i < n; i++) {
        gasket_spec_t spec;
        if (gasket_spec_init(all[i], GASKET_PROFILE_D_SHAPE, 5.0, &spec) != 0) {
            scores[i] = -1e9;
            continue;
        }
        gasket_compression_t state;
        gasket_compute_compression(&spec, COMPRESSION_MODE_FIXED_DEFLECTION,
            spec.height_uncompressed_mm * 0.25, &state);

        /* SE score: normalize to 0-100 where 100 = meets 80dB */
        double se = gasket_shielding_effectiveness(&state, freq_hz, seam_length_m);
        double se_score = (se > 80.0) ? 100.0 : se / 80.0 * 100.0;

        /* Cost score: estimate from material type (lower cost = higher score) */
        double cost_score;
        switch (all[i]) {
            case GASKET_BERYLLIUM_COPPER:   cost_score = 20.0; break;
            case GASKET_SPRING_FINGER:      cost_score = 30.0; break;
            case GASKET_ORIENTED_WIRE:      cost_score = 40.0; break;
            case GASKET_METAL_SPIRAL:       cost_score = 50.0; break;
            case GASKET_KNITTED_WIRE:       cost_score = 60.0; break;
            case GASKET_WIRE_MESH:          cost_score = 65.0; break;
            case GASKET_CONDUCTIVE_ELASTOMER: cost_score = 70.0; break;
            case GASKET_CONDUCTIVE_FABRIC:  cost_score = 80.0; break;
            case GASKET_FORM_IN_PLACE:      cost_score = 75.0; break;
            case GASKET_CONDUCTIVE_FOAM:    cost_score = 90.0; break;
            default:                        cost_score = 50.0;
        }

        /* Force score: lower closure force = easier assembly = higher score */
        double force_score = 100.0 - (state.applied_force_n_per_m / 1000.0) * 100.0;
        if (force_score < 0.0) force_score = 0.0;

        /* Environmental match */
        double env_score = (spec.environmental_class >= env_class) ? 100.0 :
            (spec.environmental_class == env_class - 1) ? 70.0 : 40.0;

        /* Life score */
        double life = gasket_service_life_estimate(&spec, 1.0, 25.0, env_class - 1);
        double life_score = (life > 20.0) ? 100.0 : life / 20.0 * 100.0;

        /* Galvanic */
        int compat = gasket_galvanic_compatibility(&spec, flange_material);
        double galv_score = (compat == 0) ? 100.0 : (compat == 1) ? 60.0 : 20.0;

        scores[i] = se_score * 0.30 + cost_score * 0.20 + force_score * 0.15
                  + env_score * 0.15 + life_score * 0.10 + galv_score * 0.10;
    }

    /* Sort by score (descending) */
    int indices[10];
    for (int i = 0; i < n; i++) indices[i] = i;
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (scores[indices[j]] > scores[indices[i]]) {
                int tmp = indices[i];
                indices[i] = indices[j];
                indices[j] = tmp;
            }
        }
    }

    *num_ranked = n;
    for (int i = 0; i < n; i++) rankings[i] = all[indices[i]];
    return 0;
}

/**
 * @brief Compute gasket compression force budget for an enclosure.
 *
 * Total closure force = perimeter_length * force_per_unit_length
 * Each screw/fastener must provide this force.
 *
 * @param spec            Gasket specification
 * @param perimeter_m     Total gasket perimeter [m]
 * @param num_fasteners   Number of fasteners along perimeter
 * @param total_force_n   Output: total required force [N]
 * @param force_per_fastener_n Output: force per fastener [N]
 * @return 0 on success
 */
int gasket_closure_force_budget(const gasket_spec_t *spec,
    double perimeter_m, int num_fasteners,
    double *total_force_n, double *force_per_fastener_n) {
    if (!spec || !total_force_n || !force_per_fastener_n ||
        perimeter_m <= 0.0 || num_fasteners < 1) return -1;

    gasket_compression_t state;
    if (gasket_compute_compression(spec, COMPRESSION_MODE_FIXED_DEFLECTION,
        spec->height_uncompressed_mm * 0.25, &state) != 0) return -1;

    *total_force_n = state.applied_force_n_per_m * perimeter_m;
    *force_per_fastener_n = *total_force_n / (double)num_fasteners;
    return 0;
}

/**
 * @brief Check if gasket compression meets IP rating requirement.
 *
 * IPx5: water jet protection (12.5 L/min, 30 kPa at 3m)
 * IPx6: powerful water jet (100 L/min, 100 kPa at 3m)
 * IPx7: immersion 1m depth for 30 min
 * IPx8: continuous immersion (specified by manufacturer)
 *
 * Minimum compression for environmental seal:
 *   IPx4: >= 15% compression
 *   IPx5: >= 20% compression
 *   IPx6: >= 25% compression
 *   IPx7: >= 30% compression
 *
 * @param state     Gasket compression state
 * @param ip_target Target IP rating (e.g., 65 means IP65)
 * @return 1 if meets IP requirement, 0 if not
 */
int gasket_check_ip_rating(const gasket_compression_t *state, int ip_target) {
    if (!state || ip_target < 10) return 0;

    int ip_second_digit = ip_target % 10;
    double min_compression;

    switch (ip_second_digit) {
        case 0: min_compression = 0.0; break;   /* no water protection */
        case 1: min_compression = 5.0; break;   /* dripping water */
        case 2: min_compression = 10.0; break;  /* dripping water tilted */
        case 3: min_compression = 10.0; break;  /* spraying water */
        case 4: min_compression = 15.0; break;  /* splashing water */
        case 5: min_compression = 20.0; break;  /* water jets */
        case 6: min_compression = 25.0; break;  /* powerful water jets */
        case 7: min_compression = 30.0; break;  /* immersion up to 1m */
        case 8: min_compression = 35.0; break;  /* immersion beyond 1m */
        default: min_compression = 20.0;
    }

    return state->compression_percent >= min_compression ? 1 : 0;
}
