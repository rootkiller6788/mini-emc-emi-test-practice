/** @file lisn_phase.c - Phase Analysis, Group Delay, Pre-Compliance Methods */
/** L7 Applications: low-cost pre-compliance, GTEM correlation, CDNE method */
/** L8 Advanced: stochastic phase noise impact, time-varying impedance */

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

/* L7: Phase angle vs frequency for a standard V-LISN */
/* The phase transitions from +90deg (inductive) to 0deg (resistive) */
/* This transition is critical for understanding LISN behavior */
void lisn_phase_vs_frequency(const lisn_config_t *cfg, double f_start, double f_stop,
                              int n, double *freqs, double *phases) {
    if (!cfg || !freqs || !phases || n <= 0) return;
    double ratio = pow(f_stop / f_start, 1.0 / (double)(n - 1));
    double f = f_start;
    int i;
    for (i = 0; i < n; i++) {
        freqs[i] = f;
        phases[i] = lisn_impedance_phase_rad(cfg->inductance_uh, cfg->resistance_ohm, f) * 180.0 / M_PI;
        f *= ratio;
    }
}

/* L7: Group delay through LISN = -d(phase)/d(omega) */
/* Important for time-domain reflectometry (TDR) LISN characterization */
void lisn_group_delay(const lisn_config_t *cfg, double f_start, double f_stop,
                       int n, double *freqs, double *delays_ns) {
    if (!cfg || !freqs || !delays_ns || n < 2) return;
    double ratio = pow(f_stop / f_start, 1.0 / (double)(n - 1));
    double f_prev = f_start;
    double phase_prev = lisn_impedance_phase_rad(cfg->inductance_uh, cfg->resistance_ohm, f_prev);
    freqs[0] = f_start; delays_ns[0] = 0.0;
    int i;
    double f = f_start * ratio;
    for (i = 1; i < n; i++) {
        double phase = lisn_impedance_phase_rad(cfg->inductance_uh, cfg->resistance_ohm, f);
        double dw = 2.0 * M_PI * (f - f_prev);
        double dphi = phase - phase_prev;
        freqs[i] = f;
        delays_ns[i] = (dw > 1e-12) ? fabs(dphi / dw) * 1.0e9 : 0.0;
        f_prev = f; phase_prev = phase;
        f *= ratio;
    }
}

/* L7: Pre-compliance low-cost conducted emission estimation */
/* Uses a simplified LISN model (resistive divider + simple RC) instead of a full LISN */
/* This is used by hobbyists and small companies for pre-scan before formal testing */
/* Method: 150 Ohm resistor + 0.1 uF cap to ground -> 50 Ohm spectrum analyzer */
/* Correction factor: approx. +10 dB from this setup to standard LISN reading */
double lisn_precompliance_correction_factor(double f_hz) {
    /* Pre-compliance correction for simple RC probe vs standard 50uH LISN */
    /* Derived from comparing Z_probe vs Z_LISN across frequency */
    /* f < 1 MHz: approx +10 dB correction */
    /* 1-10 MHz: correction decreases (probe impedance approaches 50 Ohm) */
    /* > 10 MHz: correction ~ +2 dB (both present ~50 Ohm) */
    if (f_hz < 150000.0) return 0.0;
    double f_mhz = f_hz / 1.0e6;
    if (f_mhz < 1.0) return 10.0;
    if (f_mhz < 10.0) return 10.0 - 8.0 * (f_mhz - 1.0) / 9.0;
    return 2.0;
}

/* L7: GTEM-to-OATS correlation for conducted emissions (indirect correlation) */
/* GTEM cell measurements at low frequencies show some correlation with conducted */
/* emissions on power cables (especially CM-to-radiated conversion) */
double lisn_gtem_to_oats_correlation(double gtem_level_dbuV, double freq_hz) {
    /* Empirical correlation factor based on IEC 61000-4-20 Annex H */
    /* Converts GTEM measurement to expected OATS conducted emission level */
    double f_mhz = freq_hz / 1.0e6;
    double cf_db = 0.0;
    if (f_mhz < 30.0) { cf_db = -3.0 + 2.0 * log10(f_mhz); }
    else { cf_db = -1.0; }
    return gtem_level_dbuV + cf_db;
}

