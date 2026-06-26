/* shielding_core.c -- Schelkunoff Shielding Theory Implementation
 *
 * L4: Fundamental Laws - Schelkunoff plane-wave shielding equations (1943)
 * L5: Algorithms - Multi-layer SE optimization
 *
 * Reference: S.A. Schelkunoff, Electromagnetic Waves, Van Nostrand, 1943
 *            Paul (2006) Introduction to EMC Ch.7.5-7.6
 *            Ott (2009) Electromagnetic Compatibility Engineering Ch.6
 *            Kaden, H. Wirbelstrome und Schirmung, Springer, 1959
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shielding_gasket.h"


double shielding_absorption_loss(const shield_layer_t *layer, double freq_hz) {
    if (!layer || freq_hz <= 0.0 || layer->thickness_m <= 0.0) return -1.0;
    double delta = shielding_skin_depth(&layer->material, freq_hz);
    if (delta <= 0.0) return -1.0;
    return 8.686 * layer->thickness_m / delta;
}

double shielding_reflection_loss(const shield_layer_t *layer, double freq_hz,
                                  se_field_type_t field_type, double dist_from_source_m) {
    if (!layer || freq_hz <= 0.0) return -1.0;
    double sigma = layer->material.conductivity;
    double mu_r = layer->material.relative_permeability;
    if (sigma <= 0.0) return -1.0;
    double sigma_r = sigma / SIGMA_COPPER;
    double R;
    switch (field_type) {
        case SE_FIELD_PLANE_WAVE:
            R = 168.0 - 10.0 * log10(mu_r * freq_hz / sigma_r);
            break;
        case SE_FIELD_NEAR_E:
            if (dist_from_source_m <= 0.0) return -1.0;
            R = 322.0 - 10.0 * log10(mu_r * freq_hz * freq_hz * freq_hz
                  * dist_from_source_m * dist_from_source_m / sigma_r);
            break;
        case SE_FIELD_NEAR_H:
            if (dist_from_source_m <= 0.0) return -1.0;
            R = 14.6 + 10.0 * log10(mu_r * freq_hz
                  * dist_from_source_m * dist_from_source_m / sigma_r);
            break;
        default:
            R = 168.0 - 10.0 * log10(mu_r * freq_hz / sigma_r);
            break;
    }
    if (R < 0.0) R = 0.0;
    return R;
}

double shielding_multiple_reflection_correction(const shield_layer_t *layer, double freq_hz) {
    if (!layer || freq_hz <= 0.0 || layer->thickness_m <= 0.0) return -1.0;
    double delta = shielding_skin_depth(&layer->material, freq_hz);
    if (delta <= 0.0) return -1.0;
    double factor = exp(-2.0 * layer->thickness_m / delta);
    double term = 1.0 - factor;
    if (term <= 0.0) term = 1e-15;
    double M = 20.0 * log10(term);
    if (M > 0.0) M = 0.0;
    return M;
}

se_result_t shielding_effectiveness_single_layer(const shield_layer_t *layer,
    double freq_hz, se_field_type_t field_type, double dist_from_source_m) {
    se_result_t result;
    memset(&result, 0, sizeof(result));
    result.frequency_hz = freq_hz;
    result.field_type = field_type;
    if (!layer || freq_hz <= 0.0) { result.total_se_db = 0.0; return result; }
    double A = shielding_absorption_loss(layer, freq_hz);
    double R = shielding_reflection_loss(layer, freq_hz, field_type, dist_from_source_m);
    double M = shielding_multiple_reflection_correction(layer, freq_hz);
    if (A < 0.0) A = 0.0;
    if (R < 0.0) R = 0.0;
    if (M > 0.0) M = 0.0;
    result.absorption_loss_db = A;
    result.reflection_loss_db = R;
    result.multiple_reflection_correction_db = M;
    result.total_se_db = A + R + M;
    return result;
}

double shielding_effectiveness_multilayer(const shield_stack_t *stack,
    double freq_hz, se_field_type_t field_type, double dist_from_source_m) {
    if (!stack || stack->num_layers < 1 || stack->num_layers > MAX_SHIELD_LAYERS)
        return -1.0;
    if (freq_hz <= 0.0) return -1.0;
    if (stack->num_layers == 1) {
        se_result_t r = shielding_effectiveness_single_layer(
            &stack->layers[0], freq_hz, field_type, dist_from_source_m);
        return r.total_se_db;
    }
    double A_total = 0.0, R_total = 0.0, M_total = 0.0;
    for (int i = 0; i < stack->num_layers; i++) {
        const shield_layer_t *layer = &stack->layers[i];
        double Ai = shielding_absorption_loss(layer, freq_hz);
        if (Ai > 0.0) A_total += Ai;
        if (i == 0) {
            double Ri = shielding_reflection_loss(layer, freq_hz, field_type, dist_from_source_m);
            if (Ri > 0.0) R_total += Ri;
        }
        double Mi = shielding_multiple_reflection_correction(layer, freq_hz);
        if (Mi < 0.0) M_total += Mi;
    }
    double gap_correction = 0.0;
    for (int i = 0; i < stack->num_layers - 1; i++) {
        double gap = stack->air_gap_m[i];
        if (gap > 0.0) {
            double wavelength = C0 / freq_hz;
            double phase_shift = 2.0 * M_PI * gap / wavelength;
            double resonance_factor = fabs(cos(phase_shift));
            gap_correction -= 6.0 * (1.0 - resonance_factor);
        }
    }
    double SE = A_total + R_total + M_total + gap_correction;
    if (SE < 0.0) SE = 0.0;
    return SE;
}

/* ===================================================================
 * L5: Frequency Sweep Analysis
 *
 * Compute SE across a logarithmic frequency sweep to characterize
 * shield performance from near-DC to microwave frequencies.
 * =================================================================== */

