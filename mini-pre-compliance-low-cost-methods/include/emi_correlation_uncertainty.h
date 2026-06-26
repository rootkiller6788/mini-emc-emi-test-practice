/**
 * emi_correlation_uncertainty.h - Correlation Analysis & Measurement Uncertainty
 *
 * Statistical methods for correlating low-cost pre-compliance measurements
 * to full compliance results, and computing measurement uncertainty budgets
 * per CISPR 16-4-2 / ISO/IEC Guide 98-3 (GUM).
 *
 * References:
 *   - CISPR 16-4-2: Uncertainty in EMC measurements
 *   - ISO/IEC Guide 98-3: Guide to the Expression of Uncertainty in Measurement (GUM)
 *   - JCGM 100:2008, "Evaluation of measurement data"
 *   - NIST Technical Note 1297, "Guidelines for Evaluating Uncertainty"
 *   - Williams, T., "EMC for Product Designers", 5th ed., Chapter 17.
 *
 * Knowledge Coverage:
 *   L1: Uncertainty types (Type A/B), confidence interval, coverage factor
 *   L2: Measurement uncertainty concept for EMC
 *   L3: Statistical methods — mean, std dev, RMS, correlation coefficient
 *   L4: Error propagation law (GUM), central limit theorem application
 *   L5: Uncertainty budget construction, sensitivity analysis
 *   L6: Complete Ucispr calculation per CISPR 16-4-2
 *   L8: Monte Carlo uncertainty propagation
 */

#ifndef EMI_CORRELATION_UNCERTAINTY_H
#define EMI_CORRELATION_UNCERTAINTY_H

#include "emi_precompliance_core.h"
#include "emi_lowcost_methods.h"
#include <stdint.h>

/* ─── Statistical Types (L3 Math Structures) ──────────────────────────── */

/** Descriptive statistics for a dataset. */
typedef struct {
    int     count;
    double  min;
    double  max;
    double  mean;
    double  median;
    double  std_dev;
    double  rms;
    double  variance;
    double  skewness;
    double  kurtosis;
} emi_statistics_t;

/** Linear regression result. */
typedef struct {
    double  slope;
    double  intercept;
    double  r_squared;               /** Coefficient of determination R^2 */
    double  pearson_r;               /** Pearson correlation coefficient */
    double  rmse;                    /** Root mean square error */
    double  mean_absolute_error;
    int     num_points;
} emi_regression_t;

/** Measurement uncertainty budget entry (detailed). */
typedef struct {
    const char *source;              /** Uncertainty source description */
    double standard_uncertainty_db;  /** Standard uncertainty u(x_i), dB */
    double sensitivity_coeff;        /** Sensitivity coefficient c_i */
    double ci_ui_db;                 /** |c_i * u(x_i)| contribution, dB */
    const char *type;                /** "Type A" or "Type B" */
    const char *distribution;        /** "normal", "rectangular", "u-shaped", "triangular" */
    double divisor;                  /** Divisor for conversion to standard uncertainty */
    int    degrees_of_freedom;       /** Effective degrees of freedom */
} emi_uncertainty_item_t;

/** Complete uncertainty budget per CISPR 16-4-2. */
typedef struct {
    emi_uncertainty_item_t *items;
    int    num_items;
    double combined_standard_uncertainty_dB;  /** u_c(y), dB */
    double expanded_uncertainty_dB;            /** U = k * u_c(y), dB */
    double coverage_factor;                    /** k (typically 2 for 95%) */
    int    effective_degrees_of_freedom;       /** nu_eff (Welch-Satterthwaite) */
    const char *confidence_level;              /** "95%" or "99%" */
    int    is_per_conduction;                 /** 1=conducted, 0=radiated */
    double ucispr_dB;                          /** Ucispr per CISPR 16-4-2, dB */
} emi_uncertainty_budget_t;

/* ─── Statistical Functions (L3 Math Structures) ──────────────────────── */

/**
 * Compute descriptive statistics for an array of values.
 *
 * Mean:       x_bar = (1/N) * sum(x_i)
 * Std Dev:    s = sqrt( (1/(N-1)) * sum((x_i - x_bar)^2) )
 * RMS:        sqrt( (1/N) * sum(x_i^2) )
 * Variance:   s^2
 * Skewness:   (1/N) * sum((x_i - x_bar)^3) / s^3
 * Kurtosis:   (1/N) * sum((x_i - x_bar)^4) / s^4  (excess)
 *
 * @param data       Input data array
 * @param count      Number of elements
 * @param stats_out  Output statistics (cannot be NULL)
 */
void emi_compute_statistics(const double *data, int count,
                             emi_statistics_t *stats_out);

