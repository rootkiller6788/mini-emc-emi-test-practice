#ifndef EMISSION_LIMITS_H
#define EMISSION_LIMITS_H

#include <stddef.h>
#include <math.h>
#include "radiated_emission.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * L1+L4: Radiated Emission Limit Lines
 *
 * Reference: CISPR 11/22/32, FCC Part 15, MIL-STD-461G, CISPR 25
 * ========================================================================= */

typedef struct {
    double freq_start_hz;
    double freq_end_hz;
    double limit_peak_dbuvm;
    double limit_qp_dbuvm;
    double limit_avg_dbuvm;
} el_limit_segment_t;

#define EL_MAX_SEGMENTS 32

typedef struct {
    re_standard_t standard;
    int           class_type;
    double        distance_m;
    size_t        num_segments;
    el_limit_segment_t segments[EL_MAX_SEGMENTS];
    char          description[256];
} el_limit_line_t;

void el_init_cispr22_classb_3m(el_limit_line_t *line);
void el_init_cispr22_classa_10m(el_limit_line_t *line);
void el_init_cispr32_classb_3m(el_limit_line_t *line);
void el_init_cispr32_classa_3m(el_limit_line_t *line);
void el_init_fcc15_classb_3m(el_limit_line_t *line);
void el_init_fcc15_classa_3m(el_limit_line_t *line);
void el_init_cispr11_classb_10m(el_limit_line_t *line);
void el_init_cispr25_class5_1m(el_limit_line_t *line);
void el_init_mil461_re102_navy_1m(el_limit_line_t *line);
double el_convert_distance(double limit_original, double d1, double d2,
                            double frequency_hz);
double el_query_limit(const el_limit_line_t *line, double freq_hz,
                       re_detector_type_t detector);
double el_get_standard_limit(re_standard_t standard, double freq_hz,
                              re_detector_type_t detector,
                              double dist_m, int class_type);

#ifdef __cplusplus
}
#endif

#endif /* EMISSION_LIMITS_H */