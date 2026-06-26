/**
 * @file dm_filter_model.c
 * @brief Differential-Mode Filter Model Implementation
 *
 * Implements DM filter transfer functions for all standard topologies,
 * component selection algorithms, safety capacitor design,
 * DM inductor design, and filter performance evaluation.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "cm_dm_filter.h"
#include "dm_filter_model.h"
#include "cm_choke_model.h"
#include "cm_dm_network.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_2PI
#define M_2PI (2.0 * M_PI)
#endif

/* ========================================================================
 * L2: DM LC filter parameter calculation
 *
 * Given topology and cutoff frequency, calculate L and C values.
 *
 * For LC low-pass: f_c = 1/(2π√(LC))
 *
 * Characteristic impedance: Z₀ = √(L/C)
 * Optimal Z₀ = √(R_source · R_load) for maximum power transfer
 *
 * Solving:
 *   L = Z₀ / (2π·f_c)
 *   C = 1 / (2π·f_c·Z₀)
 *
 * For π-filter (C₁-L-C₂): symmetric C₁ = C₂ = C
 *   f_c = 1/(2π√(L·2C))  (if C₁=C₂=C, effective C = 2C at ω_c)
 *   Actually: ω_c ≈ 1/√(LC · (C₁+C₂)/(C₁C₂)) → for equal C: ω_c = √2/(√(LC))
 * ======================================================================== */

int dm_lc_params_calculate(filter_topology_t topology,
                            double f_cutoff, double r_source, double r_load,
                            dm_lc_params_t *params) {
    if (params == NULL || f_cutoff <= 0.0) return -1;
    memset(params, 0, sizeof(dm_lc_params_t));

    params->topology = topology;
    if (r_source <= 0.0) r_source = 50.0;
    if (r_load <= 0.0) r_load = 50.0;

    double z0 = sqrt(r_source * r_load);
    if (z0 < 0.1) z0 = 0.1;
    if (z0 > 1000.0) z0 = 1000.0;

    double omega_c = M_2PI * f_cutoff;

    switch (topology) {
        case FILTER_TOPOLOGY_L_SERIES:
            /* L = R_load / ω_c  (RL low-pass) */
            params->l_series = r_load / omega_c;
            break;

        case FILTER_TOPOLOGY_C_SHUNT:
            /* C = 1 / (ω_c · R_source)  (RC low-pass) */
            params->c_shunt_input = 1.0 / (omega_c * r_source);
            break;

        case FILTER_TOPOLOGY_LC:
            params->l_series = z0 / omega_c;
            params->c_shunt_output = 1.0 / (omega_c * z0);
            break;

        case FILTER_TOPOLOGY_CL:
            params->c_shunt_input = 1.0 / (omega_c * z0);
            params->l_series = z0 / omega_c;
            break;

        case FILTER_TOPOLOGY_PI:
            /* Symmetric π: C-L-C */
            params->l_series = z0 / (omega_c * 0.7071);
            params->c_shunt_input = 1.0 / (omega_c * z0 * 0.7071);
            params->c_shunt_output = params->c_shunt_input;
            break;

        case FILTER_TOPOLOGY_T:
            /* Symmetric T: L-C-L */
            params->l_series = z0 / (omega_c * 1.414);
            params->c_shunt_input = 1.0 / (omega_c * z0 * 1.414);
            params->l_series2 = params->l_series;
            break;

        case FILTER_TOPOLOGY_DOUBLE_PI:
            params->l_series = z0 / (omega_c * 0.7071);
            params->c_shunt_input = 1.0 / (omega_c * z0 * 0.7071);
            params->c_shunt_output = params->c_shunt_input;
            params->l_series2 = params->l_series;
            break;

        default:
            params->l_series = z0 / omega_c;
            params->c_shunt_output = 1.0 / (omega_c * z0);
            break;
    }

    /* Damping resistor (optimal) */
    if (params->l_series > 0.0 && params->c_shunt_output > 0.0) {
        params->r_damp = sqrt(params->l_series / params->c_shunt_output);
    }

    /* Bleed resistor for X-cap: R = 1s / (C · ln(Vpk/60))
     * Safety requirement: discharge to <60V in 1 second */
    if (params->c_shunt_input > 1e-9) {
        double v_peak = 230.0 * sqrt(2.0);
        params->r_bleed = 1.0 / (params->c_shunt_input * log(v_peak / 60.0));
    }

    return 0;
}