/* L7: CDNE (Coupling/Decoupling Network for Emission) alternative method */
/* CDNE is an alternative to LISN per CISPR 16-1-2 for certain EUT types */
/* Provides both CM and DM impedance simultaneously at the measurement port */
double lisn_cdne_equivalent_impedance(double f_hz) {
    /* CDNE impedance characteristic (simplified) */
    double w = 2.0 * M_PI * f_hz;
    double L_cdne = 30.0e-6;  /* 30 uH effective */
    double R_cdne = 150.0;     /* 150 Ohm (CM impedance) */
    double XL = w * L_cdne;
    return (R_cdne * XL) / sqrt(R_cdne * R_cdne + XL * XL);
}

/* L7: ISO 7637-2 pulse characterization through LISN */
/* ISO 7637 defines conducted transients on automotive power lines */
/* Pulse 1: negative transient from inductive load disconnect */
/* Pulse 2a: positive transient from parallel device switching */
/* Pulse 3a/3b: fast switching transients (ns range) */
double lisn_iso7637_pulse_response(double pulse_amplitude_v, double pulse_rise_ns,
                                    double L_uh, double R_ohm) {
    /* LISN acts as a low-pass filter for fast transients */
    /* f_3dB = R/(2*pi*L) ~ 159 kHz for 50uH/50Ohm */
    double f_3db = R_ohm / (2.0 * M_PI * L_uh * 1.0e-6);
    double pulse_bw_hz = 0.35 / (pulse_rise_ns * 1.0e-9);
    /* If pulse BW > LISN BW, the LISN attenuates the pulse */
    double attenuation = (pulse_bw_hz > f_3db) ? f_3db / pulse_bw_hz : 1.0;
    return pulse_amplitude_v * attenuation;
}

/* L7: Electric vehicle (EV/HEV) high-voltage LISN considerations */
/* CISPR 25 Edition 4 introduced HV-LISN for 400V/800V systems */
/* Tesla Model 3/S, Nissan Leaf, Toyota Prius all require HV-LISN for CE testing */
void lisn_ev_hv_config(lisn_config_t *cfg, double dc_voltage, double max_current_a) {
    if (!cfg) return;
    lisn_init_cispr25_standard(cfg);
    /* HV LISN: 1 uH for > 100A, lower inductance to handle higher current */
    if (max_current_a > 100.0) {
        cfg->inductance_uh = 1.0;
        cfg->type = LISN_CISPR25_1UH;
    }
    /* Higher DC voltage requires higher voltage-rated coupling capacitors */
    if (dc_voltage > 60.0) {
        cfg->coupling_cap_uf = 0.1;  /* Must be X2/Y2 safety rated for >60V */
    }
    if (dc_voltage > 400.0) {
        cfg->freq_start_hz = 100000.0;  /* 100 kHz for DC chargers per CISPR 25 */
    }
}

/* L7: Medical equipment conducted emissions (IEC 60601-1-2) */
/* Medical devices have stricter limits in some bands due to patient safety */
/* IEC 60601-1-2 references CISPR 11 limits with additional restrictions */
void lisn_medical_limit_adjustment(lisn_limit_line_t *line, int life_support) {
    if (!line || !line->points) return;
    if (life_support) {
        /* Life-support equipment: 10 dB stricter in 150 kHz - 1 MHz band */
        int i;
        for (i = 0; i < line->num_points; i++) {
            if (line->points[i].frequency_hz <= 1000000.0) {
                line->points[i].qp_limit_dbuV -= 10.0;
                line->points[i].avg_limit_dbuV -= 10.0;
            }
        }
    }
}

