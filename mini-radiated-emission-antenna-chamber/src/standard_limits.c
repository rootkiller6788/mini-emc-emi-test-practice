/* =========================================================================
 * standard_limits.c — CISPR / FCC / MIL-STD Radiated Emission Limit Lines
 *
 * L1: Limit line definitions for major EMC standards
 * L4: Friis equation for distance extrapolation
 * L5: Limit query and interpolation algorithms
 * L6: Limit application in compliance testing
 * L7: CISPR 11 (ISM), CISPR 22/32 (ITE/MME), FCC Part 15, MIL-STD-461, CISPR 25
 *
 * References:
 *   CISPR 11:2015 Table 5 (Radiated emissions 30 MHz - 1 GHz)
 *   CISPR 22:2008 Tables 5, 6
 *   CISPR 32:2015 Tables A.2, A.4
 *   47 CFR 15.109 (FCC Part 15)
 *   MIL-STD-461G Figure RE102-1 through RE102-4
 *   CISPR 25:2016 Table 13
 * ========================================================================= */

#include "radiated_emission.h"
#include "emission_limits.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

#define SPEED_OF_LIGHT 299792458.0
#define PI 3.14159265358979323846

/* -------------------------------------------------------------------------
 * Helper: Add a segment to a limit line.
 * ------------------------------------------------------------------------- */
static void add_segment(el_limit_line_t *line,
                         double f_start, double f_end,
                         double peak, double qp, double avg)
{
    if (!line || line->num_segments >= EL_MAX_SEGMENTS) return;

    size_t i = line->num_segments;
    line->segments[i].freq_start_hz    = f_start;
    line->segments[i].freq_end_hz      = f_end;
    line->segments[i].limit_peak_dbuvm = peak;
    line->segments[i].limit_qp_dbuvm   = qp;
    line->segments[i].limit_avg_dbuvm  = avg;
    line->num_segments++;
}

/* -------------------------------------------------------------------------
 * L1+L7: CISPR 22 Class B at 3m.
 *
 * Per CISPR 22:2008 Table 6 (Class B, 10m):
 *   30-230 MHz:   30 dBuV/m QP at 10m
 *   230-1000 MHz: 37 dBuV/m QP at 10m
 *
 * Extrapolated to 3m using 1/r (20*log10(10/3) = 10.46 dB):
 *   30-230 MHz:   40.5 dBuV/m QP at 3m
 *   230-1000 MHz: 47.5 dBuV/m QP at 3m
 *
 * Note: CISPR 22 has been superseded by CISPR 32 but is still widely referenced.
 * ------------------------------------------------------------------------- */
void el_init_cispr22_classb_3m(el_limit_line_t *line)
{
    if (!line) return;
    memset(line, 0, sizeof(*line));

    line->standard   = RE_STD_CISPR22;
    line->class_type = 1; /* Class B */
    line->distance_m = 3.0;
    strncpy(line->description,
            "CISPR 22 Class B at 3m (extrapolated from 10m limits)",
            sizeof(line->description) - 1);

    add_segment(line, 30e6,  230e6,  40.5, 40.5, 30.0);
    add_segment(line, 230e6, 1000e6, 47.5, 47.5, 37.0);
}

/* -------------------------------------------------------------------------
 * CISPR 22 Class A at 10m.
 *
 * CISPR 22:2008 Table 5 (Class A, 10m):
 *   30-230 MHz:   40 dBuV/m QP
 *   230-1000 MHz: 47 dBuV/m QP
 * ------------------------------------------------------------------------- */
void el_init_cispr22_classa_10m(el_limit_line_t *line)
{
    if (!line) return;
    memset(line, 0, sizeof(*line));

    line->standard   = RE_STD_CISPR22;
    line->class_type = 0; /* Class A */
    line->distance_m = 10.0;
    strncpy(line->description,
            "CISPR 22 Class A at 10m",
            sizeof(line->description) - 1);

    add_segment(line, 30e6,  230e6,  50.0, 40.0, 30.0);
    add_segment(line, 230e6, 1000e6, 57.0, 47.0, 37.0);
}

/* -------------------------------------------------------------------------
 * L1+L7: CISPR 32 Class B at 3m (Multimedia Equipment).
 *
 * CISPR 32:2015 Table A.4 (Class B, 3m):
 *   30-230 MHz:   40 dBuV/m QP
 *   230-1000 MHz: 47 dBuV/m QP
 *   1-3 GHz:      70 dBuV/m Peak / 50 dBuV/m Avg
 *   3-6 GHz:      74 dBuV/m Peak / 54 dBuV/m Avg
 *
 * CISPR 32 replaces CISPR 13 (broadcast receivers) and CISPR 22 (ITE).
 * ------------------------------------------------------------------------- */