/**
 * @brief Perform SE frequency sweep and fill result array.
 *
 * Sweeps from f_start to f_end with N points per decade.
 * Returns an array of se_result_t for each frequency point.
 *
 * @param layer       Shield layer
 * @param f_start_hz  Start frequency [Hz]
 * @param f_end_hz    End frequency [Hz]
 * @param points_per_decade  Resolution (typ. 10, 20, or 50)
 * @param field_type  Incident field type
 * @param dist_m      Source distance [m]
 * @param results     Output: array of results (caller must free)
 * @param num_points  Output: number of points filled
 * @return 0 on success, -1 on error
 */
int shielding_frequency_sweep(const shield_layer_t *layer,
    double f_start_hz, double f_end_hz, int points_per_decade,
    se_field_type_t field_type, double dist_m,
    se_result_t **results, int *num_points) {
    if (!layer || !results || !num_points || f_start_hz <= 0.0 ||
        f_end_hz <= f_start_hz || points_per_decade < 1)
        return -1;

    int decades = (int)ceil(log10(f_end_hz / f_start_hz));
    int n = decades * points_per_decade + 1;
    if (n < 2) n = 2;
    if (n > 10000) n = 10000;

    se_result_t *arr = (se_result_t *)malloc((size_t)n * sizeof(se_result_t));
    if (!arr) return -1;

    double log_start = log10(f_start_hz);
    double log_end = log10(f_end_hz);
    double step = (log_end - log_start) / (n - 1);

    for (int i = 0; i < n; i++) {
        double f = pow(10.0, log_start + i * step);
        arr[i] = shielding_effectiveness_single_layer(layer, f, field_type, dist_m);
    }

    *results = arr;
    *num_points = n;
    return 0;
}

/**
 * @brief Find the frequency where SE drops below a threshold.
 * Binary search on the shield's SE curve.
 *
 * @param layer        Shield layer
 * @param f_min_hz     Search range minimum [Hz]
 * @param f_max_hz     Search range maximum [Hz]
 * @param threshold_db SE threshold [dB]
 * @param field_type   Field type
 * @param dist_m       Source distance [m]
 * @param result_hz    Output: crossover frequency [Hz]
 * @return 0 if found, -1 if never crosses threshold in range
 */
int shielding_find_crossover_frequency(const shield_layer_t *layer,
    double f_min_hz, double f_max_hz, double threshold_db,
    se_field_type_t field_type, double dist_m, double *result_hz) {
    if (!layer || !result_hz || f_min_hz <= 0.0 || f_max_hz <= f_min_hz)
        return -1;

    /* Check if crossing exists */
    se_result_t r_low = shielding_effectiveness_single_layer(
        layer, f_min_hz, field_type, dist_m);
    se_result_t r_high = shielding_effectiveness_single_layer(
        layer, f_max_hz, field_type, dist_m);

    /* SE typically decreases with frequency (reflection dominates at low f,
     * absorption at high f - net effect is generally decreasing) */
    double se_low = r_low.total_se_db;
    double se_high = r_high.total_se_db;

    if ((se_low - threshold_db) * (se_high - threshold_db) > 0)
        return -1; /* no crossing */

    /* Binary search for crossover */
    double lo = f_min_hz, hi = f_max_hz;
    for (int iter = 0; iter < 60; iter++) {
        double mid = (lo + hi) / 2.0;
        se_result_t r_mid = shielding_effectiveness_single_layer(
            layer, mid, field_type, dist_m);
        if (r_mid.total_se_db > threshold_db)
            lo = mid;
        else
            hi = mid;
    }
    *result_hz = (lo + hi) / 2.0;
    return 0;
}

/**
 * @brief Compute minimum thickness required for target SE.
 *
 * Using Schelkunoff theory, solve SE_target = A(t) + R + M(t)
 * for the minimum thickness t.
 *
 * Since M depends on t/delta, we use an iterative approach:
 * 1. Estimate t from absorption alone
 * 2. Refine with full calculation
 *
 * @param material      Shield material
 * @param freq_hz       Frequency [Hz]
 * @param target_se_db  Desired SE [dB]
 * @param field_type    Field type
 * @param dist_m        Source distance [m]
 * @return Minimum thickness [m], -1 on error
 */
