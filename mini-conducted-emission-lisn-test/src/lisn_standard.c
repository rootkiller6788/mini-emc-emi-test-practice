/** @file lisn_standard.c - CISPR/FCC/MIL Conducted Emission Limit Lines */
/** Standards: CISPR 11/14/22/25/32, FCC Part 15, MIL-STD-461G, DO-160 */
/** L1(standard limits), L4(regulatory requirements), L6(compliance testing), L7(applications) */

#include "lisn_standard.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* CISPR 22 Class A (Industrial ITE) conducted emission limits */
/* 150 kHz - 500 kHz: 79 dBuV QP, 66 dBuV AVG */
/* 500 kHz - 30 MHz:  73 dBuV QP, 60 dBuV AVG */
/* Reference: CISPR 22:2008 Table 1 */
void lisn_cispr22_class_a_limit(lisn_limit_line_t *line) {
    if (!line) return;
    memset(line, 0, sizeof(lisn_limit_line_t));
    line->standard = STD_CISPR_22;
    line->emission_class = CLASS_A;
    strncpy(line->description, "CISPR 22 Class A - ITE Industrial", 127);
    line->freq_start_hz = 150000.0;
    line->freq_stop_hz = 30000000.0;
    line->num_points = 2;
    line->points = malloc(2 * sizeof(lisn_limit_point_t));
    line->points_allocated = 1;
    if (!line->points) return;
    line->points[0].frequency_hz = 150000.0;
    line->points[0].qp_limit_dbuV = 79.0;
    line->points[0].avg_limit_dbuV = 66.0;
    line->points[1].frequency_hz = 30000000.0;
    line->points[1].qp_limit_dbuV = 73.0;
    line->points[1].avg_limit_dbuV = 60.0;
}

/* CISPR 22 Class B (Residential ITE) conducted emission limits */
/* 150-500 kHz: 66-56 dBuV QP, 56-46 dBuV AVG (decreasing log-linearly) */
/* 500 kHz-5 MHz: 56 dBuV QP, 46 dBuV AVG */
/* 5-30 MHz: 60 dBuV QP, 50 dBuV AVG */
void lisn_cispr22_class_b_limit(lisn_limit_line_t *line) {
    if (!line) return;
    memset(line, 0, sizeof(lisn_limit_line_t));
    line->standard = STD_CISPR_22;
    line->emission_class = CLASS_B;
    strncpy(line->description, "CISPR 22 Class B - ITE Residential", 127);
    line->freq_start_hz = 150000.0;
    line->freq_stop_hz = 30000000.0;
    line->num_points = 4;
    line->points = malloc(4 * sizeof(lisn_limit_point_t));
    line->points_allocated = 1;
    if (!line->points) return;
    line->points[0].frequency_hz = 150000.0;
    line->points[0].qp_limit_dbuV = 66.0;
    line->points[0].avg_limit_dbuV = 56.0;
    line->points[1].frequency_hz = 500000.0;
    line->points[1].qp_limit_dbuV = 56.0;
    line->points[1].avg_limit_dbuV = 46.0;
    line->points[2].frequency_hz = 5000000.0;
    line->points[2].qp_limit_dbuV = 56.0;
    line->points[2].avg_limit_dbuV = 46.0;
    line->points[3].frequency_hz = 30000000.0;
    line->points[3].qp_limit_dbuV = 60.0;
    line->points[3].avg_limit_dbuV = 50.0;
}