void el_init_cispr32_classb_3m(el_limit_line_t *line)
{
    if (!line) return;
    memset(line, 0, sizeof(*line));

    line->standard   = RE_STD_CISPR32;
    line->class_type = 1; /* Class B */
    line->distance_m = 3.0;
    strncpy(line->description,
            "CISPR 32 Class B at 3m (Multimedia Equipment)",
            sizeof(line->description) - 1);

    /* Below 1 GHz: QP limits */
    add_segment(line, 30e6,   230e6,   50.0, 40.0, 30.0);
    add_segment(line, 230e6,  1000e6,  57.0, 47.0, 37.0);

    /* Above 1 GHz: Peak and Average limits */
    add_segment(line, 1e9,    3e9,     70.0, 70.0, 50.0);
    add_segment(line, 3e9,    6e9,     74.0, 74.0, 54.0);
}

/* -------------------------------------------------------------------------
 * CISPR 32 Class A at 3m.
 *
 * CISPR 32:2015 Table A.2 (Class A, 3m):
 *   30-230 MHz:   50 dBuV/m QP
 *   230-1000 MHz: 57 dBuV/m QP
 *   1-3 GHz:      76 dBuV/m Peak / 56 dBuV/m Avg
 *   3-6 GHz:      80 dBuV/m Peak / 60 dBuV/m Avg
 * ------------------------------------------------------------------------- */
void el_init_cispr32_classa_3m(el_limit_line_t *line)
{
    if (!line) return;
    memset(line, 0, sizeof(*line));

    line->standard   = RE_STD_CISPR32;
    line->class_type = 0; /* Class A */
    line->distance_m = 3.0;
    strncpy(line->description,
            "CISPR 32 Class A at 3m (Multimedia Equipment)",
            sizeof(line->description) - 1);

    add_segment(line, 30e6,   230e6,   60.0, 50.0, 40.0);
    add_segment(line, 230e6,  1000e6,  67.0, 57.0, 47.0);
    add_segment(line, 1e9,    3e9,     76.0, 76.0, 56.0);
    add_segment(line, 3e9,    6e9,     80.0, 80.0, 60.0);
}

/* -------------------------------------------------------------------------
 * L1+L7: FCC Part 15 Class B at 3m.
 *
 * 47 CFR 15.109(b) (Class B radiated emission limits):
 *   30-88 MHz:     100 uV/m = 40.0 dBuV/m at 3m
 *   88-216 MHz:    150 uV/m = 43.5 dBuV/m at 3m
 *   216-960 MHz:   200 uV/m = 46.0 dBuV/m at 3m
 *   >960 MHz:      500 uV/m = 54.0 dBuV/m (Avg) at 3m
 *                  5000 uV/m = 74.0 dBuV/m (Peak) at 3m  [15.109(g)]
 *
 * Note: FCC limits are specified in uV/m. Conversion:
 *   dBuV/m = 20*log10(E_uV/m)
 * ------------------------------------------------------------------------- */
void el_init_fcc15_classb_3m(el_limit_line_t *line)
{
    if (!line) return;
    memset(line, 0, sizeof(*line));

    line->standard   = RE_STD_FCC_PART15;
    line->class_type = 1; /* Class B */
    line->distance_m = 3.0;
    strncpy(line->description,
            "FCC Part 15 Class B at 3m (47 CFR 15.109)",
            sizeof(line->description) - 1);

    /* Below 1 GHz: QP only (FCC does not specify average below 1 GHz) */
    add_segment(line, 30e6,   88e6,    50.0, 40.0, 40.0);
    add_segment(line, 88e6,   216e6,   53.5, 43.5, 43.5);
    add_segment(line, 216e6,  960e6,   56.0, 46.0, 46.0);

    /* Above 960 MHz: Peak and Average limits per 15.109(g) */
    add_segment(line, 960e6,  10e9,    74.0, 74.0, 54.0);

    /* Above 10 GHz: 15.109(g) references CISPR limits */
    add_segment(line, 10e9,   40e9,    74.0, 74.0, 54.0);
}

/* -------------------------------------------------------------------------
 * FCC Part 15 Class A at 3m.
 *
 * 47 CFR 15.109(a) (Class A):
 *   30-88 MHz:     300 uV/m = 49.5 dBuV/m at 3m
 *   88-216 MHz:    500 uV/m = 54.0 dBuV/m at 3m
 *   216-960 MHz:   700 uV/m = 56.9 dBuV/m at 3m
 *   >960 MHz: Class A digital devices follow CISPR 22/32 Class A
 * ------------------------------------------------------------------------- */
