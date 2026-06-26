/**
 * @file lisn_core.c - Core LISN impedance and transfer function implementations
 * Reference: Paul (2006) Ch.8; CISPR 16-1-2 (2014)
 * Knowledge: L1 Definitions, L3 Math (complex Z, s-domain), L4 Laws
 */
#include "lisn_core.h"
#include <string.h>
#include <stdio.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* L4: |Z(jw)| = (R*wL) / sqrt(R^2 + (wL)^2) - Parallel RL impedance */
double lisn_impedance_magnitude(double L_uh, double R_ohm, double f_hz) {
    if (R_ohm <= 0.0 || L_uh <= 0.0 || f_hz <= 0.0) return 0.0;
    double w = 2.0 * M_PI * f_hz;
    double L = L_uh * 1.0e-6;
    double XL = w * L;
    return (R_ohm * XL) / sqrt(R_ohm * R_ohm + XL * XL);
}

/* L4: Phase = atan(R/(wL)) - From +90deg(DC) to 0deg(HF) */
double lisn_impedance_phase_rad(double L_uh, double R_ohm, double f_hz) {
    if (R_ohm <= 0.0 || L_uh <= 0.0 || f_hz <= 0.0) return 0.0;
    double w = 2.0 * M_PI * f_hz;
    double L = L_uh * 1.0e-6;
    return atan(R_ohm / (w * L));
}

/* L4: fc = R/(2*pi*L) - Corner freq where |Z| drops 3dB from R */
double lisn_corner_frequency_hz(double L_uh, double R_ohm) {
    if (R_ohm <= 0.0 || L_uh <= 0.0) return 0.0;
    return R_ohm / (2.0 * M_PI * L_uh * 1.0e-6);
}

/* L4: VDF = V_recv/V_EUT = R_load / sqrt(R_load^2 + (1/(w*Cc))^2) */
double lisn_voltage_division_factor(const lisn_config_t *cfg, double f, double Rl) {
    if (!cfg || f <= 0.0 || Rl <= 0.0) return 0.0;
    double w = 2.0 * M_PI * f;
    double Cc = cfg->coupling_cap_uf * 1.0e-6;
    double Zc = (Cc > 0.0) ? 1.0 / (w * Cc) : 0.0;
    return Rl / sqrt(Rl * Rl + Zc * Zc);
}

/* L4: IL(dB) = -20*log10(VDF) */
double lisn_insertion_loss_db(const lisn_config_t *cfg, double f) {
    if (!cfg || f <= 0.0) return 999.0;
    double vdf = lisn_voltage_division_factor(cfg, f, cfg->nominal_impedance_ohm);
    if (vdf <= 0.0) return 999.0;
    double il = -20.0 * log10(vdf);
    return (il < 0.0) ? 0.0 : il;
}

/* L3: Full s-domain H(jw) = R_load / |Z_LISN + Zc + R_load| */
double lisn_transfer_function_magnitude(const lisn_config_t *cfg, double f,
                                         double Rl, double Ccp) {
    if (!cfg || f <= 0.0 || Rl <= 0.0) return 0.0;
    double w = 2.0 * M_PI * f;
    double L = cfg->inductance_uh * 1.0e-6;
    double XL = w * L;
    double R = cfg->resistance_ohm;
    double den = R * R + XL * XL;
    double zr = (R * XL * XL) / den;
    double zi = (R * R * XL) / den;
    double Zc = (Ccp > 0.0) ? 1.0 / (w * Ccp) : 0.0;
    double ztr = zr + Rl;
    double zti = zi - Zc;
    double zm = sqrt(ztr * ztr + zti * zti);
    return (zm > 0.0) ? Rl / zm : 0.0;
}

/* L1/L2: Standard LISN initializers */
void lisn_init_cispr22_standard(lisn_config_t *c) {
    if (!c) return;
    memset(c, 0, sizeof(lisn_config_t));
    c->type = LISN_V_50UH; c->inductance_uh = 50.0; c->resistance_ohm = 50.0;
    c->coupling_cap_uf = 0.1; c->freq_start_hz = 150000.0;
    c->freq_stop_hz = 30000000.0; c->nominal_impedance_ohm = 50.0;
    c->num_lines = 2;
    strncpy(c->manufacturer, "CISPR 16-1-2 Std", 63);
    strncpy(c->model, "V-LISN 50uH/50Ohm", 63);
}

void lisn_init_cispr25_standard(lisn_config_t *c) {
    if (!c) return;
    memset(c, 0, sizeof(lisn_config_t));
    c->type = LISN_CISPR25_5UH; c->inductance_uh = 5.0; c->resistance_ohm = 50.0;
    c->coupling_cap_uf = 0.1; c->freq_start_hz = 150000.0;
    c->freq_stop_hz = 108000000.0; c->nominal_impedance_ohm = 50.0;
    c->num_lines = 2;
    strncpy(c->manufacturer, "CISPR 25 Automotive", 63);
    strncpy(c->model, "AN 5uH/50Ohm", 63);
}

void lisn_init_mil461_standard(lisn_config_t *c) {
    if (!c) return;
    memset(c, 0, sizeof(lisn_config_t));
    c->type = LISN_MIL461; c->inductance_uh = 50.0; c->resistance_ohm = 50.0;
    c->coupling_cap_uf = 10.0; c->freq_start_hz = 10000.0;
    c->freq_stop_hz = 10000000.0; c->nominal_impedance_ohm = 50.0;
    c->num_lines = 2;
    strncpy(c->manufacturer, "MIL-STD-461G", 63);
    strncpy(c->model, "LISN 50uH CE102", 63);
}