/* CISPR 25 Class 5 (Automotive, most stringent) conducted limits */
/* For EV/HEV high-voltage systems: 150 kHz - 108 MHz */
/* Multiple decreasing segments per CISPR 25:2016 Table 6 */
void lisn_cispr25_class5_limit(lisn_limit_line_t *line) {
    if (!line) return;
    memset(line, 0, sizeof(lisn_limit_line_t));
    line->standard = STD_CISPR_25;
    line->emission_class = CLASS_5;
    strncpy(line->description, "CISPR 25 Class 5 - Automotive HV", 127);
    line->freq_start_hz = 150000.0;
    line->freq_stop_hz = 108000000.0;
    line->num_points = 8;
    line->points = malloc(8 * sizeof(lisn_limit_point_t));
    line->points_allocated = 1;
    if (!line->points) return;
    /* Segment 1: 150-300 kHz, 66-53 dBuV QP */
    line->points[0].frequency_hz = 150000.0;
    line->points[0].qp_limit_dbuV = 66.0; line->points[0].avg_limit_dbuV = 56.0;
    line->points[1].frequency_hz = 300000.0;
    line->points[1].qp_limit_dbuV = 53.0; line->points[1].avg_limit_dbuV = 43.0;
    /* Segment 2: 300-530 kHz, 53-43 dBuV QP */
    line->points[2].frequency_hz = 530000.0;
    line->points[2].qp_limit_dbuV = 43.0; line->points[2].avg_limit_dbuV = 33.0;
    /* Segment 3: 530 kHz-2 MHz, 43-33 dBuV QP */
    line->points[3].frequency_hz = 2000000.0;
    line->points[3].qp_limit_dbuV = 33.0; line->points[3].avg_limit_dbuV = 23.0;
    /* Segment 4: 2-30 MHz, 33-22 dBuV QP */
    line->points[4].frequency_hz = 30000000.0;
    line->points[4].qp_limit_dbuV = 22.0; line->points[4].avg_limit_dbuV = 12.0;
    /* Segment 5: 30-54 MHz */
    line->points[5].frequency_hz = 54000000.0;
    line->points[5].qp_limit_dbuV = 22.0; line->points[5].avg_limit_dbuV = 12.0;
    /* Segment 6: 54-72 MHz */
    line->points[6].frequency_hz = 72000000.0;
    line->points[6].qp_limit_dbuV = 17.0; line->points[6].avg_limit_dbuV = 7.0;
    /* Segment 7: 72-108 MHz */
    line->points[7].frequency_hz = 108000000.0;
    line->points[7].qp_limit_dbuV = 12.0; line->points[7].avg_limit_dbuV = 2.0;
}

/* CISPR 32 Class A (Multimedia Equipment, Industrial) */
/* Supersedes CISPR 13 and CISPR 22 */
void lisn_cispr32_class_a_limit(lisn_limit_line_t *line) {
    if (!line) return;
    memset(line, 0, sizeof(lisn_limit_line_t));
    line->standard = STD_CISPR_32; line->emission_class = CLASS_A;
    strncpy(line->description, "CISPR 32 Class A - Multimedia Industrial", 127);
    line->freq_start_hz = 150000.0; line->freq_stop_hz = 30000000.0;
    line->num_points = 2;
    line->points = malloc(2 * sizeof(lisn_limit_point_t));
    line->points_allocated = 1;
    if (!line->points) return;
    line->points[0].frequency_hz = 150000.0;
    line->points[0].qp_limit_dbuV = 79.0; line->points[0].avg_limit_dbuV = 66.0;
    line->points[1].frequency_hz = 30000000.0;
    line->points[1].qp_limit_dbuV = 73.0; line->points[1].avg_limit_dbuV = 60.0;
}

/* CISPR 32 Class B (Multimedia Equipment, Residential) */
void lisn_cispr32_class_b_limit(lisn_limit_line_t *line) {
    if (!line) return;
    memset(line, 0, sizeof(lisn_limit_line_t));
    line->standard = STD_CISPR_32; line->emission_class = CLASS_B;
    strncpy(line->description, "CISPR 32 Class B - Multimedia Residential", 127);
    line->freq_start_hz = 150000.0; line->freq_stop_hz = 30000000.0;
    line->num_points = 3;
    line->points = malloc(3 * sizeof(lisn_limit_point_t));
    line->points_allocated = 1;
    if (!line->points) return;
    line->points[0].frequency_hz = 150000.0;
    line->points[0].qp_limit_dbuV = 66.0; line->points[0].avg_limit_dbuV = 56.0;
    line->points[1].frequency_hz = 500000.0;
    line->points[1].qp_limit_dbuV = 56.0; line->points[1].avg_limit_dbuV = 46.0;
    line->points[2].frequency_hz = 30000000.0;
    line->points[2].qp_limit_dbuV = 60.0; line->points[2].avg_limit_dbuV = 50.0;
}

