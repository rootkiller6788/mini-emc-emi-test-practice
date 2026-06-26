#ifndef ANTENNA_TYPES_H
#define ANTENNA_TYPES_H

#include <stddef.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * L1+L2: Antenna Types for EMC Radiated Emission Measurement
 *
 * CISPR 16-1-4 specifies antenna types for each frequency band:
 *   Band C (30-300 MHz):   Biconical or broadband dipole
 *   Band D (200-1000 MHz): Log-periodic dipole array (LPDA)
 *   Band E/F (>1 GHz):     Double-ridged waveguide horn
 *
 * Reference: Balanis (2016) Antenna Theory, Ch 4, 10, 11, 13
 *           CISPR 16-1-4:2019 Annex C
 * ========================================================================= */

typedef enum {
    AT_LOOP_MAGNETIC         = 0,
    AT_ACTIVE_MONOPOLE       = 1,
    AT_TUNED_DIPOLE          = 2,
    AT_BICONICAL             = 3,
    AT_LOG_PERIODIC          = 4,
    AT_DOUBLE_RIDGED_HORN    = 5,
    AT_STANDARD_GAIN_HORN    = 6,
    AT_CONICAL_LOG_SPIRAL    = 7,
    AT_BROADBAND_DIPOLE      = 8
} antenna_type_t;

typedef enum {
    FAR_REACTIVE_NEAR  = 0,
    FAR_RADIATING_NEAR = 1,
    FAR_FRAUNHOFER     = 2
} far_region_t;

typedef struct {
    double max_dimension_m;
    double aperture_area_m2;
    double boom_length_m;
    double element_spacing_m;
    double flare_angle_deg;
    double half_angle_deg;
    double input_connector_ohm;
} antenna_geometry_t;

#define AT_MAX_PATTERN_ANGLES 360

typedef struct {
    size_t   num_angles;
    double   theta_deg[AT_MAX_PATTERN_ANGLES];
    double   e_plane_pattern[AT_MAX_PATTERN_ANGLES];
    double   h_plane_pattern[AT_MAX_PATTERN_ANGLES];
    double   max_gain_linear;
    double   half_power_beamwidth_e_deg;
    double   half_power_beamwidth_h_deg;
    double   front_to_back_ratio_db;
} antenna_pattern_t;

#define AT_MAX_NAME 64
#define AT_MAX_SN   32

typedef struct {
    antenna_type_t     type;
    char               model[AT_MAX_NAME];
    char               serial[AT_MAX_SN];
    antenna_geometry_t geometry;
    antenna_pattern_t  pattern;
    double             frequency_min_hz;
    double             frequency_max_hz;
    double             nominal_gain_dbi;
    double             nominal_impedance_ohm;
    double             vswr_max;
    double             max_continuous_power_w;
    double             balun_loss_db;
    double             cross_pol_isolation_db;
} antenna_descriptor_t;

typedef struct {
    double cone_length_m;
    double cone_half_angle_deg;
    double cage_radius_m;
    size_t num_elements;
    double element_length_m;
    double balun_ratio;
} biconical_params_t;

typedef struct {
    double scale_factor_tau;
    double spacing_factor_sigma;
    size_t num_dipoles;
    double longest_element_m;
    double shortest_element_m;
    double boom_length_m;
    double characteristic_impedance_ohm;
} lpda_params_t;

typedef struct {
    double aperture_width_m;
    double aperture_height_m;
    double axial_length_m;
    double flare_angle_e_deg;
    double flare_angle_h_deg;
    double ridge_gap_m;
    double ridge_width_m;
    double cutoff_freq_hz;
} horn_params_t;

typedef struct {
    double loop_diameter_m;
    size_t num_turns;
    double wire_diameter_m;
    double shield_gap_mm;
    double electrostatic_shield;
} loop_params_t;

/* =========================================================================
 * API: Antenna Parameter Computation
 * ========================================================================= */

/**
 * Compute the far-field boundary (Fraunhofer distance).
 * R_ff = 2 * D^2 / lambda
 * R_nf = 0.62 * sqrt(D^3 / lambda)
 * Reference: Balanis (2016) section 2.2, IEEE Std 145-2013
 */
void at_far_field_boundary(double D, double frequency_hz,
                            double *r_nf, double *r_ff);

/**
 * Compute theoretical gain of a biconical antenna.
 * Ideal infinite biconical: G approx 1.5 (1.76 dBi)
 * Reference: Balanis (2016) section 4.4
 */
double at_biconical_gain(double half_angle_deg, double freq_hz,
                          double cone_length_m);

/**
 * Compute theoretical gain of LPDA.
 * Carrel (1961): G approx 7.08*tau + 4.25*sigma
 * Reference: Balanis (2016) section 11.3
 */
double at_lpda_gain(double tau, double sigma, size_t n_dipoles);

/**
 * Compute theoretical gain of a horn antenna.
 * G = (4*pi / lambda^2) * A_e, A_e = e_ap * A_p, e_ap approx 0.5
 * Reference: Balanis (2016) section 13.4
 */
double at_horn_gain(double aperture_width_m, double aperture_height_m,
                     double freq_hz);

/**
 * Compute beamwidth for horn antenna.
 * E-plane HPBW approx 56 / (A_h/lambda) deg
 * H-plane HPBW approx 67 / (A_w/lambda) deg
 * Reference: Balanis (2016) section 13.5
 */
void at_horn_beamwidth(double aperture_width_m, double aperture_height_m,
                        double freq_hz, double *hp_e_deg, double *hp_h_deg);

/** Compute wavelength: lambda = c / f, c = 299792458 m/s */
double at_wavelength(double frequency_hz);

/**
 * Compute free-space path loss (Friis equation dB form):
 * FSPL = 20*log10(4*pi*d/lambda) = 20*log10(d)+20*log10(f)-147.55
 * Reference: Friis (1946)
 */
double at_free_space_path_loss(double distance_m, double freq_hz);

/**
 * Estimate antenna factor from geometric parameters.
 * AF_est = 20*log10(f_MHz) - G_dBi - 29.78 + correction
 */
double at_estimate_antenna_factor(antenna_type_t type, double freq_hz,
                                   const antenna_geometry_t *geom);

#ifdef __cplusplus
}
#endif

#endif /* ANTENNA_TYPES_H */