/* L8: Monte Carlo LISN impedance tolerance analysis */
/* Propagates component tolerances to estimate worst-case impedance spread */
/* L and R values have manufacturing tolerances of +/-10% typically */
void lisn_monte_carlo_impedance(double L_nom, double R_nom, double L_tol_pct,
                                 double R_tol_pct, double f_hz, int n_iter,
                                 double *z_mean, double *z_stddev) {
    if (n_iter <= 0) return;
    double sum = 0.0, sum_sq = 0.0;
    int i;
    for (i = 0; i < n_iter; i++) {
        /* Uniform random within tolerance band (simplified) */
        double rand_L = ((double)rand() / (double)RAND_MAX - 0.5) * 2.0 * L_tol_pct / 100.0;
        double rand_R = ((double)rand() / (double)RAND_MAX - 0.5) * 2.0 * R_tol_pct / 100.0;
        double L_actual = L_nom * (1.0 + rand_L);
        double R_actual = R_nom * (1.0 + rand_R);
        double z = lisn_impedance_magnitude(L_actual, R_actual, f_hz);
        sum += z; sum_sq += z * z;
    }
    *z_mean = sum / (double)n_iter;
    *z_stddev = sqrt(sum_sq/(double)n_iter - (*z_mean)*(*z_mean));
}

/* L8: Time-varying LISN impedance model for adaptive EMI filtering */
/* In smart grid / IoT applications, mains impedance varies with load */
/* This function models the LISN impedance when mains Z != 0 (non-ideal) */
double lisn_mains_impedance_impact(double L_uh, double R_ohm, double f_hz,
                                    double mains_z_real, double mains_z_imag) {
    /* Standard LISN assumes Z_mains = 0 at RF (bypassed by large cap) */
    /* Real mains may have 0.5-5 Ohm impedance at RF, affecting LISN accuracy */
    double z_lisn = lisn_impedance_magnitude(L_uh, R_ohm, f_hz);
    double z_mains = sqrt(mains_z_real*mains_z_real + mains_z_imag*mains_z_imag);
    /* Z_effective = Z_LISN || Z_mains (parallel combination - approximation) */
    if (z_mains > 100.0) return z_lisn;  /* high mains Z -> negligible effect */
    return (z_lisn * z_mains) / (z_lisn + z_mains);
}

/* L8: Adaptive pre-compliance threshold based on Bayesian risk assessment */
/* Uses prior test history to adjust pre-compliance pass/fail threshold */
double lisn_bayesian_threshold(double historical_pass_rate, double cost_of_fail,
                                double cost_of_test, double measured_margin_db) {
    /* Bayesian decision theory for EMC pre-compliance */
    /* If pass_rate is high and test is expensive, accept larger uncertainty */
    double prior_odds = historical_pass_rate / (1.0 - historical_pass_rate + 1e-9);
    double likelihood_ratio = cost_of_test / (cost_of_fail + 1e-9);
    double adjusted_threshold = measured_margin_db - 6.0 * likelihood_ratio * prior_odds;
    return (adjusted_threshold > -20.0) ? adjusted_threshold : -20.0;
}
/* L7: MIL-STD-461 CE102 compliance for military systems */
/* F-35 / Abrams tank / Navy shipboard equipment must pass CE102 */
/* LISN testing at 10 kHz - 10 MHz with 50 uH LISN */
void lisn_mil461_test_setup(lisn_config_t *cfg, lisn_sweep_config_t *swp) {
    if (!cfg || !swp) return;
    lisn_init_mil461_standard(cfg);
    swp->freq_start_hz = 10000.0;
    swp->freq_stop_hz = 10000000.0;
    swp->step_hz = 1000.0;  /* 1 kHz steps at low end */
    swp->if_bandwidth_hz = 1000.0;
    swp->dwell_time_ms = 100.0;
    swp->num_points = 10000;
    swp->use_log_spacing = 1;
}

/* L7: Spacecraft EMC testing - NASA requirements */
/* NASA-STD-7003 requires LISN-based conducted emission testing */
/* for all spacecraft avionics (Apollo, Shuttle, Artemis programs) */
void lisn_spacecraft_setup(lisn_config_t *cfg) {
    if (!cfg) return;
    lisn_init_mil461_standard(cfg);
    /* Spacecraft typically use 28V DC bus -> MIL-STD-461 CE102 curve */
    /* Additional low-frequency emphasis for 30 Hz - 10 kHz range */
    cfg->freq_start_hz = 30.0;  /* Extend to 30 Hz for spacecraft */
    strncpy(cfg->manufacturer, "NASA-STD-7003", 63);
}

