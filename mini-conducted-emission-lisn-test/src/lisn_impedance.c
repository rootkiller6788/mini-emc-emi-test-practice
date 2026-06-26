/** @file lisn_impedance.c - LISN Impedance Network Models */
/** CISPR 16-1-2 Annex A; Paul (2006) Ch.8; Ott (2009) Ch.11 */
/** L1(topology), L2(CM/DM paths), L3(network theory), L4(Kirchhoff) */

#include "lisn_impedance.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Real inductor model: Z = (R + jwL) || (1/(jwC_par)) with self-resonance */
/* Reference: Paul (2006) Section 5.2 - Inductor modeling at RF */
complex_z_t lisn_inductor_impedance(double L_uh, double R_dc, double Cp_pf, double f) {
    complex_z_t z = {0.0, 0.0};
    if (L_uh <= 0.0 || f <= 0.0) return z;
    double w = 2.0 * M_PI * f;
    double L = L_uh * 1.0e-6;
    double Cp = Cp_pf * 1.0e-12;
    /* Denominator: 1 - w^2*L*C_par + jw*R*C_par */
    double a = 1.0 - w * w * L * Cp;
    double b = w * R_dc * Cp;
    double denom = a * a + b * b;
    if (denom > 1.0e-30) {
        /* Z_eff = (R + jwL) / (1 - w^2LC + jwRC) */
        z.real_ohm = (R_dc * a + w * L * b) / denom;
        z.imag_ohm = (w * L * a - R_dc * b) / denom;
    }
    return z;
}

/* Parallel combination: Z = (Z1*Z2)/(Z1+Z2) with complex arithmetic */
/* Essential for computing the standard LISN impedance (R || jwL) */
complex_z_t lisn_parallel_z(complex_z_t z1, complex_z_t z2) {
    complex_z_t r = {0.0, 0.0};
    /* Numerator: Z1*Z2 = (a+jb)(c+jd) = (ac-bd) + j(ad+bc) */
    double nr = z1.real_ohm * z2.real_ohm - z1.imag_ohm * z2.imag_ohm;
    double ni = z1.real_ohm * z2.imag_ohm + z1.imag_ohm * z2.real_ohm;
    /* Denominator: Z1+Z2 = (a+c) + j(b+d) */
    double dr = z1.real_ohm + z2.real_ohm;
    double di = z1.imag_ohm + z2.imag_ohm;
    double dden = dr * dr + di * di;
    if (dden > 1.0e-30) {
        r.real_ohm = (nr * dr + ni * di) / dden;
        r.imag_ohm = (ni * dr - nr * di) / dden;
    }
    return r;
}

/* Series combination: Z = Z1 + Z2 */
complex_z_t lisn_series_z(complex_z_t z1, complex_z_t z2) {
    complex_z_t r;
    r.real_ohm = z1.real_ohm + z2.real_ohm;
    r.imag_ohm = z1.imag_ohm + z2.imag_ohm;
    return r;
}

/* V-LISN EUT-port impedance - full network model */
/* Circuit: EUT port -> L_series -> [R_shunt || (C_coupling -> 50 Ohm)] */
complex_z_t lisn_v_network_impedance(const lisn_network_model_t *m, double f) {
    complex_z_t z = {0.0, 0.0};
    if (!m || f <= 0.0) return z;
    double w = 2.0 * M_PI * f;
    double L = m->L_series_uh * 1.0e-6;
    double Rsh = m->R_shunt_ohm;
    double Cc = m->C_coupling_nf * 1.0e-9;
    double Rload = 50.0;
    double Zc = (Cc > 1.0e-12) ? -1.0 / (w * Cc) : 0.0;
    double nr = Rsh * Rload;
    double ni = Rsh * Zc;
    double dr = Rsh + Rload;
    double di = Zc;
    double dden = dr * dr + di * di;
    double Zsh_r = (dden > 0.0) ? (nr * dr + ni * di) / dden : Rsh;
    double Zsh_i = (dden > 0.0) ? (ni * dr - nr * di) / dden : 0.0;
    z.real_ohm = Zsh_r + m->R_series_ohm;
    z.imag_ohm = w * L + Zsh_i;
    return z;
}

