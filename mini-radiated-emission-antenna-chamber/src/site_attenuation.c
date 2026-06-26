/* =========================================================================
 * site_attenuation.c — Normalized Site Attenuation (NSA) Theory and Measurement
 *
 * L1: NSA definition, measurement procedures
 * L3: Two-ray ground reflection model
 * L4: Image theory, Friis equation, reciprocity
 * L5: NSA measurement algorithm, height scan optimization
 * L6: OATS vs SAC comparison, 3m vs 10m NSA
 *
 * References:
 *   ANSI C63.4:2014 Annex A (Site Attenuation)
 *   CISPR 16-1-4:2019 section 6 (Test Site Validation)
 *   Smith, German, Pate (1982) "Calculation of Site Attenuation
 *     from Antenna Factors"
 * ========================================================================= */

#include "radiated_emission.h"
#include "antenna_types.h"
#include "chamber_theory.h"
#include "field_propagation.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define SPEED_OF_LIGHT 299792458.0
#define PI 3.14159265358979323846

/* -------------------------------------------------------------------------
 * L3: Compute free-space wavelength.
 * ------------------------------------------------------------------------- */
static double wavelength(double freq_hz)
{
    return SPEED_OF_LIGHT / freq_hz;
}

/* -------------------------------------------------------------------------
 * L4: Compute theoretical NSA for free-space conditions.
 *
 * In free space (no ground plane), the site attenuation between
 * two antennas is given by the Friis transmission equation:
 *
 *   V_rx / V_tx = G_tx * G_rx * (lambda / (4*pi*d))^2
 *
 * NSA_free = 20*log10(4*pi*d / lambda) - G_tx_dBi - G_rx_dBi   [dB]
 *
 * Reference: CISPR 16-1-4 Annex D eqn D.1
 * ------------------------------------------------------------------------- */
static double nsa_free_space(double distance_m, double freq_hz,
                              double g_tx_dbi, double g_rx_dbi)
{
    double lambda = wavelength(freq_hz);
    double path_loss = 20.0 * log10(4.0 * PI * distance_m / lambda);

    return path_loss - g_tx_dbi - g_rx_dbi;
}

/* -------------------------------------------------------------------------
 * L3+L4: Compute theoretical NSA over a perfect ground plane.
 *
 * Two-ray model (direct + ground-reflected paths):
 *
 *   r_direct  = sqrt(d^2 + (h_tx - h_rx)^2)
 *   r_reflected = sqrt(d^2 + (h_tx + h_rx)^2)
 *
 * Total field at receive antenna:
 *   E_total = E_direct * (1 + Gamma * exp(-j*k*delta_r))
 *
 * where delta_r = r_reflected - r_direct (path difference)
 *       k = 2*pi/lambda (wavenumber)
 *       Gamma = reflection coefficient
 *
 * For horizontal polarization over perfect ground: Gamma = -1
 *   V_rx^2 ~ sin^2(k * h_tx * h_rx / d)  (for d >> h_tx, h_rx)
 *
 * NSA_OATS = -20*log10(|V_site / V_direct|)
 *
 * Reference: ANSI C63.4 Annex A eqn A.2, CISPR 16-1-4 Annex D eqn D.2
 * ------------------------------------------------------------------------- */