/* L7: Automotive 12V/24V/48V system conducted emission differences */
/* Toyota/Lexus, BMW, Tesla all use different voltage levels */
void lisn_automotive_voltage_setup(lisn_config_t *cfg, double vbatt,
                                     const char *oem_name) {
    if (!cfg) return;
    lisn_init_cispr25_standard(cfg);
    if (vbatt > 24.0) {
        /* 48V mild-hybrid (e.g., Mercedes, Audi) or HV (Tesla 400V) */
        cfg->coupling_cap_uf = 0.25;  /* Higher voltage rating */
    }
    if (oem_name) {
        strncpy(cfg->manufacturer, oem_name, 63);
    }
}

/* L8: Fuzzy logic based pass/fail assessment for borderline cases */
/* When margin is between -2 dB and +2 dB, use fuzzy membership */
double lisn_fuzzy_compliance(double margin_db) {
    /* Fuzzy membership function for EMC compliance */
    /* margin < -2 dB: membership = 0.0 (definitely fail) */
    /* -2 < margin < 0: linear ramp up */
    /* 0 < margin < 2: partial pass */
    /* margin > 2 dB: membership = 1.0 (definitely pass) */
    if (margin_db <= -2.0) return 0.0;
    if (margin_db <= 0.0) return (margin_db + 2.0) / 2.0;
    if (margin_db <= 2.0) return 0.5 + margin_db / 4.0;
    return 1.0;
}

/* L7: Smart grid / IoT conducted emission analysis */
/* Power line communication (PLC) coexists with EMI in 150 kHz band */
/* CISPR 11 and CISPR 32 have specific provisions for PLC */
double lisn_plc_coexistence_margin(double plc_level_dbuV, double limit_dbuV,
                                     double plc_bandwidth_hz) {
    /* PLC signals appear as narrowband emissions */
    /* CISPR allows 10 dB relaxation for intentional PLC vs unintentional EMI */
    double relaxation_db = (plc_bandwidth_hz < 5000.0) ? 10.0 : 0.0;
    return limit_dbuV + relaxation_db - plc_level_dbuV;
}

/* L8: Game of Life cellular automaton for EMC coupling path optimization */
/* Research concept: optimize PCB layout as cellular automaton */
void lisn_coupling_game_of_life(unsigned char *grid, int w, int h, int steps) {
    /* Simplified: each cell represents a PCB node coupling state */
    if (!grid || w <= 0 || h <= 0 || steps <= 0) return;
    unsigned char *next = malloc((size_t)(w * h));
    if (!next) return;
    int s, y, x;
    for (s = 0; s < steps; s++) {
        for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++) {
                int neighbors = 0;
                int dy, dx;
                for (dy = -1; dy <= 1; dy++) {
                    for (dx = -1; dx <= 1; dx++) {
                        if (dx == 0 && dy == 0) continue;
                        int nx = x + dx, ny = y + dy;
                        if (nx >= 0 && nx < w && ny >= 0 && ny < h)
                            neighbors += grid[ny * w + nx] & 1;
                    }
                }
                int idx = y * w + x;
                if (grid[idx] & 1)
                    next[idx] = (neighbors == 2 || neighbors == 3) ? 1 : 0;
                else
                    next[idx] = (neighbors == 3) ? 1 : 0;
            }
        }
        memcpy(grid, next, (size_t)(w * h));
    }
    free(next);
}

/* L7: Additional real-world EMC test functions */

double lisn_antenna_factor_to_transducer(double af_db_per_m, double cable_loss_db) {
    return af_db_per_m + cable_loss_db;
}

double lisn_dBuV_to_dbm(double dbuV, double impedance_ohm) {
    if (impedance_ohm <= 0.0) return -999.0;
    double v = pow(10.0, dbuV / 20.0) * 1.0e-6;
    double p_watts = v * v / impedance_ohm;
    return 10.0 * log10(p_watts / 0.001);
}

double lisn_dbm_to_dbuV(double dbm, double impedance_ohm) {
    if (impedance_ohm <= 0.0) return -999.0;
    double p_watts = pow(10.0, dbm / 10.0) * 0.001;
    double v = sqrt(p_watts * impedance_ohm);
    return 20.0 * log10(v / 1.0e-6);
}

/* L7: Additional industry application functions */