/* FCC Part 15 Subpart B Class B conducted emission limits */
/* 150 kHz - 450 kHz: QP limit decreases log-linearly */
/* 450 kHz - 30 MHz: 48 dBuV QP (yes, FCC uses 48 not 46) */
/* Reference: 47 CFR 15.107(a) */
void lisn_fcc_part15_class_b_limit(lisn_limit_line_t *line) {
    if (!line) return;
    memset(line, 0, sizeof(lisn_limit_line_t));
    line->standard = STD_FCC_PART_15; line->emission_class = CLASS_B;
    strncpy(line->description, "FCC Part 15 Class B - Residential", 127);
    line->freq_start_hz = 150000.0; line->freq_stop_hz = 30000000.0;
    line->num_points = 3;
    line->points = malloc(3 * sizeof(lisn_limit_point_t));
    line->points_allocated = 1;
    if (!line->points) return;
    line->points[0].frequency_hz = 150000.0;
    line->points[0].qp_limit_dbuV = 66.0; line->points[0].avg_limit_dbuV = 56.0;
    line->points[1].frequency_hz = 450000.0;
    line->points[1].qp_limit_dbuV = 48.0; line->points[1].avg_limit_dbuV = 48.0;
    line->points[2].frequency_hz = 30000000.0;
    line->points[2].qp_limit_dbuV = 48.0; line->points[2].avg_limit_dbuV = 48.0;
}

/* MIL-STD-461G CE102 conducted emission limits for power leads */
/* 10 kHz - 10 MHz, voltage-dependent relaxation */
/* Reference: MIL-STD-461G Figure CE102-1 */
void lisn_mil461_ce102_limit(lisn_limit_line_t *line, double nominal_voltage_v) {
    if (!line) return;
    memset(line, 0, sizeof(lisn_limit_line_t));
    line->standard = STD_MIL_STD_461; line->emission_class = CLASS_A;
    strncpy(line->description, "MIL-STD-461G CE102 Power Leads", 127);
    line->freq_start_hz = 10000.0; line->freq_stop_hz = 10000000.0;
    line->num_points = 6;
    line->points = malloc(6 * sizeof(lisn_limit_point_t));
    line->points_allocated = 1;
    if (!line->points) return;
    /* Basic curve (28V system) */
    double relax = (nominal_voltage_v > 28.0) ? 20.0 * log10(nominal_voltage_v / 28.0) : 0.0;
    line->points[0].frequency_hz = 10000.0;
    line->points[0].qp_limit_dbuV = 94.0 + relax; line->points[0].avg_limit_dbuV = 0.0;
    line->points[1].frequency_hz = 2000.0; /* Transition starts at 2 kHz */
    line->points[1].qp_limit_dbuV = 94.0 + relax; line->points[1].avg_limit_dbuV = 0.0;
    line->points[2].frequency_hz = 2000.0;
    line->points[2].qp_limit_dbuV = 94.0 + relax; line->points[2].avg_limit_dbuV = 0.0;
    line->points[3].frequency_hz = 100000.0;
    line->points[3].qp_limit_dbuV = 60.0 + relax; line->points[3].avg_limit_dbuV = 0.0;
    line->points[4].frequency_hz = 2000000.0;
    line->points[4].qp_limit_dbuV = 60.0 + relax; line->points[4].avg_limit_dbuV = 0.0;
    line->points[5].frequency_hz = 10000000.0;
    line->points[5].qp_limit_dbuV = 60.0 + relax; line->points[5].avg_limit_dbuV = 0.0;
}