double shielding_minimum_thickness(const shielding_material_t *material,
    double freq_hz, double target_se_db, se_field_type_t field_type,
    double dist_m) {
    if (!material || freq_hz <= 0.0 || target_se_db <= 0.0) return -1.0;

    double delta = shielding_skin_depth(material, freq_hz);
    if (delta <= 0.0) return -1.0;

    /* First estimate: SE ~ A = 8.686*t/delta + R */
    shield_layer_t tmp = {*material, 0.001, dist_m};
    double R = shielding_reflection_loss(&tmp, freq_hz, field_type, dist_m);
    if (R < 0.0) R = 0.0;

    double A_needed = target_se_db - R;
    if (A_needed <= 0.0) return 0.0001; /* reflection alone sufficient */

    double t_initial = A_needed * delta / 8.686;

    /* Refine with full calculation including multiple reflection */
    double t = t_initial;
    for (int iter = 0; iter < 20; iter++) {
        tmp.thickness_m = t;
        se_result_t r = shielding_effectiveness_single_layer(
            &tmp, freq_hz, field_type, dist_m);

        double error = target_se_db - r.total_se_db;
        if (fabs(error) < 0.01) break;

        /* Adjust thickness: SE changes ~8.686/delta per meter */
        double gradient = 8.686 / delta;
        double dt = error / gradient;
        t += dt;
        if (t < 0.00001) t = 0.00001;
        if (t > 0.1) t = 0.1; /* practical limit */
    }
    return t;
}

/* ===================================================================
 * L2: Field region determination utilities
 * =================================================================== */

double shielding_near_field_boundary(double freq_hz) {
    if (freq_hz <= 0.0) return -1.0;
    return C0 / (2.0 * M_PI * freq_hz);
}

se_field_type_t shielding_determine_field_region(double distance_m, double freq_hz) {
    if (distance_m <= 0.0 || freq_hz <= 0.0) return SE_FIELD_PLANE_WAVE;
    double boundary = shielding_near_field_boundary(freq_hz);
    if (distance_m > boundary) return SE_FIELD_PLANE_WAVE;
    /* Conservative: assume near H if source impedance unknown
     * (H-field is harder to shield, so this is the worst case) */
    return SE_FIELD_NEAR_H;
}

void shielding_print_se_report(const se_result_t *result) {
    if (!result) { printf("SE Report: NULL result\n"); return; }
    printf("=== Shielding Effectiveness Report ===\n");
    printf("  Frequency:           %.3e Hz\n", result->frequency_hz);
    printf("  Field Type:          %d\n", result->field_type);
    printf("  Absorption Loss:     %.2f dB\n", result->absorption_loss_db);
    printf("  Reflection Loss:     %.2f dB\n", result->reflection_loss_db);
    printf("  Multiple Refl Corr:  %.2f dB\n", result->multiple_reflection_correction_db);
    printf("  Total SE:            %.2f dB\n", result->total_se_db);
    printf("  Aperture Leakage:    %.2f dB\n", result->aperture_leakage_db);
    printf("  Seam Leakage:        %.2f dB\n", result->seam_leakage_db);
    printf("  Net SE:              %.2f dB\n", result->net_se_db);
}

void shielding_print_cavity_report(const cavity_analysis_t *analysis) {
    if (!analysis) { printf("Cavity Report: NULL analysis\n"); return; }
    printf("=== Cavity Resonance Analysis ===\n");
    printf("  Modes found:         %d\n", analysis->num_modes_found);
    if (analysis->num_modes_found > 0) {
        printf("  First resonance:     %.2f MHz\n",
            analysis->first_resonance_hz / 1.0e6);
        printf("  Mode listing (first 5):\n");
        int n = analysis->num_modes_found < 5 ? analysis->num_modes_found : 5;
        for (int i = 0; i < n; i++) {
            printf("    TE/TM_%d%d%d: %.2f MHz (Q=%.1f)\n",
                analysis->modes[i].mode_index_m,
                analysis->modes[i].mode_index_n,
                analysis->modes[i].mode_index_p,
                analysis->modes[i].resonant_frequency_hz / 1.0e6,
                analysis->modes[i].quality_factor);
        }
    }
}

int shielding_validate_enclosure(const enclosure_spec_t *enclosure) {
    if (!enclosure) return 1;
    if (enclosure->dimension_x_m <= 0.0 ||
        enclosure->dimension_y_m <= 0.0 ||
        enclosure->dimension_z_m <= 0.0) return 2;
    if (enclosure->shield.num_layers < 1 ||
        enclosure->shield.num_layers > MAX_SHIELD_LAYERS) return 3;
    for (int i = 0; i < enclosure->shield.num_layers; i++) {
        if (enclosure->shield.layers[i].thickness_m <= 0.0) return 4;
        if (enclosure->shield.layers[i].material.conductivity <= 0.0) return 5;
    }
    if (enclosure->num_apertures > 0 && !enclosure->apertures) return 6;
    return 0;
}
