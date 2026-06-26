/**
 * emi_probe_theory.c — Near-Field Probe Theory Implementation
 *
 * E-field and H-field probe response calculations, coupling models,
 * mutual inductance/capacitance, wave impedance, and calibration.
 *
 * Knowledge Coverage:
 *   L2: Probe loading effects, frequency response
 *   L3: Faraday's law, Biot-Savart, Coulomb's law applications
 *   L4: Mutual inductance/capacitance derivations
 *   L5: Probe calibration algorithm, elliptic integral approximations
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "emi_probe_theory.h"

/* ─── Elliptic Integral Approximation ────────────────────────────────── */

/**
 * Complete elliptic integral of the first kind, K(k).
 * Uses polynomial approximation from Abramowitz & Stegun, 17.3.33.
 * Relative error < 2e-8 for 0 <= k < 1.
 *
 * K(k) = ∫_0^{π/2} dθ / sqrt(1 - k^2 * sin^2(θ))
 */
static double elliptic_k(double k) {
    if (k >= 1.0) k = 0.9999999999; /* avoid divergence at k=1 */
    if (k <= 0.0) return EMI_PI / 2.0;

    /* Arithmetic-geometric mean (AGM) method for accuracy */
    double a = 1.0;
    double b = sqrt(1.0 - k * k);
    double c;
    int max_iter = 30;

    for (int i = 0; i < max_iter; i++) {
        double a_next = (a + b) / 2.0;
        b = sqrt(a * b);
        c = (a - b) / 2.0;
        a = a_next;
        if (c / a < 1e-15) break;
    }
    return EMI_PI / (2.0 * a);
}

/**
 * Complete elliptic integral of the second kind, E(k).
 * E(k) = ∫_0^{π/2} sqrt(1 - k^2 * sin^2(θ)) dθ
 *
 * Uses AGM method. E(k) = K(k) * (1 - k^2/2 * sum_i 2^i * c_i^2)
 */
static double elliptic_e(double k) {
    if (k >= 1.0) k = 0.9999999999;
    if (k <= 0.0) return EMI_PI / 2.0;

    double a = 1.0;
    double b = sqrt(1.0 - k * k);
    double sum_c2 = 0.0;
    double pow2 = 1.0;
    int max_iter = 30;

    for (int i = 0; i < max_iter; i++) {
        double a_next = (a + b) / 2.0;
        double c = (a - b) / 2.0;
        sum_c2 += pow2 * c * c;
        pow2 *= 2.0;
        b = sqrt(a * b);
        a = a_next;
        if (c / a < 1e-15) break;
    }
    return EMI_PI * (1.0 - sum_c2) / (2.0 * a);
}

/* ─── E-Field Probe Functions ────────────────────────────────────────── */

double emi_eprobe_output_voltage(double efield_vm, const emi_eprobe_spec_t *probe,
                                  double load_cap_f) {
    if (!probe) return 0.0;
    /* Effective height: h_eff ≈ probe_length / 2 for short monopole */
    double h_eff = probe->probe_length_m / 2.0;
    /* Capacitive voltage division */
    double c_ant = emi_eprobe_capacitance(probe->probe_length_m,
                                           probe->probe_diameter_m);
    double c_load_corrected = load_cap_f + probe->input_capacitance_f;
    double total_c = c_ant + c_load_corrected;
    if (total_c <= 0.0) return 0.0;
    double v_divider = c_ant / total_c;
    return efield_vm * h_eff * v_divider;
}

double emi_eprobe_efield_from_voltage(double vout, const emi_eprobe_spec_t *probe,
                                       double load_cap_f) {
    if (!probe) return 0.0;
    double h_eff = probe->probe_length_m / 2.0;
    double c_ant = emi_eprobe_capacitance(probe->probe_length_m,
                                           probe->probe_diameter_m);
    double c_load_corrected = load_cap_f + probe->input_capacitance_f;
    double total_c = c_ant + c_load_corrected;
    if (total_c <= 0.0 || c_ant <= 0.0 || h_eff <= 0.0) return 0.0;
    double v_divider = c_ant / total_c;
    if (v_divider <= 0.0) return 0.0;
    return vout / (h_eff * v_divider);
}

