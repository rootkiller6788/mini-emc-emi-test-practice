/** @file lisn_advanced.c - Advanced LISN Analysis: S-params, TDR, statistics */
/** L3 (S-parameters), L5 (TDR cal, statistics), L7 (CSV export), L8 (advanced) */

#include "lisn_core.h"
#include "lisn_impedance.h"
#include "lisn_measurement.h"
#include "lisn_standard.h"
#include "lisn_calibration.h"
#include "lisn_noise_separator.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

double lisn_impedance_at_frequency(const lisn_config_t *cfg, double f) {
    if (!cfg) return 0.0;
    return lisn_impedance_magnitude(cfg->inductance_uh, cfg->resistance_ohm, f);
}

double lisn_phase_margin_degrees(const lisn_config_t *cfg, double f) {
    if (!cfg) return 0.0;
    double phase_rad = lisn_impedance_phase_rad(cfg->inductance_uh, cfg->resistance_ohm, f);
    double phase_deg = phase_rad * 180.0 / M_PI;
    return 90.0 - phase_deg;
}

int lisn_check_frequency_in_range(const lisn_config_t *cfg, double f) {
    if (!cfg) return 0;
    return (f >= cfg->freq_start_hz && f <= cfg->freq_stop_hz) ? 1 : 0;
}

void lisn_get_standard_bandwidth(double f, double *bw, cispr_bandwidth_t *band) {
    if (!bw || !band) return;
    if (f < 150000.0) { *bw = 200.0; *band = CISPR_BW_200HZ; }
    else if (f < 30000000.0) { *bw = 9000.0; *band = CISPR_BW_9KHZ; }
    else if (f < 1000000000.0) { *bw = 120000.0; *band = CISPR_BW_120KHZ; }
    else { *bw = 1000000.0; *band = CISPR_BW_1MHZ; }
}

double lisn_thermal_noise_floor_dbm(double bw, double temp_c) {
    double T_kelvin = temp_c + 273.15;
    double k = 1.380649e-23;
    double noise_watts = k * T_kelvin * bw;
    if (noise_watts <= 0.0) return -999.0;
    return 10.0 * log10(noise_watts / 0.001);
}

void lisn_compute_pole_zero(const lisn_config_t *cfg, lisn_pole_zero_t *pz) {
    if (!cfg || !pz) return;
    double L = cfg->inductance_uh * 1.0e-6;
    double R = cfg->resistance_ohm;
    pz->pole_real = -R / L;
    pz->pole_imag = 0.0;
    pz->zero_real = 0.0;
    pz->zero_imag = 0.0;
    pz->dc_gain = 0.0;
}

void lisn_s_to_abcd(const lisn_s_parameters_t *s, lisn_abcd_matrix_t *abcd) {
    if (!s || !abcd) return;
    double denom_re = s->s21_real * 2.0;
    double denom_im = s->s21_imag * 2.0;
    double dden = denom_re * denom_re + denom_im * denom_im;
    if (dden <= 0.0) return;
    double z0 = 50.0;
    double s11r = s->s11_real, s11i = s->s11_imag;
    double s22r = s->s22_real, s22i = s->s22_imag;
    abcd->A_real = ((1.0 + s11r) * (1.0 - s22r) + s11i * s22i) / 2.0;
    abcd->B_real = z0 * ((1.0 + s11r) * (1.0 + s22r) - s11i * s22i) / 2.0;
    abcd->C_real = ((1.0 - s11r) * (1.0 - s22r) - s11i * s22i) / (2.0 * z0);
    abcd->D_real = ((1.0 - s11r) * (1.0 + s22r) + s11i * s22i) / 2.0;
    abcd->A_imag = abcd->B_imag = abcd->C_imag = abcd->D_imag = 0.0;
}

double lisn_reflection_coefficient(double z_load, double z0) {
    if (z0 <= 0.0) return 1.0;
    return (z_load - z0) / (z_load + z0);
}

double lisn_vswr_from_reflection(double gamma) {
    double abs_g = fabs(gamma);
    if (abs_g >= 1.0) return 1.0e9;
    return (1.0 + abs_g) / (1.0 - abs_g);
}

double lisn_return_loss_db(const complex_z_t *z_load, double z0) {
    if (!z_load || z0 <= 0.0) return 0.0;
    double z_mag = sqrt(z_load->real_ohm * z_load->real_ohm + z_load->imag_ohm * z_load->imag_ohm);
    double gamma = (z_mag - z0) / (z_mag + z0);
    return -20.0 * log10(fabs(gamma));
}

/* TDR capture management */
void lisn_td_capture_init(lisn_td_capture_t *cap, int n, double rate) {
    if (!cap || n <= 0) return;
    cap->samples = calloc((size_t)n, sizeof(double));
    cap->num_samples = n; cap->sample_rate_hz = rate;
    cap->trigger_level = 0.0; cap->trigger_slope = 1; }

void lisn_td_capture_free(lisn_td_capture_t *cap) {
    if (!cap) return;
    free(cap->samples);
    cap->samples = NULL;
    cap->num_samples = 0;
}

double lisn_find_max_emission_in_band(const lisn_measurement_dataset_t *ds, double fl, double fh) {
    if (!ds || !ds->results || ds->num_results <= 0) return -999.0;
    double max_l = -999.0; int i;
    for (i = 0; i < ds->num_results; i++) {
        double f = ds->results[i].frequency_hz;
        if (f >= fl && f <= fh && ds->results[i].final_level_dbuV > max_l)
            max_l = ds->results[i].final_level_dbuV; }
    return max_l; }