/**
 * Compute linear regression y = a*x + b.
 * Least-squares fit: minimize sum((y_i - a*x_i - b)^2)
 *
 * a = (N*sum(xy) - sum(x)*sum(y)) / (N*sum(x^2) - sum(x)^2)
 * b = (sum(y) - a*sum(x)) / N
 *
 * R^2 = 1 - SS_res / SS_tot
 * where SS_res = sum((y_i - y_pred_i)^2)
 *       SS_tot = sum((y_i - y_mean)^2)
 *
 * @param x       Independent variable (e.g., pre-compliance)
 * @param y       Dependent variable (e.g., full compliance)
 * @param count   Number of data pairs
 * @param reg_out Output regression result
 */
void emi_linear_regression(const double *x, const double *y, int count,
                            emi_regression_t *reg_out);

/**
 * Compute Pearson correlation coefficient.
 * r = sum((x_i - x_bar)*(y_i - y_bar))
 *     / sqrt(sum((x_i - x_bar)^2) * sum((y_i - y_bar)^2))
 *
 * r = 1: perfect positive linear correlation
 * r = 0: no linear correlation
 * r = -1: perfect negative linear correlation
 */
double emi_pearson_correlation(const double *x, const double *y, int count);

/**
 * Compute Spearman rank correlation coefficient.
 * Non-parametric correlation measure; assesses monotonic relationships.
 * Rank the data, compute Pearson correlation on ranks.
 */
double emi_spearman_correlation(const double *x, const double *y, int count);

/* ─── Uncertainty Propagation (L4 Fundamental Laws) ──────────────────── */

/**
 * Compute combined standard uncertainty using GUM law of propagation.
 *
 * u_c^2(y) = sum_i (c_i * u(x_i))^2  +  2 * sum_i sum_j>i c_i*c_j*u(x_i)*u(x_j)*r_ij
 *
 * For independent (uncorrelated) inputs (r_ij = 0):
 * u_c^2(y) = sum_i (c_i * u(x_i))^2  =  sum_i (u_i_contrib)^2
 *
 * This is the root-sum-square (RSS) combination of uncertainty contributions.
 *
 * @param items         Array of uncertainty items
 * @param num_items     Number of items
 * @param corr_matrix   Correlation matrix (num_items x num_items, NULL = uncorrelated)
 * @return Combined standard uncertainty u_c(y)
 */
double emi_combined_standard_uncertainty(const emi_uncertainty_item_t *items,
                                          int num_items,
                                          const double *corr_matrix);

/**
 * Compute expanded uncertainty U = k * u_c(y).
 *
 * k = 1 → ~68% confidence (1 sigma)
 * k = 2 → ~95% confidence (coverage factor for normal distribution)
 * k = 3 → ~99.7% confidence
 *
 * Ref: GUM Section 6.
 */
double emi_expanded_uncertainty(const emi_uncertainty_item_t *items,
                                 int num_items,
                                 const double *corr_matrix,
                                 double coverage_factor);

/**
 * Compute effective degrees of freedom (Welch-Satterthwaite formula).
 *
 * nu_eff = u_c^4 / sum_i( (c_i * u_i)^4 / nu_i )
 *
 * Required to determine the appropriate coverage factor k
 * from the Student's t-distribution when nu_eff < 30.
 *
 * Ref: GUM Annex G.4.
 */
int emi_effective_degrees_of_freedom(const emi_uncertainty_item_t *items,
                                      int num_items,
                                      const double *corr_matrix);

/**
 * Convert a standard uncertainty from a non-normal distribution.
 * Rectangular (uniform): u = a / sqrt(3)
 * Triangular:           u = a / sqrt(6)
 * U-shaped (arcsine):   u = a / sqrt(2)
 *
 * where a = half-width of the distribution.
 */
double emi_convert_to_standard_uncertainty(double half_width,
                                            const char *distribution);

/* ─── Ucispr Calculation (L6 Canonical Problem) ──────────────────────── */

/**
 * Standard CISPR 16-4-2 uncertainty contributions for conducted emissions.
 *
 * This function populates a pre-defined uncertainty budget with the
 * standard contributions listed in CISPR 16-4-2, Table A.1:
 *   - Receiver reading (AMN voltage division factor)
 *   - AMN impedance
 *   - Cable loss (AMN to receiver)
 *   - Receiver calibration
 *   - Mismatch (AMN-receiver)
 *   - Ambient effects
 *   - Repeatability (Type A)
 *
 * The user should then fill in the actual measured values
 * (the items come pre-populated with standard values).
 */
void emi_ucispr_conducted_init(emi_uncertainty_budget_t *budget);