/* Delta LISN: symmetrical 3-phase network */
complex_z_t lisn_delta_network_impedance(const lisn_network_model_t *m, double f) {
    return lisn_v_network_impedance(m, f); }

/* Common-Mode impedance: L and N in parallel to ground */
complex_z_t lisn_common_mode_impedance(const lisn_network_model_t *m, double f) {
    complex_z_t zs = lisn_v_network_impedance(m, f);
    complex_z_t zcm; zcm.real_ohm = zs.real_ohm / 2.0; zcm.imag_ohm = zs.imag_ohm / 2.0;
    return zcm; }

/* Differential-Mode impedance: L-to-N path */
complex_z_t lisn_differential_mode_impedance(const lisn_network_model_t *m, double f) {
    complex_z_t zs = lisn_v_network_impedance(m, f);
    complex_z_t zdm; zdm.real_ohm = zs.real_ohm * 2.0; zdm.imag_ohm = zs.imag_ohm * 2.0;
    return zdm; }

/* Inductor self-resonant frequency: f_SRF = 1/(2pi*sqrt(L*C_par)) */
double lisn_self_resonant_frequency(double L_uh, double C_par_pf) {
    if (L_uh <= 0.0 || C_par_pf <= 0.0) return 1.0e12;
    return 1.0 / (2.0 * M_PI * sqrt(L_uh * 1.0e-6 * C_par_pf * 1.0e-12)); }

/* Impedance sweep: Z(f) at N log-spaced points */
void lisn_impedance_sweep(const lisn_network_model_t *m, double fs, double fe, int n, lisn_impedance_state_t *res) {
    if (!m || !res || n <= 0 || fs <= 0.0 || fe <= fs) return;
    double ratio = (n > 1) ? pow(fe / fs, 1.0 / (double)(n - 1)) : 1.0;
    double freq = fs; int i;
    for (i = 0; i < n; i++) {
        complex_z_t z = lisn_v_network_impedance(m, freq);
        res[i].frequency_hz = freq;
        res[i].z_real_ohm = z.real_ohm; res[i].z_imag_ohm = z.imag_ohm;
        res[i].z_magnitude_ohm = sqrt(z.real_ohm*z.real_ohm + z.imag_ohm*z.imag_ohm);
        res[i].z_phase_rad = atan2(z.imag_ohm, z.real_ohm);
        res[i].vdf = 1.0; res[i].insertion_loss_db = 0.0;
        freq *= ratio; } }

/* Coupling capacitor impedance at frequency f */
double lisn_coupling_cap_impedance(double C_uf, double f_hz) {
    if (C_uf <= 0.0 || f_hz <= 0.0) return 1.0e9;
    return 1.0 / (2.0 * M_PI * f_hz * C_uf * 1.0e-6); }

/* Initialize CISPR 22 V-LISN network model */
void lisn_network_model_init_cispr22(lisn_network_model_t *m) {
    if (!m) return;
    m->topology = LISN_TOPOLOGY_V; m->L_series_uh = 50.0; m->R_series_ohm = 0.5;
    m->R_shunt_ohm = 50.0; m->C_shunt_nf = 0.0; m->C_coupling_nf = 100.0;
    m->R_discharge_kohm = 100.0; m->L_parasitic_nh = 50.0; m->C_parasitic_pf = 15.0;
    m->R_core_loss_ohm = 5000.0; m->skin_effect_coeff = 0.0; }

/* CISPR 16-1-2 impedance tolerance check */
void lisn_impedance_tolerance_check(const lisn_network_model_t *m, double fs, double fe, int n, double *max_pct, double *worst_f) {
    if (!m || !max_pct || !worst_f || n <= 0) return;
    *max_pct = 0.0; *worst_f = fs;
    double ratio = (n > 1) ? pow(fe / fs, 1.0 / (double)(n - 1)) : 1.0;
    double freq = fs; int i;
    for (i = 0; i < n; i++) {
        complex_z_t z = lisn_v_network_impedance(m, freq);
        double zm = sqrt(z.real_ohm*z.real_ohm + z.imag_ohm*z.imag_ohm);
        double dev = fabs(zm - 50.0) / 50.0 * 100.0;
        if (dev > *max_pct) { *max_pct = dev; *worst_f = freq; }
        freq *= ratio; } }