double ct_nsa_theoretical(double distance_m, double h_tx_m, double h_rx_m,
                           double freq_hz, re_polarization_t polarization)
{
    if (distance_m <= 0.0 || freq_hz <= 0.0) {
        return -999.0;
    }

    double lambda = wavelength(freq_hz);
    double k = 2.0 * PI / lambda;

    /* Direct path length */
    double dh = h_tx_m - h_rx_m;
    double r_direct = sqrt(distance_m * distance_m + dh * dh);

    /* Reflected path length (via ground plane) */
    double sh = h_tx_m + h_rx_m;
    double r_reflected = sqrt(distance_m * distance_m + sh * sh);

    /* Path length difference */
    double delta_r = r_reflected - r_direct;

    /* Phase difference due to path length */
    double phase_diff = k * delta_r;

    /* Reflection coefficient for perfect ground plane */
    double gamma;
    if (polarization == RE_POL_HORIZONTAL) {
        gamma = -1.0;  /* Perfect conductor, E tangential = 0 at surface */
    } else {
        /* Vertical: grazing incidence, Gamma -> +1 for perfect conductor */
        double grazing_angle = atan((h_tx_m + h_rx_m) / distance_m);
        if (grazing_angle < 0.01) {
            gamma = 1.0;  /* Near-grazing, approximate as +1 */
        } else {
            /* For perfect conductor: Gamma_v = +1 for all angles */
            gamma = 1.0;
        }
    }

    /* Total field factor: |1 + Gamma * exp(-j*phi)| */
    double cos_phi = cos(phase_diff);
    double sin_phi = sin(phase_diff);

    /* gamma is real for perfect ground */
    double mag_squared = 1.0 + gamma * gamma
                         + 2.0 * gamma * cos_phi;

    double field_factor = sqrt(mag_squared);

    if (field_factor < 1e-10) {
        return 80.0; /* Deep null in pattern */
    }

    /* NSA = -20*log10(field_factor * r_direct / lambda contribution) */
    /* The site attenuation relative to free-space considers the
     * two-ray interference pattern.
     *
     * ANSI C63.4 eqn A.4:
     * NSA = -20*log10( |V_site/V_direct| )
     *
     * For OATS measurement, NSA_theoretical [dB] is computed from
     * antenna heights, distance, frequency.
     *
     * Simplified form for d >> h:
     *   NSA ≈ 20*log10(d^2 / (h_tx * h_rx)) - G_tx - G_rx + 10*log10(...)
     */
    double nsa_db = -20.0 * log10(field_factor)
                    + 20.0 * log10(r_direct)
                    + 20.0 * log10(4.0 * PI / lambda);

    return nsa_db;
}

/* -------------------------------------------------------------------------
 * L5: NSA height scan optimization.
 *
 * For OATS/SAC measurements, the receive antenna height must be
 * scanned (typically 1-4 m) to find the maximum received signal.
 *
 * The two-ray model predicts maxima at heights where:
 *   2 * k * h_tx * h_rx / d = n*pi   (constructive interference)
 *   h_rx,n = n * lambda * d / (4 * h_tx)
 *
 * Minima at:
 *   2 * k * h_tx * h_rx / d = (2n+1)*pi/2  (destructive)
 *
 * This function performs a golden section search over 1-4m range.
 *
 * Reference: ANSI C63.4 Annex A, Fig A.1
 * ------------------------------------------------------------------------- */
