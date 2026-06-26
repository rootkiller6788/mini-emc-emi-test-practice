/**
 * emi_correlation_uncertainty.c — Correlation & Uncertainty Analysis
 *
 * Statistical computations, uncertainty propagation per CISPR 16-4-2/GUM,
 * Monte Carlo uncertainty analysis, and pre-compliance confidence assessment.
 *
 * Knowledge Coverage:
 *   L3: Statistical methods — mean, variance, skewness, kurtosis, regression
 *   L4: GUM law of propagation of uncertainty, Welch-Satterthwaite formula
 *   L5: Uncertainty budget construction, sensitivity analysis
 *   L6: Complete Ucispr calculation per CISPR 16-4-2
 *   L8: Monte Carlo uncertainty propagation (advanced)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "emi_correlation_uncertainty.h"

/* ─── Statistics ──────────────────────────────────────────────────────── */

void emi_compute_statistics(const double *data, int count,
                             emi_statistics_t *stats_out) {
    if (!data || !stats_out || count <= 0) return;
    memset(stats_out, 0, sizeof(*stats_out));
    stats_out->count = count;

    if (count == 1) {
        stats_out->min = stats_out->max = stats_out->mean
            = stats_out->median = stats_out->rms = data[0];
        return;
    }

    /* Compute mean, min, max, rms */
    double sum = 0.0, sum_sq = 0.0;
    double dmin = data[0], dmax = data[0];

    for (int i = 0; i < count; i++) {
        double x = data[i];
        sum += x;
        sum_sq += x * x;
        if (x < dmin) dmin = x;
        if (x > dmax) dmax = x;
    }

    double mean = sum / count;
    stats_out->mean = mean;
    stats_out->min = dmin;
    stats_out->max = dmax;
    stats_out->rms = sqrt(sum_sq / count);

    /* Variance (sample, n-1 for count > 1) */
    double var_sum = 0.0;
    for (int i = 0; i < count; i++) {
        double diff = data[i] - mean;
        var_sum += diff * diff;
    }
    double variance = var_sum / (count - 1);
    stats_out->variance = variance;
    stats_out->std_dev = sqrt(variance);

    /* Skewness and kurtosis */
    double s3 = 0.0, s4 = 0.0;
    for (int i = 0; i < count; i++) {
        double diff = (data[i] - mean) / stats_out->std_dev;
        double d3 = diff * diff * diff;
        double d4 = diff * diff * diff * diff;
        s3 += d3;
        s4 += d4;
    }
    stats_out->skewness = s3 / count;
    stats_out->kurtosis = (s4 / count) - 3.0; /* excess kurtosis */

    /* Median: sort copy of data, take middle */
    double *sorted = (double *)malloc(count * sizeof(double));
    if (sorted) {
        memcpy(sorted, data, count * sizeof(double));
        /* Simple insertion sort for small to medium arrays */
        for (int i = 1; i < count; i++) {
            double key = sorted[i];
            int j = i - 1;
            while (j >= 0 && sorted[j] > key) {
                sorted[j + 1] = sorted[j];
                j--;
            }
            sorted[j + 1] = key;
        }
        if (count % 2 == 1) {
            stats_out->median = sorted[count / 2];
        } else {
            stats_out->median = (sorted[count / 2 - 1] + sorted[count / 2]) / 2.0;
        }
        free(sorted);
    }
}

