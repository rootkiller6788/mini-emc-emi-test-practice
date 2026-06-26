/* =========================================================================
 * chamber_qualify.c — Chamber Qualification: SVSWR, Field Uniformity
 *
 * L1: Chamber validation definitions
 * L5: SVSWR measurement procedure, field uniformity evaluation
 * L6: Semi-anechoic chamber qualification
 * L7: CISPR 16-1-4 compliance, IEC 61000-4-3 immunity chamber requirements
 *
 * References:
 *   CISPR 16-1-4:2019 section 6 (Validation of test sites)
 *   IEC 61000-4-3:2020 (Radiated immunity test)
 *   CISPR 16-1-4 Annex E (SVSWR method)
 * ========================================================================= */

#include "radiated_emission.h"
#include "antenna_types.h"
#include "chamber_theory.h"
#include "field_propagation.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define PI 3.14159265358979323846

/* -------------------------------------------------------------------------
 * L5: Perform SVSWR validation measurement.
 *
 * SVSWR (Site Voltage Standing Wave Ratio) evaluates chamber
 * reflectivity by measuring standing wave pattern as antenna
 * is moved along a linear path.
 *
 * Procedure (CISPR 16-1-4 section 6.2):
 * 1. Place transmit antenna at fixed position
 * 2. Move receive antenna along a line (6 positions, min lambda/4 spacing)
 * 3. Record max and min received voltage at each frequency
 * 4. SVSWR = V_max / V_min
 * 5. Acceptance: SVSWR <= 2.0 (6 dB) for each frequency
 *
 * The positions are: 0, +2cm, +4cm, +6cm, +8cm, +10cm (typical)
 * (or use lambda/4 spacing for lower frequencies)
 *
 * @param pos_x_m     Array of measurement positions [m]
 * @param n_positions Number of positions
 * @param voltages    Array of measured voltages [dBuV] (n_positions elements)
 * @param freq_hz     Frequency [Hz]
 * @param result      Output SVSWR measurement
 * ------------------------------------------------------------------------- */
static void svswr_measure_at_frequency(const double *pos_x_m,
                                        size_t n_positions,
                                        const double *voltages_dbuv,
                                        double freq_hz,
                                        svswr_point_t *result)
{
    if (!result || !voltages_dbuv || n_positions < 2) return;

    memset(result, 0, sizeof(*result));
    result->frequency_hz = freq_hz;

    /* Find max and min voltages */
    double v_max = voltages_dbuv[0];
    double v_min = voltages_dbuv[0];
    double best_pos = pos_x_m[0];

    for (size_t i = 1; i < n_positions; i++) {
        if (voltages_dbuv[i] > v_max) {
            v_max = voltages_dbuv[i];
            best_pos = pos_x_m[i];
        }
        if (voltages_dbuv[i] < v_min) {
            v_min = voltages_dbuv[i];
        }
    }

    result->position_x_m = best_pos;
    result->v_max_dbuv = v_max;
    result->v_min_dbuv = v_min;

    /* SVSWR calculation:
     * SVSWR_linear = 10^((V_max_dBuV - V_min_dBuV) / 20)
     * In dB: SVSWR_dB = V_max_dBuV - V_min_dBuV
     */
    double delta_db = v_max - v_min;
    result->svswr_linear = pow(10.0, delta_db / 20.0);
    result->svswr_db = delta_db;

    /* Acceptance: SVSWR <= 2.0 (6 dB) */
    result->pass = (result->svswr_linear <= 2.0) ? 1 : 0;
}

/* -------------------------------------------------------------------------
 * L5: Full SVSWR validation over frequency range.
 *
 * Per CISPR 16-1-4, SVSWR must be measured at:
 * - At least one frequency per octave from 1 GHz to 18 GHz
 * - Each position covering 0 to +10 cm displacement
 *
 * @param report Output SVSWR report
 * ------------------------------------------------------------------------- */
