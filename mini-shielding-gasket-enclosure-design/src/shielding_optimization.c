/**
 * @file shielding_optimization.c
 * @brief Shielding optimization, cost analysis, and multi-layer design.
 * Ref: Ott Electromagnetic Compatibility Engineering, MIL-STD-285.
 */
#include "shielding_core.h"
#include "shielding_material.h"
#include <math.h>
#include <stdlib.h>

/* L5: Multi-layer shield effectiveness */
double multilayer_shielding_effectiveness_db(const double *se_layers, int n)
{
    if (!se_layers || n <= 0) return 0.0;
    double total = se_layers[0];
    for (int i = 1; i < n; i++)
        total += se_layers[i] + 6.0; /* 6dB bonus per additional layer (reflection at interfaces) */
    return total;
}

/* L6: Aperture leakage from slot/hole array */
double aperture_leakage_db(double wavelength, double max_dimension, int n_apertures,
                            double area_total)
{
    if (wavelength <= 0.0 || max_dimension <= 0.0 || area_total <= 0.0) return 0.0;
    double se_single = 20.0 * log10(wavelength / (2.0 * max_dimension));
    if (n_apertures <= 1) return se_single > 0.0 ? se_single : 0.0;
    double correction = 10.0 * log10((double)n_apertures);
    double result = se_single - correction;
    return result > 0.0 ? result : 0.0;
}

/* L6: Gasket transfer impedance to SE */
double gasket_se_from_transfer_impedance(double Zt, double gasket_length_m, double Z0)
{
    if (Zt <= 0.0 || gasket_length_m <= 0.0 || Z0 <= 0.0) return 0.0;
    double Z_contact = Zt / gasket_length_m;
    return 20.0 * log10(Z0 / (2.0 * Z_contact));
}

/* L7: Cost-optimized shielding material selection */
typedef struct { const char *name; double conductivity; double permeability; double cost_per_m2; } ShieldOption;

int shield_cost_optimize(const ShieldOption *options, int n, double target_se_db,
                          double freq, double thickness, int *best_idx, double *best_cost)
{
    if (!options || n <= 0 || !best_idx || !best_cost) return -1;
    *best_idx = -1; *best_cost = 1e12;
    for (int i = 0; i < n; i++) {
        double se = shielding_effectiveness_plane_wave(options[i].conductivity,
                         options[i].permeability, freq, thickness);
        if (se >= target_se_db && options[i].cost_per_m2 < *best_cost) {
            *best_cost = options[i].cost_per_m2;
            *best_idx = i;
        }
    }
    return (*best_idx >= 0) ? 0 : -1;
}

/* L7: Enclosure resonance modes */
int enclosure_resonance_frequencies(double a, double b, double c,
                                     double *freqs, int max_modes, int *n_found)
{
    if (a <= 0.0 || b <= 0.0 || c <= 0.0 || !freqs || max_modes <= 0 || !n_found)
        return -1;
    double c0 = 299792458.0; int count = 0;
    for (int m = 0; m <= 3 && count < max_modes; m++)
        for (int n = 0; n <= 3 && count < max_modes; n++)
            for (int p = 0; p <= 3 && count < max_modes; p++) {
                if (m == 0 && n == 0 && p == 0) continue;
                double f = c0 * 0.5 * sqrt((m/a)*(m/a) + (n/b)*(n/b) + (p/c)*(p/c));
                freqs[count++] = f;
            }
    *n_found = count;
    return 0;
}

/* L8: Near-field to far-field shielding transition */
double shield_near_field_correction(double se_plane_wave, double distance_m,
                                     double wavelength, int is_electric_field)
{
    if (wavelength <= 0.0 || distance_m <= 0.0) return se_plane_wave;
    double beta = 2.0 * M_PI / wavelength;
    double kr = beta * distance_m;
    double correction;
    if (is_electric_field)
        correction = 20.0 * log10(1.0 + 1.0 / (kr * kr));
    else
        correction = 20.0 * log10(1.0 + kr * kr);
    return se_plane_wave + correction;
}