void emi_linear_regression(const double *x, const double *y, int count,
                            emi_regression_t *reg_out) {
    if (!x || !y || !reg_out || count < 2) return;
    memset(reg_out, 0, sizeof(*reg_out));
    reg_out->num_points = count;

    double sum_x = 0.0, sum_y = 0.0, sum_xy = 0.0, sum_x2 = 0.0, sum_y2 = 0.0;

    for (int i = 0; i < count; i++) {
        sum_x  += x[i];
        sum_y  += y[i];
        sum_xy += x[i] * y[i];
        sum_x2 += x[i] * x[i];
        sum_y2 += y[i] * y[i];
    }

    double n = (double)count;
    double denom = n * sum_x2 - sum_x * sum_x;
    if (fabs(denom) < 1e-15) return;

    /* Slope: a = (N*sum_xy - sum_x*sum_y) / (N*sum_x2 - sum_x^2) */
    double slope = (n * sum_xy - sum_x * sum_y) / denom;
    /* Intercept: b = (sum_y - a*sum_x) / N */
    double intercept = (sum_y - slope * sum_x) / n;

    reg_out->slope = slope;
    reg_out->intercept = intercept;

    /* R^2 and RMSE */
    double y_mean = sum_y / n;
    double ss_res = 0.0, ss_tot = 0.0, sum_abs_err = 0.0;

    for (int i = 0; i < count; i++) {
        double y_pred = slope * x[i] + intercept;
        double res = y[i] - y_pred;
        ss_res += res * res;
        ss_tot += (y[i] - y_mean) * (y[i] - y_mean);
        sum_abs_err += fabs(res);
    }

    reg_out->rmse = (count > 0) ? sqrt(ss_res / count) : 0.0;
    reg_out->mean_absolute_error = (count > 0) ? sum_abs_err / count : 0.0;
    reg_out->r_squared = (ss_tot > 0.0) ? (1.0 - ss_res / ss_tot) : 0.0;
    /* Pearson r = slope * sigma_x / sigma_y; preserve sign of slope */
    double sigma_x = sqrt((n * sum_x2 - sum_x * sum_x) / (n * n));
    double sigma_y = sqrt((n * sum_y2 - sum_y * sum_y) / (n * n));
    if (sigma_x > 0.0 && sigma_y > 0.0) {
        reg_out->pearson_r = slope * sigma_x / sigma_y;
    } else {
        reg_out->pearson_r = 0.0;
    }
}

double emi_pearson_correlation(const double *x, const double *y, int count) {
    if (!x || !y || count < 2) return 0.0;
    emi_regression_t reg;
    emi_linear_regression(x, y, count, &reg);
    return reg.pearson_r;
}

double emi_spearman_correlation(const double *x, const double *y, int count) {
    if (!x || !y || count < 2) return 0.0;

    /* Create index array and rank the data */
    /* For simplicity, use a struct to pair value with index, sort, assign ranks */
    typedef struct { double val; int idx; } pair_t;
    pair_t *px = (pair_t *)malloc(count * sizeof(pair_t));
    pair_t *py = (pair_t *)malloc(count * sizeof(pair_t));
    double *rank_x = (double *)malloc(count * sizeof(double));
    double *rank_y = (double *)malloc(count * sizeof(double));

    if (!px || !py || !rank_x || !rank_y) {
        free(px); free(py); free(rank_x); free(rank_y);
        return 0.0;
    }

    for (int i = 0; i < count; i++) {
        px[i].val = x[i]; px[i].idx = i;
        py[i].val = y[i]; py[i].idx = i;
    }

    /* Sort by value using bubble sort (ok for small datasets) */
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (px[j].val > px[j + 1].val) {
                pair_t tmp = px[j]; px[j] = px[j + 1]; px[j + 1] = tmp;
            }
            if (py[j].val > py[j + 1].val) {
                pair_t tmp = py[j]; py[j] = py[j + 1]; py[j + 1] = tmp;
            }
        }
    }

    /* Assign ranks (handle ties by averaging) */
    for (int i = 0; i < count; ) {
        int j = i;
        while (j < count && px[j].val == px[i].val) j++;
        double avg_rank = (i + j - 1) / 2.0 + 1.0;
        for (int k = i; k < j; k++) rank_x[px[k].idx] = avg_rank;
        i = j;
    }
    for (int i = 0; i < count; ) {
        int j = i;
        while (j < count && py[j].val == py[i].val) j++;
        double avg_rank = (i + j - 1) / 2.0 + 1.0;
        for (int k = i; k < j; k++) rank_y[py[k].idx] = avg_rank;
        i = j;
    }

    double rho = emi_pearson_correlation(rank_x, rank_y, count);

    free(px); free(py); free(rank_x); free(rank_y);
    return rho;
}

/* ─── Uncertainty Propagation ─────────────────────────────────────────── */

double emi_convert_to_standard_uncertainty(double half_width,
                                            const char *distribution) {
    if (!distribution) return half_width;
    if (strcmp(distribution, "rectangular") == 0) {
        return half_width / sqrt(3.0);
    } else if (strcmp(distribution, "triangular") == 0) {
        return half_width / sqrt(6.0);
    } else if (strcmp(distribution, "u-shaped") == 0) {
        return half_width / sqrt(2.0);
    }
    /* "normal": half_width = standard uncertainty already */
    return half_width;
}