/* ========================================================================
 * L3: DM filter transfer function H(jω) for each topology
 *
 * Series L:      H(s) = R_L / (sL + R_L)
 * Shunt C:       H(s) = 1 / (1 + s·R_S·C)
 * LC (L→series, C→shunt):  H(s) = 1 / (s²LC + sL/R_L + 1)
 * π (C₁→L→C₂):  H(s) = 1 / (1 + s²LC₁ + (R_S·C₁+R_L·C₂+LC₂/R_L)·s + ...)
 *
 * These are derived from the ABCD matrix of each topology.
 * This function evaluates H(jω) by building the ABCD matrix and
 * then computing the voltage transfer ratio.
 * ======================================================================== */

complex_t dm_transfer_function(filter_topology_t topology,
                                const dm_lc_params_t *params,
                                double freq, double r_source, double r_load) {
    complex_t h = {0.0, 0.0};
    if (params == NULL) return h;

    double omega = M_2PI * freq;

    abcd_matrix_t abcd;

    switch (topology) {
        case FILTER_TOPOLOGY_L_SERIES: {
            complex_t zL = {0.0, omega * params->l_series};
            abcd = abcd_series_impedance(zL);
            break;
        }
        case FILTER_TOPOLOGY_C_SHUNT: {
            complex_t yC = {0.0, omega * params->c_shunt_input};
            abcd = abcd_shunt_admittance(yC);
            break;
        }
        case FILTER_TOPOLOGY_LC: {
            complex_t zL = {0.0, omega * params->l_series};
            complex_t yC = {0.0, omega * params->c_shunt_output};
            abcd_matrix_t abcd_l = abcd_series_impedance(zL);
            abcd_matrix_t abcd_c = abcd_shunt_admittance(yC);
            abcd = abcd_cascade_two(&abcd_l, &abcd_c);
            break;
        }
        case FILTER_TOPOLOGY_CL: {
            complex_t zL = {0.0, omega * params->l_series};
            complex_t yC = {0.0, omega * params->c_shunt_input};
            abcd_matrix_t abcd_c = abcd_shunt_admittance(yC);
            abcd_matrix_t abcd_l = abcd_series_impedance(zL);
            abcd = abcd_cascade_two(&abcd_c, &abcd_l);
            break;
        }
        case FILTER_TOPOLOGY_PI: {
            complex_t zL = {params->r_damp ? params->r_damp : 0.0,
                            omega * params->l_series};
            complex_t yC1 = {0.0, omega * params->c_shunt_input};
            complex_t yC2 = {0.0, omega * params->c_shunt_output};
            abcd_matrix_t a1 = abcd_shunt_admittance(yC1);
            abcd_matrix_t a2 = abcd_series_impedance(zL);
            abcd_matrix_t a3 = abcd_shunt_admittance(yC2);
            abcd_matrix_t temp = abcd_cascade_two(&a1, &a2);
            abcd = abcd_cascade_two(&temp, &a3);
            break;
        }
        case FILTER_TOPOLOGY_T: {
            complex_t zL1 = {0.0, omega * params->l_series};
            complex_t zL2 = {0.0, omega * (params->l_series2 > 0.0 ?
                                  params->l_series2 : params->l_series)};
            complex_t yC = {0.0, omega * params->c_shunt_input};
            abcd_matrix_t a1 = abcd_series_impedance(zL1);
            abcd_matrix_t a2 = abcd_shunt_admittance(yC);
            abcd_matrix_t a3 = abcd_series_impedance(zL2);
            abcd_matrix_t temp = abcd_cascade_two(&a1, &a2);
            abcd = abcd_cascade_two(&temp, &a3);
            break;
        }
        default:
            /* Identity */
            abcd = (abcd_matrix_t){{1.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {1.0, 0.0}};
            break;
    }

    /* H = Z_L / (A·Z_L + B + C·R_S·Z_L + D·R_S) */
    complex_t a_zL = {abcd.A.real * r_load, abcd.A.imag * r_load};
    complex_t c_rs_zl = {abcd.C.real * r_source * r_load,
                          abcd.C.imag * r_source * r_load};
    complex_t d_rs = {abcd.D.real * r_source, abcd.D.imag * r_source};
    complex_t denom = complex_add(complex_add(a_zL, abcd.B),
                                   complex_add(c_rs_zl, d_rs));
    complex_t num = {r_load, 0.0};

    if (complex_mag(denom) > 1e-30) {
        h = complex_div(num, denom);
    }

    return h;
}

/* ========================================================================
 * L2: DM Insertion Loss per CISPR 17
 *
 * CISPR 17 defines standardized measurement conditions.
 * We compute IL for each specified impedance combination.
 * ======================================================================== */

double dm_insertion_loss_cispr(const dm_lc_params_t *params,
                                cispr17_config_t config, double freq) {
    if (params == NULL) return 0.0;

    complex_t h = dm_transfer_function(params->topology, params,
                                        freq, config.z_source, config.z_load);

    /* IL = -20·log10(|H|) when comparing to direct connection
     * Actually: IL = 20·log10(|V_without|/|V_with|)
     * V_without = Z_L / (Z_S + Z_L)
     * V_with = |H| (since H = V_out/V_in, and V_in = V_S · Z_in/(Z_S+Z_in))
     *
     * Simplified: IL = 20·log10( (Z_S+Z_L)/Z_L · 1/|H| )
     */
    double v_ratio = (config.z_source + config.z_load) / config.z_load;
    double mag_h = complex_mag(h);
    if (mag_h < 1e-30) return 100.0;

    double il = 20.0 * log10(v_ratio / mag_h);
    return il > 0.0 ? il : 0.0;
}

double dm_il_worst_case(const dm_lc_params_t *params, double freq,
                         cispr17_impedance_t *worst_cond) {
    if (params == NULL) return 0.0;

    double conditions[3][2] = {{50.0, 50.0}, {0.1, 100.0}, {100.0, 0.1}};
    double worst_il = DBL_MAX;

    for (int c = 0; c < 3; c++) {
        cispr17_config_t cfg;
        cfg.condition = (cispr17_impedance_t)c;
        cfg.z_source = conditions[c][0];
        cfg.z_load = conditions[c][1];
        double il = dm_insertion_loss_cispr(params, cfg, freq);
        if (il < worst_il) {
            worst_il = il;
            if (worst_cond) *worst_cond = (cispr17_impedance_t)c;
        }
    }

    return worst_il;
}

/* ========================================================================
 * L1-L2: Safety capacitor creation and leakage current
 *
 * Y-capacitor safety is critical: they connect from line to protective
 * earth. If a Y-cap fails short, the enclosure becomes live.
 *
 * Y-capacitors use fail-open construction:
 *  - Y1: double-insulated, 8 kV impulse
 *  - Y2: basic insulation, 5 kV impulse
 *
 * Leakage current limits (IEC 62368-1):
 *  - Fixed ITE equipment: ≤ 3.5 mA
 *  - Portable ITE: ≤ 0.75 mA
 *  - Medical (IEC 60601): ≤ 0.5 mA (normal), ≤ 0.1 mA (SFC)
 * ======================================================================== */

safety_capacitor_t *safety_capacitor_create(filter_elem_type_t type,
                                             double capacitance,
                                             x_cap_class_t *x_class,
                                             y_cap_class_t *y_class,
                                             double voltage) {
    if (type != FILTER_ELEM_X_CAPACITOR && type != FILTER_ELEM_Y_CAPACITOR) {
        return NULL;
    }
    if (capacitance <= 0.0) return NULL;

    safety_capacitor_t *cap = (safety_capacitor_t *)malloc(sizeof(safety_capacitor_t));
    if (cap == NULL) return NULL;

    memset(cap, 0, sizeof(safety_capacitor_t));
    cap->base.type = type;
    cap->base.nominal_value = capacitance;
    cap->base.rated_voltage = voltage;

    if (type == FILTER_ELEM_X_CAPACITOR && x_class) {
        cap->x_class = *x_class;
        switch (*x_class) {
            case X_CLASS_X1: cap->impulse_voltage = 4000.0; break;
            case X_CLASS_X2: cap->impulse_voltage = 2500.0; break;
            case X_CLASS_X3: cap->impulse_voltage = 0.0; break;
        }
    }

    if (type == FILTER_ELEM_Y_CAPACITOR && y_class) {
        cap->y_class = *y_class;
        switch (*y_class) {
            case Y_CLASS_Y1: cap->impulse_voltage = 8000.0; break;
            case Y_CLASS_Y2: cap->impulse_voltage = 5000.0; break;
            case Y_CLASS_Y3: cap->impulse_voltage = 0.0; break;
            case Y_CLASS_Y4: cap->impulse_voltage = 2500.0; break;
        }
    }

    /* Discharge time constant: R·C */
    double r_bleed = 1e6;
    cap->discharge_constant = r_bleed * capacitance;

    return cap;
}

double y_cap_leakage_current(double capacitance, double freq,
                              double voltage_rms) {
    return M_2PI * freq * capacitance * voltage_rms;
}

double y_cap_max_for_leakage(double i_leak_max_ma, double freq,
                              double voltage_rms) {
    double i_max = i_leak_max_ma / 1000.0;  /* mA → A */
    double c_max = i_max / (M_2PI * freq * voltage_rms);
    return c_max;
}

void safety_capacitor_free(safety_capacitor_t *cap) {
    free(cap);
}

/* ========================================================================
 * L5: DM inductor design from specification
 *
 * Using the area product method (McLyman §7):
 *
 * 1. Calculate required inductance L
 * 2. Energy storage: E = ½·L·I²_pk
 * 3. Area product: A_p = (2·E·10⁴) / (K_u·B_max·J·K_f)  [cm⁴]
 *    where K_u = window utilization (0.4 for toroid)
 *          B_max = maximum flux density [T]
 *          J = current density [A/cm²]
 *          K_f = waveform factor (4.44 for sine, 4.0 for square)
 * 4. Select core with A_p ≥ calculated
 * 5. Turns: N = L·I_pk·10⁴ / (B_max·A_e)  [A_e in cm²]
 * 6. Wire gauge: A_w = I_rms / J
 * 7. Check window fill: N·A_w_insulated ≤ K_u·W_a
 * 8. Calculate losses and temperature rise
 * ======================================================================== */

dm_inductor_design_t *dm_inductor_design(const dm_inductor_spec_t *spec) {
    if (spec == NULL || spec->inductance_target <= 0.0) return NULL;

    dm_inductor_design_t *design = (dm_inductor_design_t *)
        malloc(sizeof(dm_inductor_design_t));
    if (design == NULL) return NULL;

    memset(design, 0, sizeof(dm_inductor_design_t));
    design->achieved_inductance = spec->inductance_target;

    /* Select core material */
    core_material_t *mat = core_material_lookup(spec->material);
    if (mat == NULL) mat = core_material_lookup(CORE_MAT_N87);

    /* Use a toroid core */
    double od_est = 0.020;  /* 20 mm OD */
    double id_est = 0.012;  /* 12 mm ID */
    double ht_est = 0.008;  /* 8 mm height */
    design->core.shape = CORE_SHAPE_TOROID;
    design->core.geometry = core_geom_calculate(CORE_SHAPE_TOROID,
        od_est, id_est, ht_est, mat->mu_i);
    design->core.material = *mat;
    design->core.mat_id = spec->material;

    /* Energy storage */
    double i_pk = spec->peak_current > 0.0 ? spec->peak_current : spec->rated_current_rms * 1.414;
    double energy = 0.5 * spec->inductance_target * i_pk * i_pk;

    /* Area product */
    double b_max = mat->b_sat / 2.0;  /* 50% of saturation */
    double j_a_m2 = spec->current_density_amp_mm2 > 0.0 ?
                    spec->current_density_amp_mm2 * 1e6 : 3.5e6;  /* [A/m²] */
    double ku = 0.4;
    double ap = (2.0 * energy) / (ku * b_max * j_a_m2 * 1.0);

    /* Scale core to meet A_p */
    double ap_cm4 = ap * 1e4;
    /* Adjust core dimensions */
    if (ap_cm4 > design->core.geometry.ae * 1e4 * design->core.geometry.le * 1e2) {
        double scale = pow(ap_cm4 / (design->core.geometry.ae * 1e4 *
                          design->core.geometry.le * 1e2), 0.333);
        if (scale > 1.0) {
            od_est *= scale;
            id_est *= scale;
            ht_est *= scale;
            design->core.geometry = core_geom_calculate(CORE_SHAPE_TOROID,
                od_est, id_est, ht_est, mat->mu_i);
        }
    }

    /* Calculate turns */
    double n_turns = spec->inductance_target * i_pk / (b_max * design->core.geometry.ae);
    if (n_turns < 1.0) n_turns = 1.0;
    if (n_turns > 200.0) n_turns = 200.0;

    /* Wire gauge: area = I_rms / J */
    double a_wire = spec->rated_current_rms / j_a_m2;
    double d_wire = 2.0 * sqrt(a_wire / M_PI);
    if (d_wire < 0.05e-3) d_wire = 0.05e-3;

    /* Winding parameters */
    design->winding.num_turns = n_turns;
    design->winding.wire_diameter = d_wire;
    design->winding.wire_diameter_insulated = d_wire * 1.15;
    design->winding.wire_resistivity = 1.68e-8;
    design->winding.arrangement = WINDING_SINGLE_LAYER;
    design->winding.num_layers = 1;

    /* Window area estimation */
    design->core.window_area = M_PI * (id_est / 2.0) * (id_est / 2.0);
    design->core.window_width = id_est;
    design->core.surface_area = M_PI * od_est * ht_est
                               + M_PI * (od_est * od_est - id_est * id_est) / 2.0;

    /* DC resistance */
    double mlt = M_PI * (od_est + id_est) / 2.0;
    design->dc_resistance = 1.68e-8 * n_turns * mlt / (M_PI * d_wire * d_wire / 4.0);

    /* AC resistance at ripple frequency */
    design->ac_resistance = design->dc_resistance *
        ac_dc_resistance_ratio(d_wire, spec->ripple_freq, 1, 0.7);

    /* Saturation current (70% inductance drop) */
    design->saturation_current = b_max * design->core.geometry.le /
        (4.0e-7 * M_PI * mat->mu_i * n_turns);

    /* Loss estimates */
    design->copper_loss_w = spec->rated_current_rms * spec->rated_current_rms
                            * design->dc_resistance;
    design->core_loss_w = 0.1 * design->copper_loss_w;  /* Simplified */
    design->total_loss_w = design->copper_loss_w + design->core_loss_w;

    /* Temperature rise */
    double r_th = 34.0;  /* K/W typical small toroid */
    design->temperature_rise_k = design->total_loss_w * r_th;

    /* Volume */
    design->volume_cm3 = M_PI * od_est * od_est * ht_est * 1e6;

    free(mat);
    return design;
}

void dm_inductor_design_free(dm_inductor_design_t *design) {
    free(design);
}

/* ========================================================================
 * L3: DM filter input and output impedance
 *
 * For stability analysis (Middlebrook criterion), we need Z_in(s)
 * and Z_out(s) of the filter.
 *
 * Z_in = V₁/I₁ with the output terminated in Z_L
 * Z_out = V₂/(-I₂) with the input terminated in Z_S
 *
 * From ABCD parameters:
 *   Z_in = (A·Z_L + B) / (C·Z_L + D)
 *   Z_out = (D·Z_S + B) / (C·Z_S + A)
 * ======================================================================== */

void dm_filter_impedances(filter_topology_t topology,
                           const dm_lc_params_t *params,
                           double freq, double r_load,
                           complex_t *z_in, complex_t *z_out) {
    if (params == NULL) return;

    /* Build ABCD matrix */
    abcd_matrix_t abcd;
    memset(&abcd, 0, sizeof(abcd));

    double omega = M_2PI * freq;

    switch (topology) {
        case FILTER_TOPOLOGY_LC: {
            complex_t zL = {0.0, omega * params->l_series};
            complex_t yC = {0.0, omega * params->c_shunt_output};
            abcd_matrix_t abcd_l = abcd_series_impedance(zL);
            abcd_matrix_t abcd_c = abcd_shunt_admittance(yC);
            abcd = abcd_cascade_two(&abcd_l, &abcd_c);
            break;
        }
        case FILTER_TOPOLOGY_PI: {
            complex_t zL = {params->r_damp, omega * params->l_series};
            complex_t yC1 = {0.0, omega * params->c_shunt_input};
            complex_t yC2 = {0.0, omega * params->c_shunt_output};
            abcd_matrix_t a1 = abcd_shunt_admittance(yC1);
            abcd_matrix_t a2 = abcd_series_impedance(zL);
            abcd_matrix_t a3 = abcd_shunt_admittance(yC2);
            abcd_matrix_t t = abcd_cascade_two(&a1, &a2);
            abcd = abcd_cascade_two(&t, &a3);
            break;
        }
        default:
            if (z_in) *z_in = (complex_t){r_load, 0.0};
            if (z_out) *z_out = (complex_t){0.0, 0.0};
            return;
    }

    /* Z_in = (A·Z_L + B) / (C·Z_L + D) */
    if (z_in) {
        complex_t num_in = complex_add(
            (complex_t){abcd.A.real * r_load, abcd.A.imag * r_load},
            abcd.B);
        complex_t denom_in = complex_add(
            (complex_t){abcd.C.real * r_load, abcd.C.imag * r_load},
            abcd.D);
        if (complex_mag(denom_in) > 1e-30) {
            *z_in = complex_div(num_in, denom_in);
        } else {
            *z_in = (complex_t){1e9, 0.0};
        }
    }

    /* Z_out = (D·Z_S + B) / (C·Z_S + A)
     * Z_S is the source impedance (assume 50Ω for output impedance) */
    if (z_out) {
        double r_source = 50.0;
        complex_t num_out = complex_add(
            (complex_t){abcd.D.real * r_source, abcd.D.imag * r_source},
            abcd.B);
        complex_t denom_out = complex_add(
            (complex_t){abcd.C.real * r_source, abcd.C.imag * r_source},
            abcd.A);
        if (complex_mag(denom_out) > 1e-30) {
            *z_out = complex_div(num_out, denom_out);
        } else {
            *z_out = (complex_t){1e9, 0.0};
        }
    }
}

/* ========================================================================
 * L6: Comprehensive DM filter performance evaluation
 * ======================================================================== */

dm_filter_perf_t dm_filter_evaluate(const dm_lc_params_t *params,
                                     double freq,
                                     double r_source, double r_load) {
    dm_filter_perf_t perf;
    memset(&perf, 0, sizeof(perf));
    perf.frequency = freq;

    if (params == NULL) return perf;

    /* Transfer function */
    complex_t h = dm_transfer_function(params->topology, params, freq,
                                        r_source, r_load);
    perf.voltage_attenuation = complex_mag(h);

    /* Insertion loss */
    cispr17_config_t cfg;
    cfg.condition = CISPR17_CUSTOM;
    cfg.z_source = r_source;
    cfg.z_load = r_load;
    perf.insertion_loss_db = dm_insertion_loss_cispr(params, cfg, freq);

    /* Impedances */
    dm_filter_impedances(params->topology, params, freq, r_load,
                         &perf.z_filter_input, &perf.z_filter_output);

    /* Power loss */
    double i_load = 1.0;  /* Assume 1A for loss calculation */
    if (params->l_series > 0.0) {
        perf.power_loss_w = i_load * i_load * 0.1;  /* Simplified */
    }

    return perf;
}

/* ========================================================================
 * L2: X-capacitor bleed resistor calculation
 *
 * Safety requirement per IEC 62368-1:
 * After disconnection from mains, the voltage across an X-capacitor
 * must decay to <60V within 1 second.
 *
 * V(t) = V_initial · exp(-t/RC)
 *
 * Therefore: R ≤ t / (C · ln(V_initial / 60))
 *
 * For 230V_rms mains: V_peak = 325V
 *   R ≤ 1.0 / (C · ln(325/60)) = 1.0 / (C · 1.69)
 *   R ≤ 0.591 / C
 *
 * For 1 µF X-cap: R ≤ 591 kΩ
 * Standard bleed resistor: 470 kΩ or 330 kΩ
 * ======================================================================== */

double x_cap_bleed_resistor(double capacitance, double v_peak,
                             double time_sec) {
    if (capacitance <= 0.0 || v_peak <= 0.0 || time_sec <= 0.0) {
        return DBL_MAX;
    }

    double safe_voltage = 60.0;  /* IEC 62368-1 safe touch voltage */
    if (v_peak <= safe_voltage) return DBL_MAX;

    return time_sec / (capacitance * log(v_peak / safe_voltage));
}

/* ========================================================================
 * L6: DM filter component stress
 *
 * Ensures selected components can handle worst-case voltage and current.
 *
 * Capacitor voltage stress:
 *   V_c_max = V_in_max + ripple contribution
 *   For π-filter input cap: V_c1 = V_in · (1 + Q)
 *
 * Inductor current stress:
 *   I_L_max = I_load_max + ripple current
 *   For LC filter: ΔI_L ≈ V_in / (2·f_sw·L)
 * ======================================================================== */

void dm_component_stress(const dm_lc_params_t *params,
                          double v_in_rms, double i_load_rms,
                          double *v_c_max, double *i_l_max) {
    if (params == NULL) return;

    double v_pk = v_in_rms * sqrt(2.0) * 1.2;  /* +20% tolerance */
    double i_pk = i_load_rms * sqrt(2.0) * 1.3; /* +30% overload */

    if (v_c_max) *v_c_max = v_pk;
    if (i_l_max) *i_l_max = i_pk;

    /* Add ripple for worst-case estimation */
    if (params->l_series > 0.0 && i_l_max) {
        /* Triangular ripple: ΔI = V_in·D/(f_sw·L)
         * Assume 50% duty, 65 kHz switching */
        double f_sw = 65e3;
        double delta_i = v_pk * 0.5 / (f_sw * params->l_series);
        *i_l_max = i_pk + delta_i;
    }

    if (params->c_shunt_output > 0.0 && v_c_max) {
        /* Voltage ripple: ΔV = I_load/(2·f_sw·C) */
        double f_sw = 65e3;
        double delta_v = i_pk / (2.0 * f_sw * params->c_shunt_output);
        *v_c_max = v_pk + delta_v;
    }
}