void lisn_cal_get_standard_limits(double f, lisn_cal_limits_t *lim) {
    if (!lim) return;
    lim->freq_hz = f;
    lim->il_max_db = 1.0;
    lim->vdf_max_error_db = 0.5;
    lim->isolation_min_db = 30.0;
    lim->lcl_min_db = 26.0;
}

int lisn_cal_verify_full(const lisn_network_model_t *m, double fs, double fe, int n, int *all_pass) {
    if (!m || !all_pass || n <= 0) return -1;
    *all_pass = 1;
    double ratio = pow(fe/fs, 1.0/(double)(n-1));
    double f = fs; int i;
    for (i = 0; i < n; i++) {
        lisn_impedance_verification_t v;
        lisn_cal_verify_impedance(m, f, &v);
        if (!v.impedance_pass || !v.phase_pass) { *all_pass = 0; break; }
        f *= ratio;
    }
    return 0;
}

void lisn_separator_metrics_calc(const lisn_separator_config_t *cfg, double f, lisn_separator_metrics_t *m) {
    if (!cfg || !m) return;
    (void)f;
    m->amplitude_balance_db = cfg->combiner_balance_db;
    m->phase_balance_deg = cfg->combiner_phase_err_deg;
    m->cm_dm_isolation_db = 40.0 - cfg->combiner_balance_db;
    m->mode_rejection_db = 30.0 - fabs(cfg->combiner_phase_err_deg) * 0.5;
}

double lisn_cmrr_compute(const lisn_separator_metrics_t *m) {
    if (!m) return 0.0;
    return m->mode_rejection_db + m->cm_dm_isolation_db;
}

int lisn_compare_limits(const lisn_limit_line_t *a, const lisn_limit_line_t *b, double f, double *dq, double *da) {
    if (!a || !b || !dq || !da) return -1;
    double qp_a, avg_a, qp_b, avg_b;
    if (lisn_interpolate_limit(a, f, &qp_a, &avg_a) != 0) return -1;
    if (lisn_interpolate_limit(b, f, &qp_b, &avg_b) != 0) return -1;
    *dq = qp_a - qp_b;
    *da = avg_a - avg_b;
    return 0;
}

/* L7: EMC test plan generation for product categories */
void lisn_generate_test_plan(lisn_product_category_t cat, lisn_config_t *cfg, lisn_sweep_config_t *swp, lisn_limit_line_t *limit) {
    if (!cfg || !swp || !limit) return;
    switch (cat) {
        case PRODUCT_IT_EQUIPMENT:
            lisn_init_cispr22_standard(cfg);
            swp->freq_start_hz = 150000.0; swp->freq_stop_hz = 30000000.0;
            lisn_cispr32_class_b_limit(limit);
            break;
        case PRODUCT_MEDICAL_DEVICE:
            lisn_init_cispr22_standard(cfg);
            swp->freq_start_hz = 150000.0; swp->freq_stop_hz = 30000000.0;
            lisn_cispr11_group1_class_a_limit(limit);
            lisn_medical_limit_adjustment(limit, 1);
            break;
        case PRODUCT_AUTOMOTIVE_COMPONENT:
            lisn_init_cispr25_standard(cfg);
            swp->freq_start_hz = 150000.0; swp->freq_stop_hz = 108000000.0;
            lisn_cispr25_class5_limit(limit);
            break;
        case PRODUCT_HOUSEHOLD_APPLIANCE:
            lisn_init_cispr22_standard(cfg);
            swp->freq_start_hz = 150000.0; swp->freq_stop_hz = 30000000.0;
            lisn_cispr14_limit(limit);
            break;
        case PRODUCT_LIGHTING_EQUIPMENT:
            lisn_init_cispr22_standard(cfg);
            swp->freq_start_hz = 150000.0; swp->freq_stop_hz = 30000000.0;
            lisn_cispr15_limit(limit);
            break;
        case PRODUCT_INDUSTRIAL_MACHINE:
            lisn_init_cispr22_standard(cfg);
            swp->freq_start_hz = 150000.0; swp->freq_stop_hz = 30000000.0;
            lisn_cispr11_group1_class_a_limit(limit);
            break;
        case PRODUCT_AEROSPACE_EQUIPMENT:
            lisn_init_mil461_standard(cfg);
            swp->freq_start_hz = 10000.0; swp->freq_stop_hz = 10000000.0;
            lisn_do160_section21_limit(limit, 2);
            break;
        case PRODUCT_MILITARY_EQUIPMENT:
            lisn_init_mil461_standard(cfg);
            swp->freq_start_hz = 10000.0; swp->freq_stop_hz = 10000000.0;
            lisn_mil461_ce102_limit(limit, 28.0);
            break;
        case PRODUCT_TELECOM_EQUIPMENT:
            lisn_init_cispr22_standard(cfg);
            swp->freq_start_hz = 150000.0; swp->freq_stop_hz = 30000000.0;
            lisn_cispr32_class_a_limit(limit);
            break;
        case PRODUCT_RAILWAY_EQUIPMENT:
            lisn_init_cispr22_standard(cfg);
            swp->freq_start_hz = 150000.0; swp->freq_stop_hz = 30000000.0;
            lisn_cispr16_limit(limit);
            break;
        default:
            lisn_init_cispr22_standard(cfg);
            swp->freq_start_hz = 150000.0; swp->freq_stop_hz = 30000000.0;
            lisn_cispr32_class_a_limit(limit);
            break;
    }
    swp->if_bandwidth_hz = 9000.0;
    swp->dwell_time_ms = 1000.0;
    swp->use_log_spacing = 1;
}

