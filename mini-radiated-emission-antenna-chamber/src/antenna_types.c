/* =========================================================================
 * antenna_types.c - Antenna Parameter Computations
 *
 * L2: Antenna type characteristics
 * L3: Gain computations from geometry (biconical, LPDA, horn)
 * L5: Antenna factor estimation from physical parameters
 *
 * References:
 *   Balanis (2016) Antenna Theory, sections 4.4, 11.3, 13.4
 *   CISPR 16-1-4 Annex C
 * ========================================================================= */

#include "antenna_types.h"
#include "field_propagation.h"
#include <math.h>
#include <stdio.h>

#define SPEED_OF_LIGHT 299792458.0
#define PI 3.14159265358979323846

/* Wavelength from frequency. lambda = c / f */
double at_wavelength(double frequency_hz)
{
    if (frequency_hz <= 0.0) return 0.0;
    return SPEED_OF_LIGHT / frequency_hz;
}

/* Free-space path loss (Friis equation dB form).
 * FSPL = 20*log10(4*pi*d / lambda)
 * Reference: Friis (1946) */
double at_free_space_path_loss(double distance_m, double freq_hz)
{
    if (distance_m <= 0.0 || freq_hz <= 0.0) return 0.0;
    double lambda = at_wavelength(freq_hz);
    if (lambda <= 0.0) return 0.0;
    return 20.0 * log10(4.0 * PI * distance_m / lambda);
}

/* Biconical antenna gain estimate.
 * Ideal infinite biconical: G approx 1.5 (1.76 dBi) for half-angle > 30 deg
 * Finite biconical: G varies with cone length relative to wavelength
 * Reference: Balanis (2016) section 4.4 */
double at_biconical_gain(double half_angle_deg, double freq_hz,
                          double cone_length_m)
{
    if (half_angle_deg <= 0.0 || freq_hz <= 0.0 || cone_length_m <= 0.0)
        return 1.0;

    double d0;
    if (half_angle_deg < 20.0)      d0 = 2.0;
    else if (half_angle_deg < 40.0) d0 = 1.6;
    else if (half_angle_deg < 60.0) d0 = 1.4;
    else                            d0 = 1.2;

    double lambda = at_wavelength(freq_hz);
    double length_in_lambda = cone_length_m / lambda;
    double efficiency;
    if (length_in_lambda < 0.1) {
        efficiency = length_in_lambda / 0.25;
        if (efficiency > 1.0) efficiency = 1.0;
    } else {
        efficiency = 1.0;
    }
    return d0 * efficiency;
}

/* LPDA gain estimate.
 * Carrel (1961): G approx 7.08*tau + 4.25*sigma
 * Reference: Balanis (2016) section 11.3.3 */
double at_lpda_gain(double tau, double sigma, size_t n_dipoles)
{
    if (tau <= 0.0 || sigma <= 0.0 || n_dipoles < 3) return 1.0;
    if (tau > 0.98) tau = 0.98;
    if (tau < 0.7) tau = 0.7;
    if (sigma > 0.3) sigma = 0.3;

    double d_db;
    if (tau >= 0.9)
        d_db = 7.0 + 8.0 * (tau - 0.9) + 10.0 * (sigma - 0.1);
    else if (tau >= 0.8)
        d_db = 5.5 + 12.0 * (tau - 0.8) + 10.0 * (sigma - 0.1);
    else
        d_db = 4.0 + 10.0 * (tau - 0.7) + 8.0 * (sigma - 0.05);

    if (n_dipoles < 8)      d_db -= 1.5;
    else if (n_dipoles > 15) d_db += 0.5;
    if (d_db < 3.0) d_db = 3.0;
    if (d_db > 9.0) d_db = 9.0;

    return pow(10.0, d_db / 10.0);
}

/* Horn antenna gain estimate.
 * G = (4*pi/lambda^2) * A_e, A_e = e_ap * A_p, e_ap approx 0.45
 * Reference: Balanis (2016) section 13.4 */
double at_horn_gain(double aperture_width_m, double aperture_height_m,
                     double freq_hz)
{
    if (aperture_width_m <= 0.0 || aperture_height_m <= 0.0 || freq_hz <= 0.0)
        return 1.0;

    double lambda = at_wavelength(freq_hz);
    double aperture_area = aperture_width_m * aperture_height_m;
    double e_ap = 0.45;
    double gain_linear = (4.0 * PI / (lambda * lambda)) * aperture_area * e_ap;
    if (gain_linear < 1.0) gain_linear = 1.0;
    return gain_linear;
}

/* Horn antenna beamwidth.
 * E-plane HPBW approx 56/(A_h/lambda) deg
 * H-plane HPBW approx 67/(A_w/lambda) deg
 * Reference: Balanis (2016) section 13.5 */
void at_horn_beamwidth(double aperture_width_m, double aperture_height_m,
                        double freq_hz, double *hp_e_deg, double *hp_h_deg)
{
    if (!hp_e_deg || !hp_h_deg) return;

    if (aperture_width_m <= 0.0 || aperture_height_m <= 0.0 || freq_hz <= 0.0) {
        *hp_e_deg = 90.0;
        *hp_h_deg = 90.0;
        return;
    }

    double lambda = at_wavelength(freq_hz);

    double a_h_lambda = aperture_height_m / lambda;
    double a_w_lambda = aperture_width_m / lambda;

    *hp_e_deg = (a_h_lambda > 0.1) ? 56.0 / a_h_lambda : 90.0;
    *hp_h_deg = (a_w_lambda > 0.1) ? 67.0 / a_w_lambda : 90.0;

    if (*hp_e_deg < 1.0) *hp_e_deg = 1.0;
    if (*hp_h_deg < 1.0) *hp_h_deg = 1.0;
    if (*hp_e_deg > 180.0) *hp_e_deg = 180.0;
    if (*hp_h_deg > 180.0) *hp_h_deg = 180.0;
}

/* Estimate antenna factor from geometry.
 * AF_est = 20*log10(f_MHz) - G_est_dBi - 29.78 + K_correction */
double at_estimate_antenna_factor(antenna_type_t type, double freq_hz,
                                   const antenna_geometry_t *geom)
{
    if (freq_hz <= 0.0 || !geom) return -999.0;

    double f_mhz = freq_hz / 1e6;
    double gain_linear = 1.0;
    double k_factor = 0.0;

    switch (type) {
    case AT_BICONICAL:
        gain_linear = at_biconical_gain(geom->half_angle_deg, freq_hz,
                                         geom->max_dimension_m / 2.0);
        k_factor = 1.5;
        break;
    case AT_LOG_PERIODIC:
        gain_linear = at_lpda_gain(0.87, 0.15, 15);
        k_factor = 1.0;
        break;
    case AT_DOUBLE_RIDGED_HORN:
    case AT_STANDARD_GAIN_HORN:
        gain_linear = at_horn_gain(geom->max_dimension_m * 0.7,
                                    geom->max_dimension_m * 0.5, freq_hz);
        k_factor = 0.5;
        break;
    case AT_TUNED_DIPOLE:
        gain_linear = 1.64;
        k_factor = 0.0;
        break;
    case AT_BROADBAND_DIPOLE:
        gain_linear = 2.0;
        k_factor = 1.0;
        break;
    case AT_LOOP_MAGNETIC:
        gain_linear = 0.01;
        k_factor = 3.0;
        break;
    default:
        gain_linear = 1.0;
        k_factor = 2.0;
        break;
    }

    if (gain_linear <= 0.0) gain_linear = 0.001;
    return 20.0 * log10(f_mhz) - 10.0 * log10(gain_linear) - 29.78 + k_factor;
}