double emi_combined_standard_uncertainty(const emi_uncertainty_item_t *items,
                                          int num_items,
                                          const double *corr_matrix) {
    if (!items || num_items <= 0) return 0.0;

    double sum_sq = 0.0;
    /* If correlation matrix is provided, include cross-terms */
    if (corr_matrix) {
        for (int i = 0; i < num_items; i++) {
            double ci_ui = items[i].sensitivity_coeff
                           * items[i].standard_uncertainty_db;
            sum_sq += ci_ui * ci_ui;
            for (int j = i + 1; j < num_items; j++) {
                double cj_uj = items[j].sensitivity_coeff
                               * items[j].standard_uncertainty_db;
                double r_ij = corr_matrix[i * num_items + j];
                sum_sq += 2.0 * r_ij * ci_ui * cj_uj;
            }
        }
    } else {
        /* Uncorrelated: RSS only */
        for (int i = 0; i < num_items; i++) {
            double ci_ui = items[i].sensitivity_coeff
                           * items[i].standard_uncertainty_db;
            sum_sq += ci_ui * ci_ui;
        }
    }

    return sqrt(sum_sq);
}

double emi_expanded_uncertainty(const emi_uncertainty_item_t *items,
                                 int num_items,
                                 const double *corr_matrix,
                                 double coverage_factor) {
    double uc = emi_combined_standard_uncertainty(items, num_items, corr_matrix);
    return coverage_factor * uc;
}

int emi_effective_degrees_of_freedom(const emi_uncertainty_item_t *items,
                                      int num_items,
                                      const double *corr_matrix) {
    if (!items || num_items <= 0) return 30;

    /* Welch-Satterthwaite:
     * nu_eff = u_c^4 / sum_i( (c_i * u_i)^4 / nu_i )
     *
     * For Type B (infinite degrees): nu_i = inf, term → 0
     * For Type A with N measurements: nu_i = N - 1
     */

    double uc = emi_combined_standard_uncertainty(items, num_items, corr_matrix);
    double uc4 = uc * uc * uc * uc;
    if (uc4 <= 0.0) return 30;

    double denominator = 0.0;
    for (int i = 0; i < num_items; i++) {
        double ci_ui = items[i].sensitivity_coeff
                       * items[i].standard_uncertainty_db;
        double ci_ui4 = ci_ui * ci_ui * ci_ui * ci_ui;
        int nu_i = items[i].degrees_of_freedom;
        /* Type B with normal distribution: nu_i ≈ inf, skip */
        if (nu_i > 0 && nu_i < 10000) {
            denominator += ci_ui4 / (double)nu_i;
        }
    }

    if (denominator <= 0.0) return 100000; /* all Type B → inf */
    int nu_eff = (int)(uc4 / denominator);
    if (nu_eff < 1) nu_eff = 1;
    return nu_eff;
}

/* ─── Ucispr Budgets ──────────────────────────────────────────────────── */