/* L4: CISPR 16-1-2 impedance tolerance check (+/-20% from nominal) */
int lisn_validate_impedance_compliance(const lisn_config_t *c, double f, double tol) {
    if (!c || f <= 0.0) return 0;
    double z = lisn_impedance_magnitude(c->inductance_uh, c->resistance_ohm, f);
    double zn = c->nominal_impedance_ohm;
    double tf = tol / 100.0;
    return (z >= zn * (1.0 - tf) && z <= zn * (1.0 + tf)) ? 1 : 0;
}

/* L2: Aggregate all impedance parameters at one frequency */
void lisn_compute_impedance_state(const lisn_config_t *c, double f, double Rl,
                                   lisn_impedance_state_t *s) {
    if (!c || !s || f <= 0.0) return;
    double w = 2.0 * M_PI * f;
    double L = c->inductance_uh * 1.0e-6;
    double XL = w * L;
    double R = c->resistance_ohm;
    double den = R * R + XL * XL;
    s->z_real_ohm = (R * XL * XL) / den;
    s->z_imag_ohm = (R * R * XL) / den;
    s->z_magnitude_ohm = lisn_impedance_magnitude(c->inductance_uh, c->resistance_ohm, f);
    s->z_phase_rad = lisn_impedance_phase_rad(c->inductance_uh, c->resistance_ohm, f);
    s->vdf = lisn_voltage_division_factor(c, f, Rl);
    s->insertion_loss_db = lisn_insertion_loss_db(c, f);
    s->frequency_hz = f;
}

/* Check impedance against CISPR 16-1-2 tolerance at specific frequency */
/* Returns deviation percentage (positive = above nominal, negative = below) */
double lisn_impedance_deviation_pct(const lisn_config_t *cfg, double f) {
    if (!cfg || f <= 0.0) return 999.0;
    double z = lisn_impedance_magnitude(cfg->inductance_uh, cfg->resistance_ohm, f);
    return (z - cfg->nominal_impedance_ohm) / cfg->nominal_impedance_ohm * 100.0;
}

/* Determine if LISN is operating in inductive or resistive region */
/* Returns: 1 = inductive (f < fc), 0 = transition (f ~ fc), -1 = resistive (f > fc) */
int lisn_impedance_region(const lisn_config_t *cfg, double f) {
    if (!cfg) return 0;
    double fc = lisn_corner_frequency_hz(cfg->inductance_uh, cfg->resistance_ohm);
    if (f < fc * 0.5) return 1;
    if (f > fc * 2.0) return -1;
    return 0;
}

/* Compute the -3 dB bandwidth of the LISN impedance response */
/* Bandwidth over which |Z| >= R/sqrt(2) ~ 0.707*R */
double lisn_impedance_bandwidth_hz(const lisn_config_t *cfg) {
    if (!cfg) return 0.0;
    return lisn_corner_frequency_hz(cfg->inductance_uh, cfg->resistance_ohm);
}

/* Compute the effective series inductance from measured impedance */
/* L_eff = sqrt((R*R*|Z|*|Z|)/(R*R - |Z|*|Z|)) / (2*pi*f) when |Z| < R */
double lisn_effective_inductance_uh(double z_measured_ohm, double R_ohm, double f) {
    if (R_ohm <= 0.0 || f <= 0.0 || z_measured_ohm >= R_ohm) return 0.0;
    double w = 2.0 * M_PI * f;
    double z2 = z_measured_ohm * z_measured_ohm;
    double R2 = R_ohm * R_ohm;
    double L_henry = sqrt((R2 * z2) / (R2 - z2)) / w;
    return L_henry * 1.0e6;
}

/* Compute the power dissipated in the LISN termination resistor */
/* P = V^2 / R, where V is the RF voltage at the EUT port */
double lisn_power_dissipation_watts(double eut_voltage_rms, double R_ohm) {
    if (R_ohm <= 0.0) return 0.0;
    return eut_voltage_rms * eut_voltage_rms / R_ohm;
}

/* Compute the LISN time constant (L/R) */
double lisn_time_constant_ns(double L_uh, double R_ohm) {
    if (R_ohm <= 0.0) return 0.0;
    return (L_uh * 1.0e-6) / R_ohm * 1.0e9;
}

/* Check if two LISN configurations are equivalent within tolerance */
int lisn_configs_equivalent(const lisn_config_t *a, const lisn_config_t *b, double tol_pct) {
    if (!a || !b) return 0;
    double dL = fabs(a->inductance_uh - b->inductance_uh) / fmax(a->inductance_uh, 1.0);
    double dR = fabs(a->resistance_ohm - b->resistance_ohm) / fmax(a->resistance_ohm, 1.0);
    return (dL * 100.0 <= tol_pct && dR * 100.0 <= tol_pct) ? 1 : 0;
}

/* Compute the maximum rated current for a LISN inductor */
/* Based on wire gauge and core saturation (simplified model) */
double lisn_max_current_estimate(double wire_awg, int parallel_wires) {
    double awg_ampacity[] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
        30.0, 24.0, 20.0, 16.0, 12.0, 10.0, 8.0, 6.0, 5.0, 4.0,
        3.2, 2.5, 2.0, 1.6, 1.3, 1.0, 0.8, 0.6, 0.5, 0.4,
        0.3, 0.25, 0.2, 0.16, 0.13, 0.1, 0.08, 0.06, 0.05, 0.04};
    int idx = (int)wire_awg;
    if (idx < 0 || idx > 39) return 0.0;
    return awg_ampacity[idx] * (double)parallel_wires;
}