/* CISPR 11 Group 1 Class A (ISM Equipment, Industrial) */
/* Reference: CISPR 11:2015 Table 2a */
void lisn_cispr11_group1_class_a_limit(lisn_limit_line_t *line) {
    if (!line) return;
    memset(line, 0, sizeof(lisn_limit_line_t));
    line->standard = STD_CISPR_11; line->emission_class = CLASS_A;
    strncpy(line->description, "CISPR 11 Group 1 Class A - ISM Industrial", 127);
    line->freq_start_hz = 150000.0; line->freq_stop_hz = 30000000.0;
    line->num_points = 2;
    line->points = malloc(2 * sizeof(lisn_limit_point_t));
    line->points_allocated = 1;
    if (!line->points) return;
    line->points[0].frequency_hz = 150000.0;
    line->points[0].qp_limit_dbuV = 79.0; line->points[0].avg_limit_dbuV = 66.0;
    line->points[1].frequency_hz = 30000000.0;
    line->points[1].qp_limit_dbuV = 73.0; line->points[1].avg_limit_dbuV = 60.0;
}

/* CISPR 14 (Household Appliances / Electric Tools) conducted limits */
/* 150-500 kHz: 66-56 dBuV QP (decreasing), 500k-5M: 56 dBuV, 5-30M: 60 dBuV */
/* Reference: CISPR 14-1:2016 Table 1 */
void lisn_cispr14_limit(lisn_limit_line_t *line) {
    if (!line) return;
    memset(line, 0, sizeof(lisn_limit_line_t));
    line->standard = STD_CISPR_14; line->emission_class = CLASS_B;
    strncpy(line->description, "CISPR 14 - Household Appliances", 127);
    line->freq_start_hz = 150000.0; line->freq_stop_hz = 30000000.0;
    line->num_points = 4;
    line->points = malloc(4 * sizeof(lisn_limit_point_t));
    line->points_allocated = 1;
    if (!line->points) return;
    line->points[0].frequency_hz = 150000.0;
    line->points[0].qp_limit_dbuV = 66.0; line->points[0].avg_limit_dbuV = 56.0;
    line->points[1].frequency_hz = 500000.0;
    line->points[1].qp_limit_dbuV = 56.0; line->points[1].avg_limit_dbuV = 46.0;
    line->points[2].frequency_hz = 5000000.0;
    line->points[2].qp_limit_dbuV = 56.0; line->points[2].avg_limit_dbuV = 46.0;
    line->points[3].frequency_hz = 30000000.0;
    line->points[3].qp_limit_dbuV = 60.0; line->points[3].avg_limit_dbuV = 50.0;
}

/* L5: Interpolate limit at arbitrary frequency (log-linear interpolation) */
/* L(f) = L1 + (L2-L1) * log10(f/f1) / log10(f2/f1) */
int lisn_interpolate_limit(const lisn_limit_line_t *line, double f,
                            double *qp_lim, double *avg_lim) {
    if (!line || !line->points || line->num_points < 1 || !qp_lim || !avg_lim) return -1;
    if (f < line->freq_start_hz || f > line->freq_stop_hz) return -1;
    if (line->num_points == 1) {
        *qp_lim = line->points[0].qp_limit_dbuV;
        *avg_lim = line->points[0].avg_limit_dbuV;
        return 0;
    }
    int i;
    for (i = 0; i < line->num_points - 1; i++) {
        double f1 = line->points[i].frequency_hz;
        double f2 = line->points[i+1].frequency_hz;
        if (f >= f1 && f <= f2) {
            double frac = log10(f / f1) / log10(f2 / f1);
            *qp_lim = line->points[i].qp_limit_dbuV + frac * (line->points[i+1].qp_limit_dbuV - line->points[i].qp_limit_dbuV);
            *avg_lim = line->points[i].avg_limit_dbuV + frac * (line->points[i+1].avg_limit_dbuV - line->points[i].avg_limit_dbuV);
            return 0;
        }
    }
    /* f beyond last point: use last point value */
    *qp_lim = line->points[line->num_points-1].qp_limit_dbuV;
    *avg_lim = line->points[line->num_points-1].avg_limit_dbuV;
    return 0;
}

/* L6: Check single measurement point against applicable limit */
int lisn_check_point(const lisn_limit_line_t *line, double freq,
                      double meas_qp, double meas_avg,
                      lisn_compliance_point_t *result) {
    if (!line || !result) return -1;
    double qp_lim, avg_lim;
    if (lisn_interpolate_limit(line, freq, &qp_lim, &avg_lim) != 0) return -1;
    result->frequency_hz = freq;
    result->measured_qp_dbuV = meas_qp;
    result->limit_qp_dbuV = qp_lim;
    result->margin_qp_db = qp_lim - meas_qp;
    result->measured_avg_dbuV = meas_avg;
    result->limit_avg_dbuV = avg_lim;
    result->margin_avg_db = avg_lim - meas_avg;
    result->qp_pass = (meas_qp <= qp_lim) ? 1 : 0;
    result->avg_pass = (meas_avg <= avg_lim) ? 1 : 0;
    return 0;
}

