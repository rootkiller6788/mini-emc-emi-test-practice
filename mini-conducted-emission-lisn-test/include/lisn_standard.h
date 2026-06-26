#ifndef LISN_STANDARD_H
#define LISN_STANDARD_H
#include "lisn_core.h"
#include "lisn_measurement.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    STD_CISPR_11, STD_CISPR_14, STD_CISPR_22, STD_CISPR_25,
    STD_CISPR_32, STD_FCC_PART_15, STD_MIL_STD_461, STD_DO_160,
    STD_CUSTOM_LIMIT
} lisn_standard_id_t;

typedef enum {
    CLASS_A, CLASS_B, CLASS_1, CLASS_2, CLASS_3, CLASS_4, CLASS_5
} lisn_emission_class_t;

typedef struct {
    double frequency_hz;
    double qp_limit_dbuV;
    double avg_limit_dbuV;
} lisn_limit_point_t;

typedef struct {
    lisn_standard_id_t  standard;
    lisn_emission_class_t emission_class;
    char    description[128];
    double  freq_start_hz;
    double  freq_stop_hz;
    int     num_points;
    lisn_limit_point_t *points;
    int     points_allocated;
} lisn_limit_line_t;

typedef struct {
    double  frequency_hz;
    double  measured_qp_dbuV;
    double  limit_qp_dbuV;
    double  margin_qp_db;
    double  measured_avg_dbuV;
    double  limit_avg_dbuV;
    double  margin_avg_db;
    int     qp_pass;
    int     avg_pass;
} lisn_compliance_point_t;

typedef struct {
    lisn_limit_line_t   limit_line;
    int                 num_points;
    lisn_compliance_point_t *points;
    int                 overall_pass;
    double              worst_margin_db;
    double              worst_frequency_hz;
    char                verdict[256];
} lisn_compliance_report_t;

void lisn_cispr22_class_a_limit(lisn_limit_line_t *line);
void lisn_cispr22_class_b_limit(lisn_limit_line_t *line);
void lisn_cispr25_class5_limit(lisn_limit_line_t *line);
void lisn_cispr32_class_a_limit(lisn_limit_line_t *line);
void lisn_cispr32_class_b_limit(lisn_limit_line_t *line);
void lisn_fcc_part15_class_b_limit(lisn_limit_line_t *line);
void lisn_mil461_ce102_limit(lisn_limit_line_t *line, double nominal_voltage_v);
void lisn_cispr11_group1_class_a_limit(lisn_limit_line_t *line);
void lisn_cispr14_limit(lisn_limit_line_t *line);
int lisn_interpolate_limit(const lisn_limit_line_t *line, double f_hz, double *qp_limit, double *avg_limit);
int lisn_check_point(const lisn_limit_line_t *line, double frequency_hz, double measured_qp_dbuV, double measured_avg_dbuV, lisn_compliance_point_t *result);
int lisn_compliance_check(const lisn_measurement_dataset_t *dataset, const lisn_limit_line_t *line, lisn_compliance_report_t *report);
void lisn_find_worst_margin(const lisn_compliance_report_t *report, double *worst_qp_freq, double *worst_qp_margin, double *worst_avg_freq, double *worst_avg_margin);
void lisn_limit_line_free(lisn_limit_line_t *line);
void lisn_compliance_report_free(lisn_compliance_report_t *report);
void lisn_print_compliance_report(const lisn_compliance_report_t *report);

#ifdef __cplusplus
}
#endif

/* L7: Product-specific EMC standards */
typedef enum {
    PRODUCT_IT_EQUIPMENT,
    PRODUCT_MEDICAL_DEVICE,
    PRODUCT_AUTOMOTIVE_COMPONENT,
    PRODUCT_HOUSEHOLD_APPLIANCE,
    PRODUCT_LIGHTING_EQUIPMENT,
    PRODUCT_INDUSTRIAL_MACHINE,
    PRODUCT_AEROSPACE_EQUIPMENT,
    PRODUCT_MILITARY_EQUIPMENT,
    PRODUCT_TELECOM_EQUIPMENT,
    PRODUCT_RAILWAY_EQUIPMENT
} lisn_product_category_t;

/* L7: Get applicable standard for a product category */
void lisn_get_applicable_standard(lisn_product_category_t cat, lisn_standard_id_t *std, lisn_emission_class_t *cls);

/* Additional compliance functions */
int lisn_margin_at_frequency(const lisn_compliance_report_t *r, double f, double *qp_m, double *avg_m);
void lisn_compliance_summary_json(const lisn_compliance_report_t *r, char *buf, int sz);
double lisn_aggregate_margin(const lisn_compliance_report_t *r);
int lisn_compare_limits(const lisn_limit_line_t *a, const lisn_limit_line_t *b, double f, double *delta_qp, double *delta_avg);


void lisn_do160_section21_limit(lisn_limit_line_t *line, int category);
void lisn_cispr11_group2_class_a_limit(lisn_limit_line_t *line);
int lisn_create_custom_limit(lisn_limit_line_t *line, const double *freqs, const double *qp_limits, const double *avg_limits, int n_points, const char *description);
void lisn_margin_histogram(const lisn_compliance_report_t *r, int n_bins, double *bin_edges, int *bin_counts);
void lisn_cispr15_limit(lisn_limit_line_t *line);
void lisn_cispr16_limit(lisn_limit_line_t *line);

#endif