void ct_nsa_height_scan_opt(double distance_m, double h_tx_m,
                             double freq_hz, re_polarization_t polarization,
                             double *h_rx_opt_m, double *max_v_ratio)
{
    if (!h_rx_opt_m || !max_v_ratio) return;

    /* Validate parameters */
    if (distance_m <= 0.0 || h_tx_m <= 0.0 || freq_hz <= 0.0) {
        *h_rx_opt_m = 1.0;
        *max_v_ratio = 1.0;
        return;
    }

    double lambda = wavelength(freq_hz);
    double k = 2.0 * PI / lambda;

    /* Golden section search parameters */
    const double phi = 1.618033988749895; /* Golden ratio */
    const double resphi = 2.0 - phi;       /* 1/phi^2 */

    /* Search range: CISPR 16-1-4 specifies 1-4m for OATS/SAC */
    double h_min = 1.0;
    double h_max = 4.0;

    /* If d=3m, max height may be limited by geometry */
    if (distance_m < 4.0) h_max = distance_m * 1.2;
    if (h_max > 4.0) h_max = 4.0;

    double a = h_min;
    double b = h_max;

    /* Golden section search for maximum signal */
    double x1 = a + resphi * (b - a);
    double x2 = b - resphi * (b - a);

    /* Compute signal ratio at x1 and x2 */
    double dh1 = h_tx_m - x1;
    double sh1 = h_tx_m + x1;
    double r1_dir  = sqrt(distance_m * distance_m + dh1 * dh1);
    double r1_ref  = sqrt(distance_m * distance_m + sh1 * sh1);
    double gamma = (polarization == RE_POL_HORIZONTAL) ? -1.0 : 1.0;
    double cos_phi1 = cos(k * (r1_ref - r1_dir));
    double mag1_sq  = 1.0 + gamma * gamma + 2.0 * gamma * cos_phi1;
    double f1 = sqrt(mag1_sq) / r1_dir;

    double dh2 = h_tx_m - x2;
    double sh2 = h_tx_m + x2;
    double r2_dir  = sqrt(distance_m * distance_m + dh2 * dh2);
    double r2_ref  = sqrt(distance_m * distance_m + sh2 * sh2);
    double cos_phi2 = cos(k * (r2_ref - r2_dir));
    double mag2_sq  = 1.0 + gamma * gamma + 2.0 * gamma * cos_phi2;
    double f2 = sqrt(mag2_sq) / r2_dir;

    /* Iterate */
    for (int iter = 0; iter < 30; iter++) {
        if (f1 > f2) {
            b = x2;
            x2 = x1;
            f2 = f1;
            x1 = a + resphi * (b - a);

            dh1 = h_tx_m - x1;
            sh1 = h_tx_m + x1;
            r1_dir  = sqrt(distance_m * distance_m + dh1 * dh1);
            r1_ref  = sqrt(distance_m * distance_m + sh1 * sh1);
            cos_phi1 = cos(k * (r1_ref - r1_dir));
            mag1_sq  = 1.0 + gamma * gamma + 2.0 * gamma * cos_phi1;
            f1 = sqrt(mag1_sq) / r1_dir;
        } else {
            a = x1;
            x1 = x2;
            f1 = f2;
            x2 = b - resphi * (b - a);

            dh2 = h_tx_m - x2;
            sh2 = h_tx_m + x2;
            r2_dir  = sqrt(distance_m * distance_m + dh2 * dh2);
            r2_ref  = sqrt(distance_m * distance_m + sh2 * sh2);
            cos_phi2 = cos(k * (r2_ref - r2_dir));
            mag2_sq  = 1.0 + gamma * gamma + 2.0 * gamma * cos_phi2;
            f2 = sqrt(mag2_sq) / r2_dir;
        }

        if (b - a < 0.001) break; /* 1 mm resolution */
    }

    /* Optimal height is at midpoint of final interval */
    *h_rx_opt_m = (a + b) / 2.0;

    /* Maximum signal ratio (normalized to free-space) */
    dh1 = h_tx_m - *h_rx_opt_m;
    sh1 = h_tx_m + *h_rx_opt_m;
    r1_dir  = sqrt(distance_m * distance_m + dh1 * dh1);
    r1_ref  = sqrt(distance_m * distance_m + sh1 * sh1);
    cos_phi1 = cos(k * (r1_ref - r1_dir));
    mag1_sq  = 1.0 + gamma * gamma + 2.0 * gamma * cos_phi1;
    *max_v_ratio = sqrt(mag1_sq);
}

/* -------------------------------------------------------------------------
 * L1: Compute ground plane reflection coefficient using Fresnel equations.
 *
 * Horizontal polarization:
 *   Gamma_h = (sin(psi) - sqrt(epsilon_c - cos^2(psi))) /
 *             (sin(psi) + sqrt(epsilon_c - cos^2(psi)))
 *
 * Vertical polarization:
 *   Gamma_v = (epsilon_c*sin(psi) - sqrt(epsilon_c - cos^2(psi))) /
 *             (epsilon_c*sin(psi) + sqrt(epsilon_c - cos^2(psi)))
 *
 * where psi = grazing angle (90 - incident angle)
 *       epsilon_c = epsilon_r - j*sigma/(omega*epsilon_0)
 *
 * Reference: Balanis (2016) section 4.7.2, Paul (2006) section 3.2
 * ------------------------------------------------------------------------- */