void emi_ucispr_conducted_init(emi_uncertainty_budget_t *budget) {
    if (!budget) return;
    memset(budget, 0, sizeof(*budget));
    budget->is_per_conduction = 1;
    budget->coverage_factor = 2.0;
    budget->confidence_level = "95%";

    /* CISPR 16-4-2 Table A.1 standard uncertainties (conducted) */
    int n = 7;
    budget->num_items = n;
    budget->items = (emi_uncertainty_item_t *)calloc(n, sizeof(emi_uncertainty_item_t));
    if (!budget->items) return;

    /* 1. Receiver reading (AMN voltage division factor) */
    budget->items[0].source = "AMN voltage division factor";
    budget->items[0].standard_uncertainty_db = 1.0;
    budget->items[0].sensitivity_coeff = 1.0;
    budget->items[0].type = "Type B";
    budget->items[0].distribution = "normal";
    budget->items[0].degrees_of_freedom = 100000;

    /* 2. AMN impedance */
    budget->items[1].source = "AMN impedance";
    budget->items[1].standard_uncertainty_db = 1.5;
    budget->items[1].sensitivity_coeff = 1.0;
    budget->items[1].type = "Type B";
    budget->items[1].distribution = "rectangular";
    budget->items[1].degrees_of_freedom = 100000;

    /* 3. Cable loss (AMN to receiver) */
    budget->items[2].source = "Cable loss";
    budget->items[2].standard_uncertainty_db = 0.3;
    budget->items[2].sensitivity_coeff = 1.0;
    budget->items[2].type = "Type B";
    budget->items[2].distribution = "normal";
    budget->items[2].degrees_of_freedom = 100000;

    /* 4. Receiver calibration */
    budget->items[3].source = "Receiver calibration";
    budget->items[3].standard_uncertainty_db = 0.5;
    budget->items[3].sensitivity_coeff = 1.0;
    budget->items[3].type = "Type B";
    budget->items[3].distribution = "normal";
    budget->items[3].degrees_of_freedom = 100000;

    /* 5. Mismatch */
    budget->items[4].source = "Mismatch AMN-receiver";
    budget->items[4].standard_uncertainty_db = 0.7;
    budget->items[4].sensitivity_coeff = 1.0;
    budget->items[4].type = "Type B";
    budget->items[4].distribution = "u-shaped";
    budget->items[4].degrees_of_freedom = 100000;

    /* 6. Ambient effects */
    budget->items[5].source = "Ambient effects";
    budget->items[5].standard_uncertainty_db = 0.5;
    budget->items[5].sensitivity_coeff = 1.0;
    budget->items[5].type = "Type B";
    budget->items[5].distribution = "rectangular";
    budget->items[5].degrees_of_freedom = 100000;

    /* 7. Repeatability (Type A — to be filled by user) */
    budget->items[6].source = "Measurement repeatability";
    budget->items[6].standard_uncertainty_db = 0.5;
    budget->items[6].sensitivity_coeff = 1.0;
    budget->items[6].type = "Type A";
    budget->items[6].distribution = "normal";
    budget->items[6].degrees_of_freedom = 9; /* n=10 measurements */
}

void emi_ucispr_radiated_init(emi_uncertainty_budget_t *budget) {
    if (!budget) return;
    memset(budget, 0, sizeof(*budget));
    budget->is_per_conduction = 0;
    budget->coverage_factor = 2.0;
    budget->confidence_level = "95%";

    /* CISPR 16-4-2 Table A.3 standard uncertainties (radiated) */
    int n = 9;
    budget->num_items = n;
    budget->items = (emi_uncertainty_item_t *)calloc(n, sizeof(emi_uncertainty_item_t));
    if (!budget->items) return;

    budget->items[0].source = "Receiver reading";
    budget->items[0].standard_uncertainty_db = 1.0;
    budget->items[0].sensitivity_coeff = 1.0;
    budget->items[0].type = "Type B";
    budget->items[0].distribution = "normal";
    budget->items[0].degrees_of_freedom = 100000;

    budget->items[1].source = "Antenna factor calibration";
    budget->items[1].standard_uncertainty_db = 1.5;
    budget->items[1].sensitivity_coeff = 1.0;
    budget->items[1].type = "Type B";
    budget->items[1].distribution = "normal";
    budget->items[1].degrees_of_freedom = 100000;

    budget->items[2].source = "Cable loss";
    budget->items[2].standard_uncertainty_db = 0.5;
    budget->items[2].sensitivity_coeff = 1.0;
    budget->items[2].type = "Type B";
    budget->items[2].distribution = "normal";
    budget->items[2].degrees_of_freedom = 100000;

    budget->items[3].source = "Receiver calibration";
    budget->items[3].standard_uncertainty_db = 0.5;
    budget->items[3].sensitivity_coeff = 1.0;
    budget->items[3].type = "Type B";
    budget->items[3].distribution = "normal";
    budget->items[3].degrees_of_freedom = 100000;

    budget->items[4].source = "Mismatch antenna-receiver";
    budget->items[4].standard_uncertainty_db = 1.0;
    budget->items[4].sensitivity_coeff = 1.0;
    budget->items[4].type = "Type B";
    budget->items[4].distribution = "u-shaped";
    budget->items[4].degrees_of_freedom = 100000;

    budget->items[5].source = "Site imperfection (NSA)";
    budget->items[5].standard_uncertainty_db = 3.0;
    budget->items[5].sensitivity_coeff = 1.0;
    budget->items[5].type = "Type B";
    budget->items[5].distribution = "rectangular";
    budget->items[5].degrees_of_freedom = 100000;

    budget->items[6].source = "Antenna positioning";
    budget->items[6].standard_uncertainty_db = 0.5;
    budget->items[6].sensitivity_coeff = 1.0;
    budget->items[6].type = "Type B";
    budget->items[6].distribution = "rectangular";
    budget->items[6].degrees_of_freedom = 100000;

    budget->items[7].source = "Ambient effects";
    budget->items[7].standard_uncertainty_db = 1.0;
    budget->items[7].sensitivity_coeff = 1.0;
    budget->items[7].type = "Type B";
    budget->items[7].distribution = "rectangular";
    budget->items[7].degrees_of_freedom = 100000;

    budget->items[8].source = "Measurement repeatability";
    budget->items[8].standard_uncertainty_db = 0.5;
    budget->items[8].sensitivity_coeff = 1.0;
    budget->items[8].type = "Type A";
    budget->items[8].distribution = "normal";
    budget->items[8].degrees_of_freedom = 9;
}