void ct_svswr_validate(svswr_report_t *report)
{
    if (!report) return;

    memset(report, 0, sizeof(*report));
    strncpy(report->chamber_id, "CHAMBER-001", sizeof(report->chamber_id) - 1);

    /* SVSWR test frequencies (one per octave 1-18 GHz) */
    double test_freqs[] = {
        1.0e9, 1.5e9, 2.0e9, 3.0e9, 4.0e9, 6.0e9,
        8.0e9, 12.0e9, 16.0e9, 18.0e9
    };
    size_t n_freqs = sizeof(test_freqs) / sizeof(test_freqs[0]);

    if (n_freqs > SVSWR_MAX_POINTS) n_freqs = SVSWR_MAX_POINTS;

    /* Six measurement positions with 2 cm spacing */
    double positions[] = {0.0, 0.02, 0.04, 0.06, 0.08, 0.10};
    size_t n_pos = 6;

    double worst_svswr = 1.0;
    int all_pass = 1;

    for (size_t f = 0; f < n_freqs; f++) {
        double freq = test_freqs[f];
        double lambda = 299792458.0 / freq;

        /* Simulate standing wave at each position:
         * V(x) = V_0 * |1 + Gamma * exp(-j*2*k*x)|
         * where Gamma is the effective chamber reflection coefficient
         */
        double k = 2.0 * PI / lambda;

        /* Example: absorber reflectivity -30 dB => Gamma = 0.0316 */
        double gamma_chamber = 0.0316; /* -30 dB absorber */

        double voltages_dbuv[6];
        double v_ref = 100.0; /* Reference level in dBuV */

        for (size_t p = 0; p < n_pos; p++) {
            double phase = 2.0 * k * positions[p];
            /* Standing wave: |1 + Gamma*exp(-j*phase)| */
            double cos_phase = cos(phase);
            double mag = sqrt(1.0 + gamma_chamber * gamma_chamber
                              + 2.0 * gamma_chamber * cos_phase);
            voltages_dbuv[p] = v_ref + 20.0 * log10(mag);
        }

        /* Measure SVSWR at this frequency */
        svswr_measure_at_frequency(positions, n_pos, voltages_dbuv,
                                    freq, &report->points[f]);

        if (report->points[f].svswr_db > worst_svswr) {
            worst_svswr = report->points[f].svswr_db;
        }

        if (!report->points[f].pass) {
            all_pass = 0;
        }
    }

    report->num_points     = n_freqs;
    report->worst_svswr_db = worst_svswr;
    report->overall_pass   = all_pass;
}

/* -------------------------------------------------------------------------
 * L5: Field uniformity evaluation per IEC 61000-4-3.
 *
 * For radiated immunity testing, the field must be uniform
 * over a 1.5m x 1.5m plane (16 points, 4x4 grid).
 *
 * Uniformity criterion (IEC 61000-4-3 Ed 4):
 *   |E_max - E_min| / |E_max + E_min| <= 0.25 (4 dB)
 *
 * In dB: 20*log10((1+0.25)/(1-0.25)) = 20*log10(1.667) = 4.44 dB
 *
 * Pre-2012 (Ed 3): <= 6 dB allowed
 *
 * Procedure:
 * 1. Place isotropic field probe at each grid point
 * 2. Record field strength at each point at each test frequency
 * 3. Compute uniformity
 *
 * @param freq_hz    Frequency [Hz]
 * @param fields_vm  Array of 16 field measurements [V/m]
 * @param result     Output uniformity result
 * ------------------------------------------------------------------------- */
void ct_field_uniformity_evaluate(double freq_hz,
                                   const double fields_vm[16],
                                   field_uniformity_point_t *result)
{
    if (!result || !fields_vm) return;

    memset(result, 0, sizeof(*result));
    result->frequency_hz = freq_hz;

    /* Copy field values */
    memcpy(result->e_field_points, fields_vm, 16 * sizeof(double));

    /* Compute mean */
    double sum = 0.0;
    double e_max = fields_vm[0];
    double e_min = fields_vm[0];

    for (int i = 0; i < 16; i++) {
        sum += fields_vm[i];
        if (fields_vm[i] > e_max) e_max = fields_vm[i];
        if (fields_vm[i] < e_min) e_min = fields_vm[i];
    }

    result->e_mean   = sum / 16.0;

    /* Compute standard deviation */
    double var_sum = 0.0;
    for (int i = 0; i < 16; i++) {
        double diff = fields_vm[i] - result->e_mean;
        var_sum += diff * diff;
    }
    result->e_stddev = sqrt(var_sum / 16.0);

    /* Uniformity in dB:
     * uniformity_dB = 20*log10((E_max + E_min)/(E_max - E_min))
     * Acceptable if <= 4 dB (Ed 4) or <= 6 dB (Ed 3)
     */
    if (fabs(e_max - e_min) < 1e-10) {
        result->uniformity_db = 0.0; /* Perfectly uniform */
    } else {
        double ratio = (e_max + e_min) / (e_max - e_min);
        if (ratio < 1.0) ratio = 1.0; /* Prevent log of <1 */
        result->uniformity_db = 20.0 * log10(ratio);
    }

    /* Check against 4 dB criterion (IEC 61000-4-3 Ed 4) */
    result->pass = (result->uniformity_db <= 4.0) ? 1 : 0;
}