void el_init_fcc15_classa_3m(el_limit_line_t *line)
{
    if (!line) return;
    memset(line, 0, sizeof(*line));

    line->standard   = RE_STD_FCC_PART15;
    line->class_type = 0; /* Class A */
    line->distance_m = 3.0;
    strncpy(line->description,
            "FCC Part 15 Class A at 3m (47 CFR 15.109)",
            sizeof(line->description) - 1);

    add_segment(line, 30e6,   88e6,    59.5, 49.5, 49.5);
    add_segment(line, 88e6,   216e6,   64.0, 54.0, 54.0);
    add_segment(line, 216e6,  960e6,   66.9, 56.9, 56.9);
    add_segment(line, 960e6,  10e9,    80.0, 80.0, 60.0);
    add_segment(line, 10e9,   40e9,    80.0, 80.0, 60.0);
}

/* -------------------------------------------------------------------------
 * L1+L7: CISPR 11 (ISM Equipment) Group 1 Class B at 10m.
 *
 * CISPR 11:2015 Table 5 (Radiated emissions, 30 MHz - 1 GHz):
 *   Group 1 Class A: 40/47 dBuV/m (QP) at 10m
 *   Group 1 Class B: 30/37 dBuV/m (QP) at 10m
 *
 * For measurements at 3m: add 10.46 dB
 *
 * CISPR 11 also covers >1 GHz for Group 2 equipment.
 * ------------------------------------------------------------------------- */
void el_init_cispr11_classb_10m(el_limit_line_t *line)
{
    if (!line) return;
    memset(line, 0, sizeof(*line));

    line->standard   = RE_STD_CISPR11;
    line->class_type = 1; /* Group 1 Class B */
    line->distance_m = 10.0;
    strncpy(line->description,
            "CISPR 11 Group 1 Class B at 10m (ISM Equipment)",
            sizeof(line->description) - 1);

    add_segment(line, 30e6,  230e6,  40.0, 30.0, 20.0);
    add_segment(line, 230e6, 1000e6, 47.0, 37.0, 27.0);

    /* >1 GHz: CISPR 11 refers to CISPR 16-2-3 measurement methods
     * but does not specify limits directly for all equipment types.
     * Table 6 covers 1-18 GHz for Group 2 equipment. */
    add_segment(line, 1e9,   18e9,   70.0, 70.0, 50.0);
}

/* -------------------------------------------------------------------------
 * L1+L7: CISPR 25 Class 5 at 1m (Automotive Components).
 *
 * CISPR 25:2016 Table 13 (Radiated emissions, ALSE method):
 *   For component/module testing with 1m measurement distance.
 *
 * Class 5 is the most stringent level (typical for vehicle manufacturers).
 *
 * Bands:
 *   0.15-30 MHz:    E-field (rod antenna) limits
 *   30-200 MHz:     limits per Class 5
 *   200-1000 MHz:   limits per Class 5
 *   1-2.5 GHz:      limits per Class 5
 *
 * Approximate Class 5 limits (vary slightly between OEM specifications):
 *   30-54 MHz:      16 dBuV/m (Peak)
 *   54-200 MHz:     26 dBuV/m (Peak)
 *   200-400 MHz:    28 dBuV/m (Peak)
 *   400-1000 MHz:   33 dBuV/m (Peak)
 *   1-2.5 GHz:      30 dBuV/m (Average) / 50 dBuV/m (Peak)
 *
 * Reference: CISPR 25:2016 Table 13, Ford EMC-CS-2009
 * ------------------------------------------------------------------------- */
void el_init_cispr25_class5_1m(el_limit_line_t *line)
{
    if (!line) return;
    memset(line, 0, sizeof(*line));

    line->standard   = RE_STD_CISPR25;
    line->class_type = 5; /* Class 5 */
    line->distance_m = 1.0;
    strncpy(line->description,
            "CISPR 25 Class 5 at 1m (Automotive ALSE Method)",
            sizeof(line->description) - 1);

    /* 150 kHz - 30 MHz: Rod antenna / monopole */
    add_segment(line, 150e3,   4e6,     20.0, 20.0, 10.0);
    add_segment(line, 4e6,     20e6,    16.0, 16.0, 6.0);
    add_segment(line, 20e6,    30e6,    16.0, 16.0, 6.0);

    /* 30-1000 MHz: Biconical + LPDA */
    add_segment(line, 30e6,    54e6,    26.0, 16.0, 6.0);
    add_segment(line, 54e6,    200e6,   36.0, 26.0, 16.0);
    add_segment(line, 200e6,   400e6,   38.0, 28.0, 18.0);
    add_segment(line, 400e6,   1000e6,  43.0, 33.0, 23.0);

    /* 1-2.5 GHz: Horn antenna */
    add_segment(line, 1e9,     2.5e9,    50.0, 50.0, 30.0);
}