double emi_eprobe_sensitivity_mag(double freq_hz, const emi_eprobe_spec_t *probe,
                                   double load_cap_f) {
    if (!probe) return 0.0;
    double h_eff = probe->probe_length_m / 2.0;
    double c_ant = emi_eprobe_capacitance(probe->probe_length_m,
                                           probe->probe_diameter_m);
    double c_total = c_ant + load_cap_f + probe->input_capacitance_f;
    double r_in = probe->input_resistance_ohm;
    if (r_in <= 0.0 || c_total <= 0.0) return h_eff;
    /* First-order lowpass model:
     * |S(f)| = h_eff * C_ant/C_total / sqrt(1 + (f/f_c)^2)
     * where f_c = 1/(2*pi*R*C_total) */
    double fc = 1.0 / (2.0 * EMI_PI * r_in * c_total);
    double v_divider = c_ant / c_total;
    double denom = sqrt(1.0 + (freq_hz / fc) * (freq_hz / fc));
    return h_eff * v_divider / denom;
}

/* ─── H-Field Probe Functions ────────────────────────────────────────── */

double emi_hprobe_self_inductance(double loop_radius_m, double wire_radius_m,
                                   int num_turns) {
    if (loop_radius_m <= 0.0 || wire_radius_m <= 0.0) return 0.0;
    double mu0 = 4.0 * EMI_PI * 1.0e-7;  /* 4π × 10^-7 H/m */
    double n = (double)num_turns;
    /* Grover Eq. 119: L = mu0 * N^2 * r * [ln(8r/a) - 2] */
    double ratio = 8.0 * loop_radius_m / wire_radius_m;
    if (ratio <= 1.0) return mu0 * n * n * loop_radius_m;
    return mu0 * n * n * loop_radius_m * (log(ratio) - 2.0);
}

double emi_hprobe_output_voltage(double freq_hz, double hfield_am,
                                  const emi_hprobe_spec_t *probe) {
    if (!probe) return 0.0;
    double mu0 = 4.0 * EMI_PI * 1.0e-7;
    double r = probe->loop_diameter_m / 2.0;
    double area = EMI_PI * r * r;        /* Loop area, m^2 */
    double n = (double)probe->num_turns;
    /* Faraday: V = ω * N * μ0 * H * A */
    double omega = 2.0 * EMI_PI * freq_hz;
    double v_ideal = omega * n * mu0 * hfield_am * area;

    /* Self-resonance correction:
     * V_actual = V_ideal / sqrt((1 - (f/f_res)^2)^2 + (f/(f_res*Q))^2)
     * Simplified: only apply near resonance */
    double f_res = probe->self_resonance_hz;
    if (f_res > 0.0 && freq_hz > 0.1 * f_res) {
        double ratio = freq_hz / f_res;
        double q = 20.0; /* typical Q for small loops */
        double denom = sqrt((1.0 - ratio * ratio) * (1.0 - ratio * ratio)
                            + (ratio / q) * (ratio / q));
        if (denom < 0.01) denom = 0.01;
        return v_ideal / denom;
    }
    return v_ideal;
}

double emi_hprobe_hfield_from_voltage(double freq_hz, double vout,
                                       const emi_hprobe_spec_t *probe) {
    if (!probe || freq_hz <= 0.0) return 0.0;
    double mu0 = 4.0 * EMI_PI * 1.0e-7;
    double r = probe->loop_diameter_m / 2.0;
    double area = EMI_PI * r * r;
    double n = (double)probe->num_turns;
    double omega = 2.0 * EMI_PI * freq_hz;
    double denominator = omega * n * mu0 * area;
    if (denominator <= 0.0) return 0.0;
    return vout / denominator;
}