/* -------------------------------------------------------------------------
 * L5: Compute anisotropic performance of RF absorber.
 *
 * Absorber reflectivity is typically specified at normal incidence.
 * Off-angle performance degrades:
 *
 *   Gamma(theta) ≈ Gamma(0) * (1 + k_theta * theta^2)
 *
 * For typical pyramidal foam absorber:
 *   Normal: -40 dB at 1 GHz
 *   30 deg:  -35 dB
 *   45 deg:  -28 dB
 *   60 deg:  -18 dB
 *
 * Reference: Emerson & Cuming AN-79, TDK absorber datasheets
 *
 * @param normal_reflectivity_db Reflectivity at normal incidence [dB]
 * @param incident_angle_deg     Incident angle from normal [degrees]
 * @return Estimated reflectivity at angle [dB]
 * ------------------------------------------------------------------------- */
double ct_absorber_angular_reflectivity(double normal_reflectivity_db,
                                         double incident_angle_deg)
{
    /* Convert to radians */
    double theta = incident_angle_deg * PI / 180.0;

    /* Empirical degradation model for pyramidal absorber */
    double degradation_db;

    if (theta < 0.1) {
        degradation_db = 0.0;
    } else if (theta < PI / 6.0) { /* 0-30 degrees */
        degradation_db = 5.0 * sin(theta) * sin(theta);
    } else if (theta < PI / 4.0) { /* 30-45 degrees */
        degradation_db = 12.0 * sin(theta);
    } else if (theta < PI / 3.0) { /* 45-60 degrees */
        degradation_db = 22.0 * sin(theta);
    } else {
        /* Grazing incidence: poor performance */
        degradation_db = 35.0;
    }

    /* Reflectivity degrades (becomes less negative) with angle */
    double angular_reflectivity = normal_reflectivity_db + degradation_db;

    /* Physical limit: cannot exceed 0 dB (total reflection) */
    if (angular_reflectivity > 0.0) angular_reflectivity = 0.0;

    return angular_reflectivity;
}

/* -------------------------------------------------------------------------
 * L5: Compute chamber quiet zone dimensions.
 *
 * The quiet zone is the volume where reflections are sufficiently
 * suppressed for accurate measurements.
 *
 * For a rectangular chamber with absorber on all walls:
 *
 * Quiet zone extent depends on chamber geometry and absorber performance.
 *
 * Approximate quiet zone radius:
 *   R_qz ≈ D_min * sqrt(|Gamma_absorber|)
 *
 * where D_min = minimum distance to any wall
 *
 * Reference: Hemming (2002) "Electromagnetic Anechoic Chambers"
 * ------------------------------------------------------------------------- */
void ct_quiet_zone_estimate(double chamber_lwh_m[3],
                             double absorber_reflectivity_db,
                             double *qz_length_m,
                             double *qz_width_m,
                             double *qz_height_m)
{
    if (!qz_length_m || !qz_width_m || !qz_height_m) return;

    double gamma = pow(10.0, absorber_reflectivity_db / 20.0);

    /* Quiet zone is smaller than chamber by margin proportional to sqrt(|Gamma|) */
    double margin_factor = sqrt(fabs(gamma));

    /* Chamber dimensions minus margin on each side */
    *qz_length_m = chamber_lwh_m[0] * (1.0 - 2.0 * margin_factor);
    *qz_width_m  = chamber_lwh_m[1] * (1.0 - 2.0 * margin_factor);
    *qz_height_m = chamber_lwh_m[2] * (1.0 - 2.0 * margin_factor);

    /* Clamp to minimum of 0.3m */
    if (*qz_length_m < 0.3) *qz_length_m = 0.3;
    if (*qz_width_m  < 0.3) *qz_width_m  = 0.3;
    if (*qz_height_m < 0.3) *qz_height_m = 0.3;
}

/* -------------------------------------------------------------------------
 * L5: Compute ambient noise check for chamber.
 *
 * Ambient (no EUT powered) must be at least 6 dB below limit.
 *
 * Reference: CISPR 16-2-3 section 7.3
 * ------------------------------------------------------------------------- */
int ct_ambient_noise_check(const re_emission_scan_t *ambient_scan,
                            const re_emission_scan_t *eut_scan,
                            double margin_required_db)
{
    if (!ambient_scan || !eut_scan) return -1;

    if (ambient_scan->num_points == 0 && eut_scan->num_points > 0) {
        return 1; /* No ambient noise = good */
    }

    /* Check worst case */
    double worst_ambient = -999.0;
    for (size_t i = 0; i < ambient_scan->num_points; i++) {
        if (ambient_scan->points[i].field_strength_dbuvm > worst_ambient) {
            worst_ambient = ambient_scan->points[i].field_strength_dbuvm;
        }
    }

    double worst_eut = -999.0;
    for (size_t i = 0; i < eut_scan->num_points; i++) {
        if (eut_scan->points[i].field_strength_dbuvm > worst_eut) {
            worst_eut = eut_scan->points[i].field_strength_dbuvm;
        }
    }

    double margin = worst_eut - worst_ambient;

    return (margin >= margin_required_db) ? 1 : 0;
}