/* -------------------------------------------------------------------------
 * L1+L7: MIL-STD-461G RE102 at 1m (Navy Surface Ships).
 *
 * MIL-STD-461G Figure RE102-1 (surface ships):
 *
 * Frequency-dependent limits (approximate, actual limits are piecewise):
 *   10 kHz     -> 100 dBuV/m (magnetic field dominant region)
 *   100 kHz    -> 60 dBuV/m
 *   2 MHz      -> 46 dBuV/m
 *   30 MHz     -> 24 dBuV/m
 *   200 MHz    -> 24 dBuV/m
 *   1 GHz      -> 46 dBuV/m
 *   18 GHz     -> 46 dBuV/m
 *
 * MIL-STD-461 uses peak detection with specified bandwidths.
 *
 * Reference: MIL-STD-461G (2015) Figure RE102-1
 * ------------------------------------------------------------------------- */
void el_init_mil461_re102_navy_1m(el_limit_line_t *line)
{
    if (!line) return;
    memset(line, 0, sizeof(*line));

    line->standard   = RE_STD_MIL_STD_461;
    line->class_type = 0; /* Navy surface ships */
    line->distance_m = 1.0;
    strncpy(line->description,
            "MIL-STD-461G RE102 Navy Surface Ships at 1m",
            sizeof(line->description) - 1);

    /* RE102 limit curve is continuous with multiple slope breaks */

    /* 10 kHz - 2 MHz: Magnetic field region (use loop antenna) */
    add_segment(line, 10e3,   100e3,   100.0, 100.0, 100.0);
    add_segment(line, 100e3,  2e6,     60.0,  60.0,  60.0);

    /* 2 MHz - 30 MHz: Transition region */
    add_segment(line, 2e6,    30e6,    46.0,  46.0,  46.0);

    /* 30 MHz - 200 MHz: Flat region */
    add_segment(line, 30e6,   200e6,   24.0,  24.0,  24.0);

    /* 200 MHz - 1 GHz: Rising with frequency */
    add_segment(line, 200e6,  1e9,     38.0,  38.0,  38.0);

    /* 1 GHz - 18 GHz */
    add_segment(line, 1e9,    18e9,    46.0,  46.0,  46.0);
}

/* -------------------------------------------------------------------------
 * L5: Convert limit from one distance to another.
 *
 * Uses far-field 1/r dependence:
 *   E2 = E1 * (d1/d2)   =>   E2_dB = E1_dB + 20*log10(d1/d2)
 *
 * For measurements below the transition frequency (near-field), the
 * 1/r rule may not apply. CISPR 16-2-3 warns against extrapolation
 * below 30 MHz or for loop antennas.
 *
 * For electrically small sources in reactive near-field:
 *   - Electric dipole: E ~ 1/r^3  =>  E2_dB = E1_dB + 60*log10(d1/d2)
 *   - Magnetic dipole: E ~ 1/r^2  =>  E2_dB = E1_dB + 40*log10(d1/d2)
 *
 * CISPR 16-1-4 Annex D recommends using 1/r only when r > lambda/(2*pi).
 *
 * @param limit_original Limit at known distance [dBuV/m]
 * @param d1             Known distance [m]
 * @param d2             Target distance [m]
 * @param frequency_hz   Frequency [Hz]
 * @return Limit at target distance [dBuV/m]
 * ------------------------------------------------------------------------- */
double el_convert_distance(double limit_original, double d1, double d2,
                            double frequency_hz)
{
    if (d1 <= 0.0 || d2 <= 0.0 || frequency_hz <= 0.0) {
        return limit_original;
    }

    /* Determine if far-field condition is met */
    double lambda = SPEED_OF_LIGHT / frequency_hz;
    double r_transition = lambda / (2.0 * PI);

    double exponent;
    if (d1 >= r_transition && d2 >= r_transition) {
        exponent = 1.0;  /* Far-field: 1/r, 20 dB/decade */
    } else if (d1 < r_transition && d2 < r_transition) {
        exponent = 1.5;  /* Near-field: approximately 30 dB/decade */
    } else {
        /* Mixed: conservatively use 20 dB/decade */
        exponent = 1.0;
    }

    return limit_original + 20.0 * exponent * log10(d1 / d2);
}