/* L6: Full compliance check - compare measurement dataset against limit */
int lisn_compliance_check(const lisn_measurement_dataset_t *ds,
                           const lisn_limit_line_t *line,
                           lisn_compliance_report_t *report) {
    if (!ds || !line || !report || ds->num_results <= 0) return -1;
    report->num_points = ds->num_results;
    report->points = malloc(ds->num_results * sizeof(lisn_compliance_point_t));
    if (!report->points) return -1;
    report->overall_pass = 1;
    report->worst_margin_db = 999.0;
    report->worst_frequency_hz = ds->sweep.freq_start_hz;
    int i;
    for (i = 0; i < ds->num_results; i++) {
        lisn_compliance_point_t cp;
        memset(&cp, 0, sizeof(cp));
        if (lisn_check_point(line, ds->results[i].frequency_hz,
                              ds->results[i].qp_level_dbuV,
                              ds->results[i].avg_level_dbuV, &cp) == 0) {
            report->points[i] = cp;
            if (!cp.qp_pass || !cp.avg_pass) report->overall_pass = 0;
            if (cp.margin_qp_db < report->worst_margin_db) {
                report->worst_margin_db = cp.margin_qp_db;
                report->worst_frequency_hz = cp.frequency_hz;
            }
            if (cp.margin_avg_db < report->worst_margin_db) {
                report->worst_margin_db = cp.margin_avg_db;
                report->worst_frequency_hz = cp.frequency_hz;
            }
        }
    }
    /* Copy limit line info */
    memcpy(&report->limit_line, line, sizeof(lisn_limit_line_t));
    if (report->overall_pass)
        snprintf(report->verdict, 255, "PASS - Worst margin: %.1f dB at %.1f kHz",
                 report->worst_margin_db, report->worst_frequency_hz / 1000.0);
    else
        snprintf(report->verdict, 255, "FAIL - Worst margin: %.1f dB at %.1f kHz",
                 report->worst_margin_db, report->worst_frequency_hz / 1000.0);
    return 0;
}

/* L6: Find frequency with worst (smallest) margin */
void lisn_find_worst_margin(const lisn_compliance_report_t *r,
                             double *wf_qp, double *wm_qp,
                             double *wf_avg, double *wm_avg) {
    if (!r || !r->points || r->num_points <= 0) return;
    *wf_qp = r->points[0].frequency_hz; *wm_qp = r->points[0].margin_qp_db;
    *wf_avg = r->points[0].frequency_hz; *wm_avg = r->points[0].margin_avg_db;
    int i;
    for (i = 1; i < r->num_points; i++) {
        if (r->points[i].margin_qp_db < *wm_qp) {
            *wm_qp = r->points[i].margin_qp_db; *wf_qp = r->points[i].frequency_hz;
        }
        if (r->points[i].margin_avg_db < *wm_avg) {
            *wm_avg = r->points[i].margin_avg_db; *wf_avg = r->points[i].frequency_hz;
        }
    }
}

/* Free dynamically allocated limit line */
void lisn_limit_line_free(lisn_limit_line_t *line) {
    if (!line) return;
    if (line->points_allocated && line->points) {
        free(line->points); line->points = NULL; line->points_allocated = 0;
    }
}

/* Free compliance report memory */
void lisn_compliance_report_free(lisn_compliance_report_t *report) {
    if (!report) return;
    if (report->points) { free(report->points); report->points = NULL; }
    report->num_points = 0;
}