double ct_ground_reflection_coeff(double epsilon_r, double conductivity_sm,
                                   double freq_hz, double grazing_angle_rad,
                                   re_polarization_t polarization)
{
    if (freq_hz <= 0.0) return 1.0;
    if (grazing_angle_rad <= 0.0) {
        /* At exactly grazing, Gamma = -1 (horizontal) or +1 (vertical) */
        return (polarization == RE_POL_HORIZONTAL) ? 1.0 : 1.0;
    }

    double epsilon_0 = 8.854187817e-12;
    double omega = 2.0 * PI * freq_hz;

    /* Complex permittivity: epsilon_c = epsilon_r - j*sigma/(omega*epsilon_0) */
    double loss_tangent = conductivity_sm / (omega * epsilon_0 * epsilon_r);

    double sin_psi = sin(grazing_angle_rad);
    double cos_psi = cos(grazing_angle_rad);

    /* sqrt(epsilon_c - cos^2(psi)) */
    double re = epsilon_r - cos_psi * cos_psi;
    double im = -loss_tangent * epsilon_r;

    /* Compute sqrt of complex number */
    double mag = sqrt(sqrt(re * re + im * im));
    double arg = atan2(im, re) / 2.0;
    double sqrt_re = mag * cos(arg);
    double sqrt_im = mag * sin(arg);

    /* Compute reflection coefficient */
    double gamma_mag;

    if (polarization == RE_POL_HORIZONTAL) {
        /* Gamma_h = (sin(psi) - sqrt) / (sin(psi) + sqrt) */
        double denom_re = sin_psi + sqrt_re;
        double denom_im = sqrt_im;
        double denom_mag2 = denom_re * denom_re + denom_im * denom_im;

        double num_re = sin_psi - sqrt_re;
        double num_im = -sqrt_im;

        /* |Gamma| = sqrt(num_re^2 + num_im^2) / sqrt(denom_re^2 + denom_im^2) */
        gamma_mag = sqrt((num_re * num_re + num_im * num_im) / denom_mag2);
    } else {
        /* Gamma_v = (epsilon_c*sin(psi) - sqrt) / (epsilon_c*sin(psi) + sqrt) */
        double eps_re = epsilon_r * sin_psi;
        double eps_im = -loss_tangent * epsilon_r * sin_psi;

        double num_re = eps_re - sqrt_re;
        double num_im = eps_im - sqrt_im;
        double denom_re = eps_re + sqrt_re;
        double denom_im = eps_im + sqrt_im;

        gamma_mag = sqrt((num_re * num_re + num_im * num_im) /
                         (denom_re * denom_re + denom_im * denom_im));
    }

    return gamma_mag;
}

/* -------------------------------------------------------------------------
 * L5: Perform NSA measurement at a single frequency and configuration.
 *
 * This simulates the measurement process:
 * 1. Measure V_direct (antennas connected via coaxial adapter)
 * 2. Measure V_site   (antennas at measurement distance over ground plane)
 * 3. Compute NSA = V_direct - V_site
 * 4. Compare with theoretical NSA for validation
 *
 * The +/-4 dB criterion per CISPR 16-1-4 is applied.
 *
 * @param config  Measurement configuration
 * @param af_tx   Transmit antenna factor table
 * @param af_rx   Receive antenna factor table
 * @param result  Output measurement result
 * ------------------------------------------------------------------------- */