/* -------------------------------------------------------------------------
 * L5: Query limit at a specific frequency from a limit line.
 *
 * Searches through segments to find the one containing the frequency.
 * If frequency falls in a gap between segments, returns -999.
 * Uses simple step interpolation (constant within segment).
 *
 * @param line     Limit line
 * @param freq_hz  Frequency [Hz]
 * @param detector Detector type
 * @return Limit in dBuV/m
 * ------------------------------------------------------------------------- */
double el_query_limit(const el_limit_line_t *line, double freq_hz,
                       re_detector_type_t detector)
{
    if (!line || line->num_segments == 0) return -999.0;
    if (freq_hz <= 0.0) return -999.0;

    for (size_t i = 0; i < line->num_segments; i++) {
        if (freq_hz >= line->segments[i].freq_start_hz &&
            freq_hz <= line->segments[i].freq_end_hz) {

            switch (detector) {
            case RE_DETECTOR_PEAK:
            case RE_DETECTOR_QUASI_PEAK:
                return (detector == RE_DETECTOR_QUASI_PEAK)
                       ? line->segments[i].limit_qp_dbuvm
                       : line->segments[i].limit_peak_dbuvm;
            case RE_DETECTOR_AVERAGE:
            case RE_DETECTOR_CISPR_AVG:
                return line->segments[i].limit_avg_dbuvm;
            case RE_DETECTOR_RMS:
                return line->segments[i].limit_avg_dbuvm + 2.0; /* RMS is slightly higher */
            default:
                return line->segments[i].limit_peak_dbuvm;
            }
        }
    }

    /* Frequency not covered by this limit line */
    return -999.0;
}

/* -------------------------------------------------------------------------
 * L5: Get standard limit with automatic initialization.
 *
 * This is the main interface function. It initializes the appropriate
 * limit line for the given standard+class+distance and queries it.
 *
 * For distances other than the nominal, applies distance extrapolation.
 * ------------------------------------------------------------------------- */
double el_get_standard_limit(re_standard_t standard, double freq_hz,
                              re_detector_type_t detector,
                              double dist_m, int class_type)
{
    el_limit_line_t line;
    double nominal_dist = 0.0;

    /* Determine which limit line to initialize */
    switch (standard) {
    case RE_STD_CISPR22:
        if (class_type == 1) {
            el_init_cispr22_classb_3m(&line);
            nominal_dist = 3.0;
        } else {
            el_init_cispr22_classa_10m(&line);
            nominal_dist = 10.0;
        }
        break;

    case RE_STD_CISPR32:
    case RE_STD_EN55032:
        if (class_type == 1) {
            el_init_cispr32_classb_3m(&line);
        } else {
            el_init_cispr32_classa_3m(&line);
        }
        nominal_dist = 3.0;
        break;

    case RE_STD_FCC_PART15:
        if (class_type == 1) {
            el_init_fcc15_classb_3m(&line);
        } else {
            el_init_fcc15_classa_3m(&line);
        }
        nominal_dist = 3.0;
        break;

    case RE_STD_CISPR11:
    case RE_STD_EN55011:
        el_init_cispr11_classb_10m(&line);
        nominal_dist = 10.0;
        break;

    case RE_STD_CISPR25:
        el_init_cispr25_class5_1m(&line);
        nominal_dist = 1.0;
        break;

    case RE_STD_MIL_STD_461:
        el_init_mil461_re102_navy_1m(&line);
        nominal_dist = 1.0;
        break;

    default:
        return -999.0;
    }

    /* Query limit at nominal distance */
    double limit = el_query_limit(&line, freq_hz, detector);

    /* If measurement distance differs from nominal, extrapolate */
    if (limit > -900.0 && fabs(dist_m - nominal_dist) > 0.01) {
        limit = el_convert_distance(limit, nominal_dist, dist_m, freq_hz);
    }

    return limit;
}

/* -------------------------------------------------------------------------
 * Convenience: Get standard limit wrapper matching radiated_emission.h API.
 *
 * This function is called by re_get_limit() in the core module.
 * ------------------------------------------------------------------------- */
double re_get_limit(re_standard_t standard, double freq_hz,
                    re_detector_type_t detector, double dist_m,
                    int class_type)
{
    return el_get_standard_limit(standard, freq_hz, detector, dist_m, class_type);
}