/* L6: Print human-readable compliance report */
void lisn_print_compliance_report(const lisn_compliance_report_t *r) {
    if (!r) return;
    printf("========================================\\n");
    printf("  CONDUCTED EMISSION COMPLIANCE REPORT\\n");
    printf("========================================\\n");
    printf("  Standard: %s\\n", r->limit_line.description);
    printf("  Frequency Range: %.1f kHz - %.1f MHz\\n",
           r->limit_line.freq_start_hz / 1000.0, r->limit_line.freq_stop_hz / 1000000.0);
    printf("  Points Checked: %d\\n", r->num_points);
    printf("  Worst Margin: %.1f dB at %.3f MHz\\n",
           r->worst_margin_db, r->worst_frequency_hz / 1000000.0);
    printf("  Verdict: %s\\n", r->verdict);
    printf("========================================\\n");
}
/* RTCA DO-160 Section 21 - Aircraft conducted emissions */
/* 150 kHz - 152 MHz for airborne equipment */
void lisn_do160_section21_limit(lisn_limit_line_t *line, int category) {
    if (!line) return;
    memset(line, 0, sizeof(lisn_limit_line_t));
    line->standard = STD_DO_160; line->emission_class = CLASS_A;
    strncpy(line->description, "DO-160 Section 21 - Aircraft CE", 127);
    line->freq_start_hz = 150000.0; line->freq_stop_hz = 152000000.0;
    line->num_points = 5;
    line->points = malloc(5 * sizeof(lisn_limit_point_t));
    line->points_allocated = 1;
    if (!line->points) return;
    /* Category B (most common): 150k-2M: 30 dBuV, 2M-30M: 20, 30M-152M: 12 */
    double base = (category == 1) ? 20.0 : 30.0;
    line->points[0].frequency_hz = 150000.0;
    line->points[0].qp_limit_dbuV = base; line->points[0].avg_limit_dbuV = base;
    line->points[1].frequency_hz = 2000000.0;
    line->points[1].qp_limit_dbuV = base; line->points[1].avg_limit_dbuV = base;
    line->points[2].frequency_hz = 30000000.0;
    line->points[2].qp_limit_dbuV = base - 10.0; line->points[2].avg_limit_dbuV = base - 10.0;
    line->points[3].frequency_hz = 100000000.0;
    line->points[3].qp_limit_dbuV = base - 18.0; line->points[3].avg_limit_dbuV = base - 18.0;
    line->points[4].frequency_hz = 152000000.0;
    line->points[4].qp_limit_dbuV = base - 18.0; line->points[4].avg_limit_dbuV = base - 18.0;
}

/* CISPR 11 Group 2 Class A (ISM equipment with RF energy) */
/* Group 2 equipment intentionally generates RF energy (e.g., microwave ovens) */
void lisn_cispr11_group2_class_a_limit(lisn_limit_line_t *line) {
    if (!line) return;
    memset(line, 0, sizeof(lisn_limit_line_t));
    line->standard = STD_CISPR_11; line->emission_class = CLASS_A;
    strncpy(line->description, "CISPR 11 Group 2 Class A - ISM RF", 127);
    line->freq_start_hz = 150000.0; line->freq_stop_hz = 30000000.0;
    line->num_points = 3;
    line->points = malloc(3 * sizeof(lisn_limit_point_t));
    line->points_allocated = 1;
    if (!line->points) return;
    /* Group 2 Class A has relaxed limits vs Group 1 */
    line->points[0].frequency_hz = 150000.0;
    line->points[0].qp_limit_dbuV = 100.0; line->points[0].avg_limit_dbuV = 90.0;
    line->points[1].frequency_hz = 500000.0;
    line->points[1].qp_limit_dbuV = 86.0; line->points[1].avg_limit_dbuV = 76.0;
    line->points[2].frequency_hz = 30000000.0;
    line->points[2].qp_limit_dbuV = 73.0; line->points[2].avg_limit_dbuV = 63.0;
}

