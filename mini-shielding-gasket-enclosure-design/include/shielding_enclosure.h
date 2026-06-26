#ifndef SHIELDING_ENCLOSURE_H
#define SHIELDING_ENCLOSURE_H

#include "shielding_gasket.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===================================================================
 * L1: Enclosure-specific type extensions and constants
 * =================================================================== */

/** Maximum number of apertures per enclosure face */
#define MAX_APERTURES_PER_ENCLOSURE 20

/** Minimum screw spacing for effective seam closure [m] */
#define MIN_SCREW_SPACING_M 0.025

/** Typical enclosure design margins */
#define ENCLOSURE_THICKNESS_MARGIN 1.5

/** Aperture array for multiple openings on a single face */
typedef struct {
    int face_index;                          /**< 0=front, 1=back, 2=left, 3=right, 4=top, 5=bottom */
    aperture_spec_t apertures[MAX_APERTURES_PER_ENCLOSURE];
    int num_apertures;
} enclosure_face_t;

/** Complete enclosure with per-face aperture specifications */
typedef struct {
    enclosure_spec_t base;
    enclosure_face_t faces[6];  /**< Up to 6 faces */
    int num_faces_used;
} detailed_enclosure_t;

/* ===================================================================
 * L5: Enclosure Design Rule Checks
 * =================================================================== */

/**
 * @brief Check if screw spacing meets lambda/20 rule for SE.
 * Screws should be spaced less than lambda/20 at max frequency
 * to prevent seam leakage between fasteners.
 *
 * @param spacing_m  Distance between adjacent screws [m]
 * @param max_freq_hz  Maximum frequency of concern [Hz]
 * @return 1 if spacing is adequate, 0 if too large
 */
int enclosure_check_screw_spacing(double spacing_m, double max_freq_hz);

/**
 * @brief Recommend screw count for perimeter of given length.
 * N >= perimeter / (lambda_min/20)
 *
 * @param perimeter_m  Total seam perimeter [m]
 * @param max_freq_hz  Maximum frequency [Hz]
 * @return Recommended number of screws (minimum)
 */
int enclosure_recommend_screw_count(double perimeter_m, double max_freq_hz);

/**
 * @brief Check if enclosure dimensions avoid cavity resonance
 * in a specified frequency band.
 *
 * @param enclosure  Enclosure specification
 * @param f_start_hz  Start of frequency band [Hz]
 * @param f_end_hz    End of frequency band [Hz]
 * @return 0 if band is clear, 1 if resonance found in band
 */
int enclosure_check_resonance_band(const enclosure_spec_t *enclosure,
    double f_start_hz, double f_end_hz);

/**
 * @brief Compute the waveguide-below-cutoff SE for a honeycomb vent.
 *
 * SE = 27.3 * (t/d) * sqrt(1 - (f/f_c)^2)  for f < f_c
 * where f_c = c0/(1.706*d) for circular cells
 *
 * @param cell_diameter_m  Honeycomb cell diameter [m]
 * @param thickness_m      Honeycomb panel thickness [m]
 * @param freq_hz          Frequency [Hz]
 * @return Waveguide SE [dB]
 */
double enclosure_honeycomb_se(double cell_diameter_m, double thickness_m, double freq_hz);

/**
 * @brief Compute the minimum slot length that causes SE degradation.
 * For a given SE target, any slot longer than this will cause failure.
 *
 * L_max = lambda / (2 * 10^(SE_target/20))
 *
 * @param target_se_db  Target SE [dB]
 * @param freq_hz       Frequency [Hz]
 * @return Maximum allowable slot length [m]
 */
double enclosure_max_allowable_slot(double target_se_db, double freq_hz);

#ifdef __cplusplus
}
#endif
#endif /* SHIELDING_ENCLOSURE_H */