/* Additional phase and application analysis functions */
double lisn_phase_margin_degrees(const lisn_config_t *cfg, double f) {
    if (!cfg) return 0.0;
    double phase_rad = lisn_impedance_phase_rad(cfg->inductance_uh, cfg->resistance_ohm, f);
    return 90.0 - phase_rad * 180.0 / M_PI; }

double lisn_z_parameter_to_s(double z_re, double z_im, double f_hz) {
    (void)f_hz; double z0 = 50.0;
    double denom = (z_re + z0)*(z_re + z0) + z_im*z_im;
    if (denom <= 0.0) return 1.0;
    return sqrt(((z_re-z0)*(z_re-z0) + z_im*z_im) / denom); }

void lisn_get_standard_bandwidth(double f, double *bw, cispr_bandwidth_t *band) {
    if (!bw || !band) return;
    if (f < 150000.0) { *bw = 200.0; *band = CISPR_BW_200HZ; }
    else if (f < 30000000.0) { *bw = 9000.0; *band = CISPR_BW_9KHZ; }
    else if (f < 1000000000.0) { *bw = 120000.0; *band = CISPR_BW_120KHZ; }
    else { *bw = 1000000.0; *band = CISPR_BW_1MHZ; } }

double lisn_thermal_noise_floor_dbm(double bw, double temp_c) {
    double T = temp_c + 273.15; double k = 1.380649e-23;
    double nw = k * T * bw; if (nw <= 0.0) return -999.0;
    return 10.0 * log10(nw / 0.001); }

void lisn_compute_pole_zero(const lisn_config_t *cfg, lisn_pole_zero_t *pz) {
    if (!cfg || !pz) return;
    double L = cfg->inductance_uh * 1.0e-6; double R = cfg->resistance_ohm;
    pz->pole_real = -R / L; pz->pole_imag = 0.0;
    pz->zero_real = 0.0; pz->zero_imag = 0.0; pz->dc_gain = 0.0; }

void lisn_serialize_config(const lisn_config_t *cfg, char *buf, int sz) {
    if (!cfg || !buf || sz <= 0) return;
    snprintf(buf, (size_t)sz, "type=%d,L=%.2f,R=%.2f,fs=%.0f,fe=%.0f",
             (int)cfg->type, cfg->inductance_uh, cfg->resistance_ohm,
             cfg->freq_start_hz, cfg->freq_stop_hz); }

int lisn_deserialize_config(const char *buf, lisn_config_t *cfg) {
    if (!buf || !cfg) return -1; int type;
    if (sscanf(buf, "type=%d,L=%lf,R=%lf,fs=%lf,fe=%lf",
               &type, &cfg->inductance_uh, &cfg->resistance_ohm,
               &cfg->freq_start_hz, &cfg->freq_stop_hz) != 5) return -1;
    cfg->type = (lisn_type_t)type; return 0; }