void emi_ucispr_finalize(emi_uncertainty_budget_t *budget) {
    if (!budget || !budget->items || budget->num_items <= 0) return;

    /* Compute individual contribution magnitudes */
    for (int i = 0; i < budget->num_items; i++) {
        budget->items[i].ci_ui_db = budget->items[i].sensitivity_coeff
                                     * budget->items[i].standard_uncertainty_db;
    }

    /* Combined standard uncertainty */
    budget->combined_standard_uncertainty_dB
        = emi_combined_standard_uncertainty(budget->items,
                                             budget->num_items, NULL);

    /* Effective degrees of freedom */
    budget->effective_degrees_of_freedom
        = emi_effective_degrees_of_freedom(budget->items,
                                            budget->num_items, NULL);

    /* Expanded uncertainty */
    budget->expanded_uncertainty_dB = budget->coverage_factor
                                       * budget->combined_standard_uncertainty_dB;

    /* Ucispr: CISPR 16-4-2 uses k=2 (95% confidence) */
    budget->ucispr_dB = budget->expanded_uncertainty_dB;
}

emi_margin_status_t emi_ucispr_decision(double measured_dbuv, double limit_dbuv,
                                         double expanded_uncertainty_db) {
    double pass_boundary = measured_dbuv + expanded_uncertainty_db;
    double fail_boundary = measured_dbuv - expanded_uncertainty_db;

    if (pass_boundary <= limit_dbuv) {
        return EMI_MARGIN_PASS;  /* compliant with confidence */
    } else if (fail_boundary > limit_dbuv) {
        return EMI_MARGIN_FAIL;  /* non-compliant */
    } else {
        return EMI_MARGIN_MARGINAL; /* indeterminate */
    }
}

/* ─── Monte Carlo Uncertainty (L8 Advanced) ──────────────────────────────── */

void emi_monte_carlo_uncertainty(const double *input_means,
                                  const double *input_stdu,
                                  int n_inputs,
                                  int n_iterations,
                                  double (*model_func)(const double *, int),
                                  emi_statistics_t *output_stats) {
    if (!input_means || !input_stdu || !model_func || !output_stats
        || n_inputs <= 0 || n_iterations <= 0) return;

    double *results = (double *)malloc(n_iterations * sizeof(double));
    if (!results) return;

    /* Box-Muller transform for normal random numbers.
     * Seed with fixed value for reproducibility;
     * in production, use a true random seed. */
    unsigned seed = 12345u;

    for (int iter = 0; iter < n_iterations; iter++) {
        double *inputs = (double *)malloc(n_inputs * sizeof(double));
        if (!inputs) continue;

        for (int i = 0; i < n_inputs; i++) {
            /* Generate two independent N(0,1) samples via Box-Muller */
            double u1 = (double)((seed = seed * 1103515245u + 12345u) & 0x7FFFFFFF)
                        / 2147483648.0;
            double u2 = (double)((seed = seed * 1103515245u + 12345u) & 0x7FFFFFFF)
                        / 2147483648.0;
            /* Use first sample for this input, discard second */
            double r = sqrt(-2.0 * log(u1 + 1e-15));
            double theta = 2.0 * EMI_PI * u2;
            double z = r * cos(theta); /* N(0,1) */

            /* Scale to input distribution: N(mean, stdu) */
            inputs[i] = input_means[i] + z * input_stdu[i];
        }

        results[iter] = model_func(inputs, n_inputs);
        free(inputs);
    }

    emi_compute_statistics(results, n_iterations, output_stats);
    free(results);
}