double emi_hprobe_antenna_factor(double freq_hz, const emi_hprobe_spec_t *probe) {
    if (!probe) return 0.0;
    double mu0 = 4.0 * EMI_PI * 1.0e-7;
    double r = probe->loop_diameter_m / 2.0;
    double area = EMI_PI * r * r;
    double n = (double)probe->num_turns;
    double omega = 2.0 * EMI_PI * freq_hz;
    double sensitivity = omega * n * mu0 * area; /* V / (A/m) */
    if (sensitivity <= 0.0) return 200.0;
    /* AF_H = -20*log10(S) = -20*log10(V/(A/m)) */
    return -20.0 * log10(sensitivity);
}

double emi_hprobe_self_resonance(const emi_hprobe_spec_t *probe) {
    if (!probe) return 1e9;
    double mu0 = 4.0 * EMI_PI * 1.0e-7;
    double r = probe->loop_diameter_m / 2.0;
    double a = probe->wire_diameter_m / 2.0;
    /* Effective loop inductance (single turn for resonance calc) */
    double ratio = 8.0 * r / a;
    double L = mu0 * r * (ratio > 1.0 ? log(ratio) - 2.0 : 0.0);
    if (L <= 0.0) return 1e9;
    /* Self-capacitance estimate for circular loop:
     * C_self ≈ 2*pi*eps0 * r / ln(8r/a) */
    double eps0 = 8.8541878128e-12;
    double C = 2.0 * EMI_PI * eps0 * r / log(ratio > 1.0 ? ratio : 2.0);
    if (C <= 0.0) return 1e9;
    return 1.0 / (2.0 * EMI_PI * sqrt(L * C));
}

/* ─── Mutual Inductance and Capacitance ──────────────────────────────── */

double emi_mutual_inductance_coaxial(double r1_m, double r2_m, double distance_m) {
    if (r1_m <= 0.0 || r2_m <= 0.0) return 0.0;
    if (distance_m < 0.0) distance_m = 0.0;

    double mu0 = 4.0 * EMI_PI * 1.0e-7;
    double sum_r = r1_m + r2_m;
    double k2 = (4.0 * r1_m * r2_m)
                / (sum_r * sum_r + distance_m * distance_m);
    if (k2 >= 1.0) k2 = 0.9999999999;
    if (k2 <= 0.0) return 0.0;

    double k = sqrt(k2);
    double Kk = elliptic_k(k);
    double Ek = elliptic_e(k);

    /* M = mu0 * sqrt(r1*r2) * [ (2/k - k)*K(k) - (2/k)*E(k) ]
     * The sign depends on loop geometry; take absolute value. */
    double term = (2.0 / k - k) * Kk - (2.0 / k) * Ek;
    return fabs(mu0 * sqrt(r1_m * r2_m) * term);
}

double emi_mutual_capacitance_parallel(double radius_m, double spacing_m,
                                        double length_m) {
    if (radius_m <= 0.0 || spacing_m <= 2.0 * radius_m || length_m <= 0.0) {
        return 0.0;
    }
    double eps0 = 8.8541878128e-12;
    /* C_m = pi * eps0 * L / arccosh(d / (2a)) */
    double x = spacing_m / (2.0 * radius_m);
    /* arccosh(x) = ln(x + sqrt(x^2 - 1)) for x >= 1 */
    if (x < 1.0) return 0.0;
    double acosh_x = log(x + sqrt(x * x - 1.0));
    if (acosh_x <= 0.0) return 0.0;
    return EMI_PI * eps0 * length_m / acosh_x;
}

/* ─── Near-Field Wave Impedance ──────────────────────────────────────── */

