/** @file lisn_noise_separator.c - CM/DM Noise Separation Algorithms */
/** Paul (2006) Ch.3; Ott (2009) Ch.11; Guo-Chen IEEE TPE 1996 */
/** L1(defs), L2(CM vs DM), L5(separation algorithms), L7(filter design) */

#include "lisn_noise_separator.h"
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <stdio.h>

/* L5: Mathematical CM/DM decomposition from L-N-G measurements */
/* V_cm = (V_L + V_N) / 2,  V_dm = (V_L - V_N) / 2 */
/* Reference: Paul (2006) Eq. 3.18-3.20 */
void lisn_separate_math_decomp(double vL, double vN, double freq,
                                lisn_cm_dm_result_t *r) {
    if (!r) return;
    r->frequency_hz = freq;
    double vL_lin = lisn_dbuV_to_voltage(vL);
    double vN_lin = lisn_dbuV_to_voltage(vN);
    double vcm_lin = (vL_lin + vN_lin) / 2.0;
    double vdm_lin = (vL_lin - vN_lin) / 2.0;
    r->cm_voltage_dbuV = lisn_voltage_to_dbuV(vcm_lin);
    r->dm_voltage_dbuV = lisn_voltage_to_dbuV(vdm_lin);
    double vcm_sq = vcm_lin * vcm_lin;
    double vdm_sq = vdm_lin * vdm_lin;
    double total_lin = sqrt(vcm_sq + vdm_sq);
    r->total_voltage_dbuV = lisn_voltage_to_dbuV(total_lin);
    double total_sq = vcm_sq + vdm_sq;
    if (total_sq > 0.0) {
        r->cm_ratio_pct = vcm_sq / total_sq * 100.0;
        r->dm_ratio_pct = vdm_sq / total_sq * 100.0;
    } else { r->cm_ratio_pct = 50.0; r->dm_ratio_pct = 50.0; }
}

/* L1: Convert dBuV to linear voltage */
/* V = 10^(dBuV/20) * 1e-6 */
double lisn_dbuV_to_voltage(double dbuV) {
    return pow(10.0, dbuV / 20.0) * 1.0e-6;
}

/* L1: Convert linear voltage to dBuV */
/* dBuV = 20 * log10(V / 1e-6) */
double lisn_voltage_to_dbuV(double v) {
    if (v <= 0.0) return -999.0;
    return 20.0 * log10(v / 1.0e-6);
}

/* L5: RF Combiner method - models hardware separator with imbalance */
void lisn_separate_rf_combiner(double vL, double vN, double freq,
                                const lisn_separator_config_t *cfg,
                                lisn_cm_dm_result_t *r) {
    if (!r || !cfg) return;
    /* First do mathematical separation */
    lisn_separate_math_decomp(vL, vN, freq, r);
    /* Apply combiner imperfections */
    double bal_lin = pow(10.0, cfg->combiner_balance_db / 20.0);
    double phase_err_rad = cfg->combiner_phase_err_deg * M_PI / 180.0;
    /* Imbalance causes CM-to-DM leakage */
    double leakage = (1.0 - 1.0/bal_lin) * cos(phase_err_rad);
    double cm_leak = r->cm_voltage_dbuV * fabs(leakage);
    r->dm_voltage_dbuV = 10.0 * log10(pow(10.0, r->dm_voltage_dbuV/10.0) + pow(10.0, cm_leak/10.0));
}

/* L5: CM-to-DM conversion due to LISN impedance imbalance */
/* V_dm_converted = V_cm * (delta_Z / Z_nominal) */
double lisn_cm_to_dm_conversion(double cm_dbuV, double z_imb_pct, double f) {
    (void)f;  /* frequency reserved for future frequency-dependent model */
    double cm_lin = lisn_dbuV_to_voltage(cm_dbuV);
    double dm_lin = cm_lin * (z_imb_pct / 100.0);
    return lisn_voltage_to_dbuV(dm_lin);
}

/* L5: DM-to-CM conversion due to LISN asymmetry */
double lisn_dm_to_cm_conversion(double dm_dbuV, double z_imb_pct, double f) {
    (void)f;
    double dm_lin = lisn_dbuV_to_voltage(dm_dbuV);
    double cm_lin = dm_lin * (z_imb_pct / 100.0);
    return lisn_voltage_to_dbuV(cm_lin);
}

/* L7: Compute required filter attenuation for CM and DM */
/* atten = measured_level - limit + margin (6 dB typical) */
/* This guides EMI filter design: CM choke size, DM capacitor values */
void lisn_filter_attenuation_required(double cm, double dm, double limit,
                                       double margin, double *acm, double *adm) {
    *acm = cm - limit + margin;
    *adm = dm - limit + margin;
    if (*acm < 0.0) *acm = 0.0;
    if (*adm < 0.0) *adm = 0.0;
}

/* L5: Longitudinal Conversion Loss */
/* LCL(dB) = 20*log10(V_cm_input / V_dm_measured) */
double lisn_lcl_compute(double vcm_in, double vdm_meas) {
    if (vdm_meas <= 0.0) return 999.0;
    double cm_lin = lisn_dbuV_to_voltage(vcm_in);
    double dm_lin = lisn_dbuV_to_voltage(vdm_meas);
    if (dm_lin <= 0.0) return 999.0;
    return 20.0 * log10(cm_lin / dm_lin);
}

/* L5: Transverse Conversion Loss (dual of LCL) */
/* TCL(dB) = 20*log10(V_dm_input / V_cm_measured) */
double lisn_tcl_compute(double vdm_in, double vcm_meas) {
    if (vcm_meas <= 0.0) return 999.0;
    double dm_lin = lisn_dbuV_to_voltage(vdm_in);
    double cm_lin = lisn_dbuV_to_voltage(vcm_meas);
    if (cm_lin <= 0.0) return 999.0;
    return 20.0 * log10(dm_lin / cm_lin);
}

/* L5: Full CM/DM sweep across frequency range */
void lisn_cm_dm_sweep(const double *vL, const double *vN,
                       const double *freqs, int n, lisn_cm_dm_result_t *res) {
    if (!vL || !vN || !freqs || !res || n <= 0) return;
    int i;
    for (i = 0; i < n; i++) {
        lisn_separate_math_decomp(vL[i], vN[i], freqs[i], &res[i]);
    }
}