/* Additional advanced functions */

double lisn_max_power_transfer_impedance(const lisn_network_model_t *m, double f) {
    if (!m || f <= 0.0) return 0.0;
    complex_z_t z = lisn_v_network_impedance(m, f);
    double zm = sqrt(z.real_ohm*z.real_ohm + z.imag_ohm*z.imag_ohm);
    return zm;
}

void lisn_abcd_to_s(const lisn_abcd_matrix_t *abcd, lisn_s_parameters_t *s) {
    if (!abcd || !s) return;
    double denom = abcd->A_real + abcd->B_real/50.0 + abcd->C_real*50.0 + abcd->D_real;
    if (fabs(denom) < 1e-12) return;
    s->s11_real = (abcd->A_real + abcd->B_real/50.0 - abcd->C_real*50.0 - abcd->D_real) / denom;
    s->s21_real = 2.0 / denom;
    s->s12_real = 2.0 * (abcd->A_real*abcd->D_real - abcd->B_real*abcd->C_real) / denom;
    s->s22_real = (-abcd->A_real + abcd->B_real/50.0 - abcd->C_real*50.0 + abcd->D_real) / denom;
}

double lisn_z_parameter_to_s(double z_re, double z_im, double f_hz) {
    (void)f_hz;
    double z0 = 50.0;
    double denom = (z_re + z0)*(z_re + z0) + z_im*z_im;
    if (denom <= 0.0) return 1.0;
    double s11_mag_sq = ((z_re-z0)*(z_re-z0) + z_im*z_im) / denom;
    return sqrt(s11_mag_sq);
}

/* Additional diagnostics and troubleshooting functions */

void lisn_compliance_summary_json(const lisn_compliance_report_t *r, char *buf, int sz) {
    if (!r || !buf || sz <= 0) return;
    snprintf(buf, (size_t)sz,
        "{\"standard\":\"%s\",\"points\":%d,\"pass\":%s,\"worst_margin_db\":%.1f,\"worst_freq_hz\":%.0f}",
        r->limit_line.description, r->num_points,
        r->overall_pass ? "true" : "false",
        r->worst_margin_db, r->worst_frequency_hz);
}

double lisn_cal_interpolate_vdf(const lisn_calibration_t *cal, double f) {
    if (!cal || !cal->points || cal->num_points <= 0) return 0.0;
    int lo = 0, hi = cal->num_points - 1;
    if (f <= cal->points[0].frequency_hz) return cal->points[0].vdf_db;
    if (f >= cal->points[hi].frequency_hz) return cal->points[hi].vdf_db;
    while (hi - lo > 1) {
        int mid = (lo + hi) / 2;
        if (cal->points[mid].frequency_hz <= f) lo = mid;
        else hi = mid;
    }
    double t = (f - cal->points[lo].frequency_hz) / (cal->points[hi].frequency_hz - cal->points[lo].frequency_hz);
    return cal->points[lo].vdf_db + t * (cal->points[hi].vdf_db - cal->points[lo].vdf_db);
}

void lisn_mode_matrix_invert(const lisn_mode_matrix_t *m, lisn_mode_matrix_t *inv) {
    if (!m || !inv) return;
    double det = m->a11 * m->a22 - m->a12 * m->a21;
    if (fabs(det) < 1e-12) return;
    inv->a11 = m->a22 / det;  inv->a12 = -m->a12 / det;
    inv->a21 = -m->a21 / det; inv->a22 = m->a11 / det;
}

void lisn_apply_mode_matrix(const lisn_mode_matrix_t *mat, double vL, double vN, double *vcm, double *vdm) {
    if (!mat || !vcm || !vdm) return;
    *vcm = mat->a11 * vL + mat->a12 * vN;
    *vdm = mat->a21 * vL + mat->a22 * vN;
}

double lisn_dBuV_to_dBm(double dbuV, double impedance_ohm) {
    if (impedance_ohm <= 0.0) return -999.0;
    double v = pow(10.0, dbuV / 20.0) * 1.0e-6;
    double p_watts = v * v / impedance_ohm;
    return 10.0 * log10(p_watts / 0.001);
}

double lisn_dBm_to_dBuV(double dbm, double impedance_ohm) {
    if (impedance_ohm <= 0.0) return -999.0;
    double p_watts = pow(10.0, dbm / 10.0) * 0.001;
    double v = sqrt(p_watts * impedance_ohm);
    return 20.0 * log10(v / 1.0e-6);
}

double lisn_cable_loss_estimate(double freq_hz, double cable_length_m, int cable_type) {
    if (freq_hz <= 0.0 || cable_length_m <= 0.0) return 0.0;
    double freq_mhz = freq_hz / 1.0e6;
    double loss_per_m;
    switch (cable_type) {
        case 0: loss_per_m = 0.05 * sqrt(freq_mhz); break;
        case 1: loss_per_m = 0.10 * sqrt(freq_mhz); break;
        case 2: loss_per_m = 0.02 * sqrt(freq_mhz); break;
        case 3: loss_per_m = 0.20 * sqrt(freq_mhz); break;
        default: loss_per_m = 0.05 * sqrt(freq_mhz); break;
    }
    return loss_per_m * cable_length_m;
}