void ct_nsa_measure_single(const nsa_config_t *config,
                            const re_antenna_factor_table_t *af_tx,
                            const re_antenna_factor_table_t *af_rx,
                            nsa_measurement_t *result)
{
    if (!config || !result) return;

    memset(result, 0, sizeof(*result));

    result->frequency_hz  = config->frequency_hz;
    result->height_tx_m   = config->height_tx_m;
    result->height_rx_m   = config->height_rx_m;
    result->polarization  = config->polarization;

    /* Interpolate antenna factors */
    double af_tx_db, af_rx_db;
    if (af_tx && re_af_interpolate(af_tx, config->frequency_hz, &af_tx_db) != 0) {
        af_tx_db = 10.0; /* Default */
    }
    if (af_rx && re_af_interpolate(af_rx, config->frequency_hz, &af_rx_db) != 0) {
        af_rx_db = 10.0; /* Default */
    }

    /* Compute free-space NSA (Friis) */
    double g_tx_dbi = 2.0; /* Assume dipole-like */
    double g_rx_dbi = 2.0;
    double nsa_free = nsa_free_space(config->distance_m, config->frequency_hz,
                                      g_tx_dbi, g_rx_dbi);

    /* Compute theoretical NSA with ground plane */
    double nsa_theory = ct_nsa_theoretical(config->distance_m,
                                            config->height_tx_m,
                                            config->height_rx_m,
                                            config->frequency_hz,
                                            config->polarization);

    /* Theoretical NSA including antenna factors */
    result->nsa_theoretical_db = nsa_theory - af_tx_db - af_rx_db;

    /* Simulated measurement:
     * In a real measurement, V_direct and V_site are measured.
     * Here we model V_site = V_direct + NSA_theoretical + noise
     */
    result->v_direct_dbuv   = 100.0; /* Arbitrary reference */
    result->v_site_dbuv     = result->v_direct_dbuv - result->nsa_theoretical_db;
    result->nsa_measured_db = result->nsa_theoretical_db; /* Ideal case */

    /* For a real site, add +/- 2 dB site imperfection (simulated) */
    /* Deviation */
    result->deviation_db = result->nsa_measured_db - result->nsa_theoretical_db;

    /* CISPR 16-1-4: deviation must be within +/- 4 dB */
    result->pass = (fabs(result->deviation_db) <= 4.0) ? 1 : 0;
}

/* -------------------------------------------------------------------------
 * Compute SVSWR from absorber reflectivity.
 *
 * SVSWR = (1 + |Gamma_eff|) / (1 - |Gamma_eff|)
 *
 * Where |Gamma_eff| is the effective reflection coefficient
 * considering the absorber performance and chamber geometry.
 *
 * Absorber refl. [dB] = 20*log10(|Gamma|) -> |Gamma| = 10^(dB/20)
 *
 * SVSWR limit: typically <= 2.0 (6 dB) per CISPR 16-1-4
 *
 * Reference: CISPR 16-1-4:2019 section 6.2.2
 * ------------------------------------------------------------------------- */
double ct_svswr_from_reflectivity(double absorber_reflectivity_db,
                                   double chamber_gain_db)
{
    /* Effective reflection coefficient */
    double gamma_absorber = pow(10.0, absorber_reflectivity_db / 20.0);

    /* Chamber geometry factor: multiple reflections can increase effective Gamma */
    double gamma_eff = gamma_absorber * pow(10.0, chamber_gain_db / 20.0);

    /* Clip to physical range */
    if (gamma_eff >= 1.0) gamma_eff = 0.999;

    /* SVSWR */
    double svswr = (1.0 + gamma_eff) / (1.0 - gamma_eff);

    return svswr;
}

/* -------------------------------------------------------------------------
 * Compute minimum usable distance for far-field measurements.
 *
 * Fraunhofer distance: R_ff = 2*D^2/lambda
 *
 * For CISPR bands:
 *   Band C (30-300 MHz, biconical D~1.37m): R_ff(30) = 2*(1.37)^2/10 = 0.38m -> OK at 3m
 *   Band D (200-1000 MHz, LPDA D~1.5m): R_ff(200) = 2*(1.5)^2/1.5 = 3m -> Marginal at 3m
 *   Band E (1-18 GHz, horn D~0.1m): R_ff(1) = 2*(0.1)^2/0.3 = 0.07m -> OK
 *
 * For SAC with 3m distance, LPDA measurements at low end are in
 * radiating near-field, requiring near-field correction.
 *
 * Reference: CISPR 16-1-4 Annex D
 * ------------------------------------------------------------------------- */
double ct_minimum_far_field_distance(double antenna_dimension_m,
                                      double frequency_hz)
{
    double lambda = SPEED_OF_LIGHT / frequency_hz;

    if (lambda <= 0.0) return 999.0;

    return 2.0 * antenna_dimension_m * antenna_dimension_m / lambda;
}