double emi_near_field_wave_impedance(double distance_m, double freq_hz,
                                      int source_type) {
    if (distance_m <= 0.0 || freq_hz <= 0.0) return EMI_Z0_FREESPACE;

    double lambda = EMI_C0 / freq_hz;
    double k = 2.0 * EMI_PI / lambda;   /* Wave number */
    double kr = k * distance_m;

    if (kr < 1e-6) {
        /* Extremely close — return asymptotic values */
        return source_type ? 0.0 : 1e9;
    }
    if (kr > 100.0) {
        /* Far-field: both approaches return Z0 */
        return EMI_Z0_FREESPACE;
    }

    double kr2 = kr * kr;
    double kr4 = kr2 * kr2;
    double sqrt_term_K2 = sqrt(1.0 + 1.0 / kr2);
    double sqrt_term_K4 = sqrt(1.0 + 1.0 / kr2 + 1.0 / kr4);

    if (source_type == 0) {
        /* Electric dipole source: Z_w >> Z0 in near-field
         * Z_w = Z0 * sqrt(1 + 1/(kr)^2 + 1/(kr)^4) / sqrt(1 + 1/(kr)^2) */
        return EMI_Z0_FREESPACE * sqrt_term_K4 / sqrt_term_K2;
    } else {
        /* Magnetic loop source: Z_w << Z0 in near-field
         * Z_w = Z0 * sqrt(1 + 1/(kr)^2) / sqrt(1 + 1/(kr)^2 + 1/(kr)^4) */
        return EMI_Z0_FREESPACE * sqrt_term_K2 / sqrt_term_K4;
    }
}

/* ─── Probe Calibration Functions ────────────────────────────────────── */

void emi_calibrate_eprobe(double e_ref_vm,
                           const double *v_measured_arr,
                           const double *freq_hz_arr,
                           int num_points,
                           double *cal_factor_db_out) {
    if (!v_measured_arr || !freq_hz_arr || !cal_factor_db_out || num_points <= 0) return;
    for (int i = 0; i < num_points; i++) {
        if (v_measured_arr[i] <= 0.0) {
            cal_factor_db_out[i] = 0.0;
        } else {
            /* K_dB = 20*log10(E_ref / V_measured) */
            cal_factor_db_out[i] = 20.0 * log10(e_ref_vm / v_measured_arr[i]);
        }
    }
}

void emi_calibrate_hprobe(double h_ref_am,
                           const double *v_measured_arr,
                           const double *freq_hz_arr,
                           int num_points,
                           double *cal_factor_db_out) {
    if (!v_measured_arr || !freq_hz_arr || !cal_factor_db_out || num_points <= 0) return;
    for (int i = 0; i < num_points; i++) {
        if (v_measured_arr[i] <= 0.0) {
            cal_factor_db_out[i] = 0.0;
        } else {
            /* K_dB = 20*log10(H_ref / V_measured) */
            cal_factor_db_out[i] = 20.0 * log10(h_ref_am / v_measured_arr[i]);
        }
    }
}

double emi_apply_eprobe_calibration(double v_measured, double cal_factor_db) {
    double factor_linear = pow(10.0, cal_factor_db / 20.0);
    return v_measured * factor_linear;
}

double emi_apply_hprobe_calibration(double v_measured, double cal_factor_db) {
    double factor_linear = pow(10.0, cal_factor_db / 20.0);
    return v_measured * factor_linear;
}

double emi_interpolate_cal_factor(double freq_hz,
                                   const double *freq_arr,
                                   const double *cal_db_arr,
                                   int num_points) {
    if (!freq_arr || !cal_db_arr || num_points <= 0) return 0.0;
    if (num_points == 1) return cal_db_arr[0];

    /* Find bracketing points */
    if (freq_hz <= freq_arr[0]) return cal_db_arr[0];
    if (freq_hz >= freq_arr[num_points - 1]) return cal_db_arr[num_points - 1];

    for (int i = 0; i < num_points - 1; i++) {
        if (freq_hz >= freq_arr[i] && freq_hz <= freq_arr[i + 1]) {
            /* Log-frequency linear interpolation */
            double log_f = log10(freq_hz);
            double log_fa = log10(freq_arr[i]);
            double log_fb = log10(freq_arr[i + 1]);
            double t = (log_f - log_fa) / (log_fb - log_fa);
            return cal_db_arr[i] + t * (cal_db_arr[i + 1] - cal_db_arr[i]);
        }
    }
    return cal_db_arr[num_points - 1];
}
