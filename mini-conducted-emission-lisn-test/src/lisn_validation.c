/**
 * @file lisn_validation.c
 * @brief LISN measurement validation, uncertainty budget, and inter-lab comparison.
 * Ref: CISPR 16-1-2, ANSI C63.4, ISO 17025.
 */
#include "lisn_core.h"
#include "lisn_impedance.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* L6: Measurement Uncertainty Budget per CISPR 16-4-2 */
typedef struct { double value; double std_uncertainty; const char *source; } LISN_UncertaintyComponent;

int lisn_uncertainty_budget(const LISN_UncertaintyComponent *components, int n, double *expanded_uncertainty)
{
    if (!components || n <= 0 || !expanded_uncertainty) return -1;
    double u2 = 0.0;
    for (int i = 0; i < n; i++) {
        double u = components[i].std_uncertainty;
        u2 += u * u;
    }
    *expanded_uncertainty = 2.0 * sqrt(u2); /* k=2, 95% confidence */
    return 0;
}

/* L6: LISN impedance verification sweep */
int lisn_verify_impedance_sweep(const double *frequencies, int n, const double *Z_measured,
                                 const double *Z_expected, const double *tolerance_pct,
                                 int *pass_fail)
{
    if (!frequencies || n <= 0 || !Z_measured || !Z_expected || !tolerance_pct || !pass_fail)
        return -1;
    for (int i = 0; i < n; i++) {
        double z_meas = Z_measured[i], z_exp = Z_expected[i];
        if (z_exp <= 0.0) { pass_fail[i] = 0; continue; }
        double deviation = fabs(z_meas - z_exp) / z_exp * 100.0;
        pass_fail[i] = (deviation <= tolerance_pct[i]) ? 1 : 0;
    }
    return 0;
}

/* L7: Inter-lab comparison Z-score */
int lisn_interlab_zscore(const double *lab1_values, const double *lab2_values, int n,
                          double sigma_target, double *zscores)
{
    if (!lab1_values || !lab2_values || n <= 0 || sigma_target <= 0.0 || !zscores)
        return -1;
    for (int i = 0; i < n; i++) {
        zscores[i] = (lab1_values[i] - lab2_values[i]) / sigma_target;
    }
    return 0;
}

/* L7: Margin to limit */
double lisn_margin_to_limit(double measured_dBuV, double limit_dBuV)
{
    return limit_dBuV - measured_dBuV;
}

int lisn_is_compliant(const double *emissions, int n, const double *limits, int *compliant)
{
    if (!emissions || n <= 0 || !limits || !compliant) return -1;
    for (int i = 0; i < n; i++)
        compliant[i] = (emissions[i] <= limits[i]) ? 1 : 0;
    return 0;
}

/* L8: Time-domain LISN (TDR-based) for transient EMI */
int lisn_tdr_impedance_profile(const double *tdr_waveform, int n, double dt,
                                double *impedance_profile)
{
    if (!tdr_waveform || n <= 1 || dt <= 0.0 || !impedance_profile) return -1;
    double Z0 = 50.0;
    for (int i = 0; i < n; i++) {
        double rho = tdr_waveform[i];
        if (fabs(1.0 - rho) < 1e-15) impedance_profile[i] = Z0;
        else impedance_profile[i] = Z0 * (1.0 + rho) / (1.0 - rho);
    }
    return 0;
}