/* Build a custom limit line from user-specified breakpoints */
int lisn_create_custom_limit(lisn_limit_line_t *line, const double *freqs,
                               const double *qp_limits, const double *avg_limits,
                               int n_points, const char *description) {
    if (!line || !freqs || !qp_limits || n_points <= 0) return -1;
    memset(line, 0, sizeof(lisn_limit_line_t));
    line->standard = STD_CUSTOM_LIMIT;
    line->emission_class = CLASS_A;
    strncpy(line->description, description ? description : "Custom Limit", 127);
    line->num_points = n_points;
    line->points = malloc(n_points * sizeof(lisn_limit_point_t));
    line->points_allocated = 1;
    if (!line->points) return -1;
    line->freq_start_hz = freqs[0];
    line->freq_stop_hz = freqs[n_points - 1];
    int i;
    for (i = 0; i < n_points; i++) {
        line->points[i].frequency_hz = freqs[i];
        line->points[i].qp_limit_dbuV = qp_limits[i];
        line->points[i].avg_limit_dbuV = avg_limits ? avg_limits[i] : qp_limits[i] - 10.0;
    }
    return 0;
}

/* L6: Generate margin histogram for statistical compliance analysis */
void lisn_margin_histogram(const lisn_compliance_report_t *r, int n_bins,
                            double *bin_edges, int *bin_counts) {
    if (!r || !r->points || !bin_edges || !bin_counts || n_bins <= 0) return;
    double min_m = r->points[0].margin_qp_db;
    double max_m = min_m;
    int i, j;
    for (i = 0; i < r->num_points; i++) {
        if (r->points[i].margin_qp_db < min_m) min_m = r->points[i].margin_qp_db;
        if (r->points[i].margin_qp_db > max_m) max_m = r->points[i].margin_qp_db;
    }
    double bin_width = (max_m - min_m) / (double)n_bins;
    if (bin_width <= 0.0) bin_width = 1.0;
    for (i = 0; i < n_bins; i++) {
        bin_edges[i] = min_m + i * bin_width;
        bin_counts[i] = 0;
    }
    for (i = 0; i < r->num_points; i++) {
        j = (int)((r->points[i].margin_qp_db - min_m) / bin_width);
        if (j >= n_bins) j = n_bins - 1;
        if (j < 0) j = 0;
        bin_counts[j]++;
    }
}

/* CISPR 15 (Lighting Equipment) conducted emission limits */
/* Reference: CISPR 15:2018 Table 3a */
void lisn_cispr15_limit(lisn_limit_line_t *line) {
    if (!line) return;
    memset(line, 0, sizeof(lisn_limit_line_t));
    line->standard = STD_CISPR_11;
    line->emission_class = CLASS_B;
    strncpy(line->description, "CISPR 15 - Lighting Equipment", 127);
    line->freq_start_hz = 150000.0;
    line->freq_stop_hz = 30000000.0;
    line->num_points = 3;
    line->points = malloc(3 * sizeof(lisn_limit_point_t));
    line->points_allocated = 1;
    if (!line->points) return;
    line->points[0].frequency_hz = 150000.0;
    line->points[0].qp_limit_dbuV = 66.0; line->points[0].avg_limit_dbuV = 56.0;
    line->points[1].frequency_hz = 500000.0;
    line->points[1].qp_limit_dbuV = 56.0; line->points[1].avg_limit_dbuV = 46.0;
    line->points[2].frequency_hz = 30000000.0;
    line->points[2].qp_limit_dbuV = 60.0; line->points[2].avg_limit_dbuV = 50.0;
}

/* CISPR 16 (Basic EMC Standard - reference limits) */
void lisn_cispr16_limit(lisn_limit_line_t *line) {
    if (!line) return;
    memset(line, 0, sizeof(lisn_limit_line_t));
    line->standard = STD_CISPR_32;
    line->emission_class = CLASS_A;
    strncpy(line->description, "CISPR 16 Reference Limits", 127);
    line->freq_start_hz = 150000.0;
    line->freq_stop_hz = 30000000.0;
    line->num_points = 2;
    line->points = malloc(2 * sizeof(lisn_limit_point_t));
    line->points_allocated = 1;
    if (!line->points) return;
    line->points[0].frequency_hz = 150000.0;
    line->points[0].qp_limit_dbuV = 79.0; line->points[0].avg_limit_dbuV = 66.0;
    line->points[1].frequency_hz = 30000000.0;
    line->points[1].qp_limit_dbuV = 73.0; line->points[1].avg_limit_dbuV = 60.0;
}