/**
 * Standard CISPR 16-4-2 uncertainty contributions for radiated emissions.
 *
 * Table A.3 contributions:
 *   - Receiver reading
 *   - Antenna factor calibration
 *   - Cable loss
 *   - Receiver calibration
 *   - Mismatch (antenna-receiver)
 *   - Site imperfection (NSA deviation)
 *   - Antenna positioning
 *   - Ambient effects
 *   - Repeatability (Type A)
 */
void emi_ucispr_radiated_init(emi_uncertainty_budget_t *budget);

/**
 * Finalize the uncertainty budget by computing u_c, U, nu_eff.
 * Must be called after all uncertainty items are populated.
 */
void emi_ucispr_finalize(emi_uncertainty_budget_t *budget);

/**
 * Check if a measurement result PASSES or FAILS considering uncertainty.
 *
 * Per CISPR 16-4-2 decision rules:
 *   Pass:   E_meas + U <= Limit  → compliant with 95% confidence
 *   Fail:   E_meas - U >  Limit  → non-compliant
 *   Indeterminate: otherwise
 *
 * @param measured_dbuv     Measured emission level
 * @param limit_dbuv        Applicable limit
 * @param expanded_uncertainty_db   Expanded uncertainty U
 * @return EMI_MARGIN_PASS if (E_meas + U) <= Limit,
 *         EMI_MARGIN_FAIL if (E_meas - U) > Limit,
 *         EMI_MARGIN_MARGINAL otherwise
 */
emi_margin_status_t emi_ucispr_decision(double measured_dbuv, double limit_dbuv,
                                         double expanded_uncertainty_db);

/* ─── Monte Carlo Uncertainty Propagation (L8 Advanced) ────────────────── */

/**
 * Monte Carlo simulation for measurement uncertainty.
 *
 * For each iteration:
 *   1. Draw random samples from each input distribution
 *   2. Compute the measurement result using the model function
 *   3. Store the result
 *
 * After N iterations, compute statistics of the output distribution.
 * This avoids assumptions of normality and linearity in the GUM method.
 *
 * Ref: JCGM 101:2008, "Propagation of distributions using a Monte Carlo method".
 *
 * @param input_means     Mean values of input quantities
 * @param input_stdu      Standard uncertainties of inputs
 * @param n_inputs        Number of input quantities
 * @param n_iterations    Number of Monte Carlo iterations (recommend >= 100000)
 * @param model_func      Function f(x0, ..., xn-1) = output
 * @param output_stats    Output statistics of the MC simulation
 */
void emi_monte_carlo_uncertainty(const double *input_means,
                                  const double *input_stdu,
                                  int n_inputs,
                                  int n_iterations,
                                  double (*model_func)(const double *, int),
                                  emi_statistics_t *output_stats);

/**
 * Compute the 95% coverage interval from Monte Carlo results.
 * The interval [y_low, y_high] contains 95% of the simulated values,
 * using the 2.5th and 97.5th percentiles.
 *
 * This is the probabilistic equivalent of the k=2 expanded uncertainty.
 */
void emi_mc_coverage_interval(const double *simulated_results, int n,
                               double *y_low, double *y_high);

/* ─── Pre-Compliance Confidence Assessment (L5 Algorithms) ────────────── */

/**
 * Compute the probability that a pre-compliance measurement
 * correctly predicts full compliance outcome.
 *
 * For each emission:
 *   1. Compute Z-score: Z = (Limit - E_pre - offset) / sigma
 *   2. P(pass) = Phi(Z) where Phi is the standard normal CDF
 *
 * This assumes normally-distributed errors between pre-compliance
 * and full compliance. The offset and sigma are obtained from
 * correlation analysis.
 *
 * @param e_pre_dbuvm        Pre-compliance field strength, dBuV/m
 * @param limit_dbuvm        Full compliance limit, dBuV/m
 * @param mean_offset_db     Mean (E_full - E_pre) offset, dB
 * @param std_dev_db         Standard deviation of offset, dB
 * @return Probability of passing full compliance (0.0 to 1.0)
 */
double emi_compliance_pass_probability(double e_pre_dbuvm, double limit_dbuvm,
                                        double mean_offset_db,
                                        double std_dev_db);

/**
 * Compute the pre-compliance measurement confidence level.
 *
 * Confidence = 1 - alpha, where alpha = probability of false pass
 *   + probability of false fail.
 *
 * False pass: E_pre <= limit_pre but E_full > limit_full
 * False fail: E_pre > limit_pre but E_full <= limit_full
 *
 * A good pre-compliance system has confidence > 90%.
 */
double emi_precompliance_confidence(const emi_correlation_result_t *corr,
                                     double limit_dbuvm,
                                     double pre_limit_margin_db);

#endif /* EMI_CORRELATION_UNCERTAINTY_H */