void emi_mc_coverage_interval(const double *simulated_results, int n,
                               double *y_low, double *y_high) {
    if (!simulated_results || !y_low || !y_high || n <= 0) return;

    /* Sort the results */
    double *sorted = (double *)malloc(n * sizeof(double));
    if (!sorted) return;
    memcpy(sorted, simulated_results, n * sizeof(double));

    for (int i = 1; i < n; i++) {
        double key = sorted[i];
        int j = i - 1;
        while (j >= 0 && sorted[j] > key) {
            sorted[j + 1] = sorted[j];
            j--;
        }
        sorted[j + 1] = key;
    }

    /* 2.5th and 97.5th percentiles for 95% coverage */
    int idx_low = (int)(0.025 * n);
    int idx_high = (int)(0.975 * n);
    if (idx_low < 0) idx_low = 0;
    if (idx_high >= n) idx_high = n - 1;

    *y_low = sorted[idx_low];
    *y_high = sorted[idx_high];
    free(sorted);
}

/* ─── Pre-Compliance Confidence ────────────────────────────────────────── */

/**
 * Standard normal CDF approximation (Abramowitz & Stegun 26.2.17).
 * Phi(z) ≈ 1 - 0.5 * (1 + c1*z + c2*z^2 + c3*z^3 + c4*z^4)^(-4)
 * Maximum error < 2.5e-4.
 */
static double std_normal_cdf(double z) {
    if (z < -8.0) return 0.0;
    if (z > 8.0) return 1.0;

    double c1 = 0.196854;
    double c2 = 0.115194;
    double c3 = 0.000344;
    double c4 = 0.019527;

    double t = 1.0 / (1.0 + 0.2316419 * fabs(z));
    double phi = 1.0 - (1.0 / sqrt(2.0 * EMI_PI))
                 * exp(-z * z / 2.0)
                 * (c1 * t + c2 * t * t + c3 * t * t * t + c4 * t * t * t * t);

    return (z >= 0.0) ? phi : (1.0 - phi);
}

double emi_compliance_pass_probability(double e_pre_dbuvm, double limit_dbuvm,
                                        double mean_offset_db,
                                        double std_dev_db) {
    if (std_dev_db <= 0.0) {
        /* Zero uncertainty: deterministic */
        return (e_pre_dbuvm + mean_offset_db <= limit_dbuvm) ? 1.0 : 0.0;
    }

    /* Z-score: how many standard deviations is the limit above/below
     * the expected full compliance value.
     * E_full_expected = E_pre + mean_offset
     * Z = (Limit - E_full_expected) / sigma
     * P(pass) = Phi(Z) */
    double expected_full = e_pre_dbuvm + mean_offset_db;
    double z = (limit_dbuvm - expected_full) / std_dev_db;
    return std_normal_cdf(z);
}

double emi_precompliance_confidence(const emi_correlation_result_t *corr,
                                     double limit_dbuvm,
                                     double pre_limit_margin_db) {
    if (!corr || corr->num_comparable_points < 2) return 0.0;

    double offset = corr->mean_offset_db;
    double sigma = corr->std_deviation_db;
    if (sigma <= 0.0) return 1.0;

    /* Z = (limit - (pre_limit + offset)) / sigma
     *   = (limit - (limit - margin + offset)) / sigma
     *   = (margin - offset) / sigma
     * where pre_limit = limit_dbuvm - pre_limit_margin_db */
    (void)limit_dbuvm; /* incorporated via pre_limit_margin_db */

    /* Compute confidence as the probability that if we pass at the
     * pre-compliance limit, we also pass at the full compliance limit.
     * P(E_full <= limit | E_pre <= pre_limit)
     *
     * Simplified: Z = (limit - (pre_limit + offset)) / sigma
     *            = (limit - limit + margin - offset) / sigma
     *            = (margin - offset) / sigma
     */
    double z = (pre_limit_margin_db - offset) / sigma;
    return std_normal_cdf(z);
}
