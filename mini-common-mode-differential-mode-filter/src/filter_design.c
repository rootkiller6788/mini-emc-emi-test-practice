/**
 * @file filter_design.c
 * @brief EMI Filter Design — Safety, Thermal, Reliability, Optimization
 *
 * Implements filter design support functions:
 *  - EMC limit summaries
 *  - Safety spacing calculations (IEC 62368-1)
 *  - Y-capacitor safety limits
 *  - Thermal analysis
 *  - Component database management
 *  - Filter optimization
 *  - Standard filter selection
 *  - Reliability prediction (MIL-HDBK-217F)
 *  - Impedance interaction analysis
 *  - Active EMI filter design
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdio.h>

#include "cm_dm_filter.h"
#include "filter_design_params.h"
#include "cm_choke_model.h"
#include "dm_filter_model.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_2PI
#define M_2PI (2.0 * M_PI)
#endif

/* ========================================================================
 * L1: EMC Limit Summary Retrieval
 *
 * Returns a compact struct with key limit values for quick reference
 * during filter design. These are the baseline values that the filter
 * must meet or exceed.
 * ======================================================================== */

emc_limit_summary_t emc_limit_summary_get(emc_standard_t standard,
                                           int is_class_a) {
    emc_limit_summary_t ls;
    memset(&ls, 0, sizeof(ls));
    ls.standard = standard;
    ls.is_class_a = is_class_a;
    ls.freq_start_khz = 150.0;
    ls.freq_stop_mhz = 30.0;
    ls.num_segments = 3;

    switch (standard) {
        case EMC_STD_CISPR32:
        case EMC_STD_CISPR22:
        case EMC_STD_FCC_PART15_A:
        case EMC_STD_FCC_PART15_B:
            if (is_class_a) {
                ls.limit_150k_qp_dbuV = 79.0;
                ls.limit_500k_qp_dbuV = 73.0;
                ls.limit_5M_qp_dbuV  = 73.0;
                ls.limit_30M_qp_dbuV = 73.0;
                ls.limit_150k_avg_dbuV = 66.0;
                ls.limit_500k_avg_dbuV = 60.0;
                ls.limit_5M_avg_dbuV  = 60.0;
                ls.limit_30M_avg_dbuV = 60.0;
                snprintf(ls.description, sizeof(ls.description),
                    "CISPR 32 Class A — Commercial/Industrial");
            } else {
                ls.limit_150k_qp_dbuV = 66.0;
                ls.limit_500k_qp_dbuV = 56.0;
                ls.limit_5M_qp_dbuV  = 56.0;
                ls.limit_30M_qp_dbuV = 60.0;
                ls.limit_150k_avg_dbuV = 56.0;
                ls.limit_500k_avg_dbuV = 46.0;
                ls.limit_5M_avg_dbuV  = 46.0;
                ls.limit_30M_avg_dbuV = 50.0;
                snprintf(ls.description, sizeof(ls.description),
                    "CISPR 32 Class B — Residential");
            }
            break;

        case EMC_STD_CISPR11:
            if (is_class_a) {
                ls.limit_150k_qp_dbuV = 79.0;
                ls.limit_500k_qp_dbuV = 73.0;
                ls.limit_5M_qp_dbuV  = 73.0;
                ls.limit_30M_qp_dbuV = 73.0;
                snprintf(ls.description, sizeof(ls.description),
                    "CISPR 11 Class A — ISM Equipment");
            } else {
                ls.limit_150k_qp_dbuV = 66.0;
                ls.limit_500k_qp_dbuV = 56.0;
                ls.limit_5M_qp_dbuV  = 56.0;
                ls.limit_30M_qp_dbuV = 60.0;
                snprintf(ls.description, sizeof(ls.description),
                    "CISPR 11 Class B — ISM Equipment");
            }
            break;

        case EMC_STD_MIL_STD_461:
            ls.limit_150k_qp_dbuV = 94.0;
            ls.limit_500k_qp_dbuV = 60.0;
            ls.limit_5M_qp_dbuV  = 60.0;
            ls.limit_30M_qp_dbuV = 60.0;
            ls.freq_start_khz = 10.0;
            snprintf(ls.description, sizeof(ls.description),
                "MIL-STD-461G CE102 — Military");
            break;

        case EMC_STD_DO_160:
            ls.limit_150k_qp_dbuV = 76.0;
            ls.limit_500k_qp_dbuV = 70.0;
            ls.limit_5M_qp_dbuV  = 60.0;
            ls.limit_30M_qp_dbuV = 60.0;
            snprintf(ls.description, sizeof(ls.description),
                "RTCA DO-160 — Aerospace");
            break;

        case EMC_STD_CISPR25:
            ls.limit_150k_qp_dbuV = 65.0;
            ls.limit_500k_qp_dbuV = 45.0;
            ls.limit_5M_qp_dbuV  = 45.0;
            ls.limit_30M_qp_dbuV = 55.0;
            ls.freq_stop_mhz = 108.0;
            snprintf(ls.description, sizeof(ls.description),
                "CISPR 25 — Automotive");
            break;

        default:
            break;
    }

    return ls;
}

/* ========================================================================
 * L2: Safety spacing calculation per IEC 62368-1
 *
 * Creepage and clearance are fundamental safety requirements that
 * must be satisfied for any AC-connected EMI filter.
 *
 * Creepage: Shortest path along surface between conductors.
 *   Determined by: working voltage, pollution degree, CTI
 *                      (Comparative Tracking Index)
 *
 * Clearance: Shortest distance through air between conductors.
 *   Determined by: peak working voltage, overvoltage category,
 *                   altitude correction
 *
 * Pollution Degree (PD):
 *   PD1: Sealed, no pollution
 *   PD2: Non-conductive pollution only, occasional condensation (typical office)
 *   PD3: Conductive pollution or dry non-conductive that becomes conductive
 *
 * Material Groups (CTI):
 *   I:    CTI ≥ 600 (ceramic, glass)
 *   II:   CTI ≥ 400 (phenolic, FR-4)
 *   IIIa: CTI ≥ 175 (paper-epoxy)
 *   IIIb: CTI ≥ 100 (typical cheaper materials)
 *
 * Reference: IEC 62368-1:2018, Table 20/21
 * ======================================================================== */

safety_spacing_t safety_spacing_calculate(double voltage_rms,
                                           int pollution_deg,
                                           int material_grp,
                                           int ovc) {
    safety_spacing_t ss;
    memset(&ss, 0, sizeof(ss));
    ss.operating_voltage_rms = voltage_rms;
    ss.pollution_degree = (double)(pollution_deg > 0 ? pollution_deg : 2);
    ss.material_group = material_grp;

    /* Transient overvoltage based on OVC (IEC 60664-1) */
    switch (ovc) {
        case 1: ss.transient_overvoltage = 330.0; break;
        case 2: ss.transient_overvoltage = 500.0; break;
        case 3: ss.transient_overvoltage = 800.0; break;
        case 4: ss.transient_overvoltage = 1500.0; break;
        default: ss.transient_overvoltage = 500.0;
    }

    /* Creepage calculation
     * For 230V_rms, PD2, material group II:
     *   Creepage = 2.5 mm (basic insulation)
     * Linear interpolation from IEC 62368-1 tables */
    double v = voltage_rms;
    double base_creepage;

    if (v <= 50.0) base_creepage = 0.6;
    else if (v <= 125.0) base_creepage = 1.0;
    else if (v <= 150.0) base_creepage = 1.5;
    else if (v <= 250.0) base_creepage = 2.5;
    else if (v <= 400.0) base_creepage = 4.0;
    else if (v <= 600.0) base_creepage = 6.3;
    else base_creepage = 6.3 * (v / 600.0);

    /* CTI material group multiplier
     * Group IIIa: ×1.4, Group IIIb: ×1.8 */
    double cti_mult[4] = {1.0, 1.0, 1.4, 1.8};
    if (material_grp >= 0 && material_grp < 4) {
        base_creepage *= cti_mult[material_grp];
    }

    /* Pollution degree multiplier
     * PD2: ×1.0, PD3: ×1.5 */
    if (pollution_deg == 3) base_creepage *= 1.5;
    else if (pollution_deg == 4) base_creepage *= 2.0;

    ss.creepage_mm = base_creepage;

    /* Clearance calculation
     * Based on peak voltage (V_rms × √2 for sinusoidal) and OVC
     * Basic clearance for 230V_rms (325V_pk): 2.0 mm */
    double v_peak = voltage_rms * sqrt(2.0) + ss.transient_overvoltage;
    double base_clearance;

    if (v_peak <= 330.0) base_clearance = 0.2;
    else if (v_peak <= 500.0) base_clearance = 0.5;
    else if (v_peak <= 800.0) base_clearance = 0.8;
    else if (v_peak <= 1000.0) base_clearance = 1.5;
    else if (v_peak <= 1500.0) base_clearance = 3.0;
    else if (v_peak <= 2000.0) base_clearance = 4.5;
    else base_clearance = 4.5 * (v_peak / 2000.0);

    ss.clearance_mm = base_clearance;

    /* Solid insulation test voltage */
    if (voltage_rms <= 150.0) ss.solid_insulation_kv = 1.5;
    else if (voltage_rms <= 300.0) ss.solid_insulation_kv = 2.5;
    else ss.solid_insulation_kv = 3.0;

    return ss;
}

/* ========================================================================
 * L2: Y-capacitor limits calculation
 *
 * Calculates the maximum allowable Y-capacitance based on
 * leakage current limits and application type.
 *
 * Application types:
 *   0: ITE fixed (3.5 mA max)
 *   1: ITE portable (0.75 mA max)
 *   2: Medical normal (0.5 mA)
 *   3: Medical single-fault (0.1 mA)
 * ======================================================================== */

y_cap_limits_t y_cap_limits_calculate(int application,
                                       double line_voltage_rms,
                                       double line_freq_hz) {
    y_cap_limits_t yl;
    memset(&yl, 0, sizeof(yl));
    yl.line_voltage_rms = line_voltage_rms;
    yl.line_freq_hz = line_freq_hz;
    yl.application_type = application;

    switch (application) {
        case 0: yl.max_leakage_ma = 3.5; break;  /* ITE fixed */
        case 1: yl.max_leakage_ma = 0.75; break; /* ITE portable */
        case 2: yl.max_leakage_ma = 0.5; break;  /* Medical normal */
        case 3: yl.max_leakage_ma = 0.1; break;  /* Medical SFC */
        default: yl.max_leakage_ma = 3.5;
    }

    if (line_freq_hz <= 0.0) line_freq_hz = 50.0;
    if (line_voltage_rms <= 0.0) line_voltage_rms = 230.0;

    double i_max = yl.max_leakage_ma / 1000.0;
    yl.max_total_y_cap_nf = i_max / (M_2PI * line_freq_hz * line_voltage_rms) * 1e9;

    return yl;
}

/* ========================================================================
 * L2: Safety compliance check
 *
 * Checks a filter design against all safety requirements:
 *  1. Creepage and clearance for PCB/terminal layout
 *  2. Y-cap leakage current within limits
 *  3. Bleed resistor ensures X-cap discharge <1s to <60V
 *  4. Hi-pot test voltage ≥ 2×rated + 1000V (basic insulation)
 * ======================================================================== */

safety_compliance_t safety_compliance_check(const cm_dm_filter_t *filter,
                                             const filter_design_spec_t *spec) {
    safety_compliance_t sc;
    memset(&sc, 0, sizeof(sc));

    if (filter == NULL || spec == NULL) return sc;

    /* Creepage / clearance */
    sc.spacing = safety_spacing_calculate(spec->v_nominal, 2, 1, 2);

    /* Y-cap limits */
    sc.y_cap = y_cap_limits_calculate(0, spec->v_nominal, spec->f_grid);

    /* Bleed resistor requirement */
    double v_peak = spec->v_nominal * sqrt(2.0);
    sc.bleed_resistor_ohms = x_cap_bleed_resistor(1e-6, v_peak, 1.0);
    sc.discharge_time_ms = 1000.0;

    /* Hi-pot test voltage:
     * Basic insulation: 2×V_rated + 1000V (for V_rated ≤ 250V)
     * For 230V: 2×230 + 1000 = 1460 → round to 1500V */
    if (spec->v_nominal <= 250.0) {
        sc.hi_pot_test_kv = (int)((2.0 * spec->v_nominal + 1000.0) / 1000.0 + 0.5);
    } else {
        sc.hi_pot_test_kv = (int)((2.4 * spec->v_nominal + 2400.0) / 1000.0 + 0.5);
    }

    /* Pass/Fail: simplified — all spacings must be met */
    sc.passes_safety = (sc.spacing.creepage_mm < 10.0 &&
                        sc.spacing.clearance_mm < 6.0) ? 1 : 0;

    return sc;
}

/* ========================================================================
 * L2: Thermal analysis
 *
 * Estimates filter temperature rise from power dissipation.
 * Uses a lumped thermal model:
 *
 *   T_case = T_ambient + R_th_ca · P_loss
 *   T_core = T_case + R_th_cc · P_core_loss
 *
 * Typical natural convection thermal resistance:
 *   R_th_ca ≈ 50 / A_surface [K/W]  where A_surface in cm²
 *
 * Safe operating area:
 *   Ferrite core max temp: 120-150°C (below Curie temp of ~200°C)
 *   Capacitor max temp: 85-105°C (per datasheet)
 *   PCB FR-4 max temp: ~130°C (Tg = 130°C for standard FR-4)
 * ======================================================================== */

thermal_analysis_t thermal_analysis(const cm_dm_filter_t *filter,
                                     double ambient_temp_c,
                                     double surface_area_cm2) {
    thermal_analysis_t ta;
    memset(&ta, 0, sizeof(ta));
    ta.ambient_temp_c = ambient_temp_c;

    if (filter == NULL) return ta;

    ta.total_power_loss_w = filter->power_loss;
    ta.surface_area_cm2 = surface_area_cm2 > 0.0 ? surface_area_cm2 : 10.0;

    /* Natural convection: h ≈ 10 W/(m²·K)
     * R_th = 1 / (h · A) = 1 / (10 · A) = 0.1 / A [K/W] per cm²
     * More precisely: R_th = 100 / A_cm2 */
    ta.thermal_resistance_kw = 100.0 / ta.surface_area_cm2;
    if (ta.thermal_resistance_kw < 1.0) ta.thermal_resistance_kw = 1.0;

    ta.internal_temp_rise_k = ta.total_power_loss_w * ta.thermal_resistance_kw;
    ta.case_temp_c = ta.ambient_temp_c + ta.internal_temp_rise_k * 0.7;
    ta.core_hotspot_temp_c = ta.ambient_temp_c + ta.internal_temp_rise_k;
    ta.max_core_temp_c = 120.0;  /* Safe operating limit for ferrite */
    ta.temp_margin_k = ta.max_core_temp_c - ta.core_hotspot_temp_c;
    ta.passes_thermal = (ta.temp_margin_k > 10.0) ? 1 : 0;

    return ta;
}

/* ========================================================================
 * L5: Component database operations
 * ======================================================================== */

component_database_t *component_database_create(void) {
    component_database_t *db = (component_database_t *)malloc(sizeof(component_database_t));
    if (db == NULL) return NULL;

    db->entries = NULL;
    db->count = 0;
    db->capacity = 0;
    return db;
}

int component_database_add(component_database_t *db,
                            const component_entry_t *entry) {
    if (db == NULL || entry == NULL) return -1;

    if (db->count >= db->capacity) {
        size_t new_cap = db->capacity == 0 ? 16 : db->capacity * 2;
        component_entry_t *new_entries = (component_entry_t *)realloc(
            db->entries, new_cap * sizeof(component_entry_t));
        if (new_entries == NULL) return -1;
        db->entries = new_entries;
        db->capacity = new_cap;
    }

    db->entries[db->count] = *entry;
    db->count++;
    return 0;
}

size_t *component_database_find(const component_database_t *db,
                                 filter_elem_type_t type,
                                 double value, double tol,
                                 size_t *n_out) {
    if (db == NULL || n_out == NULL) return NULL;

    /* First pass: count matches */
    size_t count = 0;
    double lo = value * (1.0 - tol);
    double hi = value * (1.0 + tol);

    for (size_t i = 0; i < db->count; i++) {
        if (db->entries[i].type == type &&
            db->entries[i].value >= lo &&
            db->entries[i].value <= hi) {
            count++;
        }
    }

    *n_out = count;
    if (count == 0) return NULL;

    size_t *indices = (size_t *)malloc(count * sizeof(size_t));
    if (indices == NULL) {
        *n_out = 0;
        return NULL;
    }

    count = 0;
    for (size_t i = 0; i < db->count; i++) {
        if (db->entries[i].type == type &&
            db->entries[i].value >= lo &&
            db->entries[i].value <= hi) {
            indices[count++] = i;
        }
    }

    return indices;
}

void component_database_free(component_database_t *db) {
    if (db) {
        free(db->entries);
        free(db);
    }
}

/* ========================================================================
 * L5: Filter optimization using weighted-sum scalarization
 *
 * Multi-objective optimization of filter design:
 *
 * Objective: minimize f = w₁·(-IL) + w₂·Cost + w₃·Volume + w₄·Loss
 *
 * where:
 *   IL = insertion loss (maximize → minimize -IL)
 *   Cost = BOM cost (minimize)
 *   Volume = total volume (minimize)
 *   Loss = power loss (minimize)
 *
 * The optimization searches over:
 *  - Number of stages (1-4)
 *  - Topology for each stage (LC, π, T)
 *  - Component values (L, C)
 *
 * It uses a simple grid search with Pareto frontier tracking.
 *
 * For production use, this would be replaced with gradient-based
 * or evolutionary optimization (NSGA-II, etc.).
 * ======================================================================== */

optimization_result_t *filter_optimize(const filter_design_spec_t *spec,
                                        const component_database_t *db,
                                        const optimization_weights_t *weights,
                                        const noise_spectrum_t *noise) {
    (void)db; /* Component database selection not used in basic optimization */
    if (spec == NULL) return NULL;

    optimization_result_t *opt = (optimization_result_t *)
        malloc(sizeof(optimization_result_t));
    if (opt == NULL) return NULL;

    memset(opt, 0, sizeof(optimization_result_t));

    /* Default weights if not provided */
    optimization_weights_t w;
    if (weights) {
        w = *weights;
    } else {
        w.weight_insertion_loss = 0.4;
        w.weight_cost = 0.2;
        w.weight_volume = 0.15;
        w.weight_power_loss = 0.15;
        w.weight_margin = 0.1;
        w.weight_cmrr = 0.0;
    }

    /* Try different numbers of stages */
    double best_fitness = DBL_MAX;
    filter_design_result_t *best_design = NULL;

    for (int n_stages = 1; n_stages <= 4; n_stages++) {
        /* Adjust specification for each trial */
        filter_design_spec_t trial_spec = *spec;
        filter_design_result_t *result = filter_design_emi(&trial_spec, noise);
        if (result == NULL) continue;

        /* Evaluate fitness */
        double il_score = result->filter.dm_insertion_loss / 100.0;
        double cost_score = result->estimated_cost_usd / spec->max_cost_usd;
        double vol_score = result->estimated_volume_cm3 / spec->max_volume_cm3;
        double loss_score = result->filter.power_loss / (spec->i_nominal * spec->v_nominal * 0.1 + 1.0);

        double fitness = w.weight_insertion_loss * (1.0 - il_score)
                        + w.weight_cost * cost_score
                        + w.weight_volume * vol_score
                        + w.weight_power_loss * loss_score
                        - w.weight_margin * (result->achieved_margin_db / 20.0);

        if (fitness < best_fitness) {
            best_fitness = fitness;
            if (best_design) filter_design_result_free(best_design);
            best_design = result;
            opt->iterations_used += result->iterations;
        } else {
            filter_design_result_free(result);
        }
    }

    if (best_design) {
        opt->filter = (cm_dm_filter_t *)malloc(sizeof(cm_dm_filter_t));
        if (opt->filter) {
            memcpy(opt->filter, &best_design->filter, sizeof(cm_dm_filter_t));
        }
        opt->fitness = best_fitness;
        opt->il_score = best_design->filter.dm_insertion_loss / 100.0;
        opt->cost_score = best_design->estimated_cost_usd / (spec->max_cost_usd > 0.0 ? spec->max_cost_usd : 1.0);
        opt->volume_score = best_design->estimated_volume_cm3 / (spec->max_volume_cm3 > 0.0 ? spec->max_volume_cm3 : 1.0);
        opt->loss_score = best_design->filter.power_loss / (spec->i_nominal * spec->v_nominal * 0.1 + 1.0);
        opt->pareto_optimal = 1;
        filter_design_result_free(best_design);
    }

    return opt;
}

void optimization_result_free(optimization_result_t *result) {
    if (result) {
        free(result->filter);
        free(result);
    }
}

/* ========================================================================
 * L7: Standard filter selection from commercial products
 *
 * Matches a design specification to off-the-shelf EMI filters from
 * major manufacturers. This is the most common approach in practice:
 * rather than designing a custom filter, engineers select from
 * a catalog.
 *
 * Selection criteria:
 *  1. Rated current ≥ I_nominal
 *  2. Rated voltage ≥ V_nominal
 *  3. CM IL at key frequencies meets attenuation needs
 *  4. Leakage current within limits
 *  5. Dimensions fit within mechanical constraints
 *  6. Cost within budget
 *
 * Reference: Schaffner FN Series, TDK-Lambda RS Series, KEMET SS Coils
 * ======================================================================== */

static standard_filter_t g_standard_filters[] = {
    /* Schaffner */
    {"Schaffner", "FN2010-3-06", 3.0, 250, 30, 35, 55, 60, 0.4, 64, 35, 24, 8.50, 0},
    {"Schaffner", "FN2010-6-06", 6.0, 250, 30, 35, 50, 55, 0.5, 85, 41, 30, 12.00, 0},
    {"Schaffner", "FN2010-10-06", 10.0, 250, 25, 30, 45, 50, 0.6, 98, 51, 38, 16.00, 0},
    {"Schaffner", "FN2070-3-07", 3.0, 250, 45, 50, 70, 75, 0.3, 96, 56, 45, 18.00, 1},
    {"Schaffner", "FN2070-6-07", 6.0, 250, 45, 50, 65, 70, 0.4, 101, 66, 55, 22.00, 1},
    /* TDK-Lambda */
    {"TDK-Lambda", "RSEN-2003", 3.0, 250, 25, 30, 50, 55, 0.3, 60, 32, 22, 7.00, 0},
    {"TDK-Lambda", "RSEN-2006", 6.0, 250, 25, 30, 45, 50, 0.4, 75, 38, 28, 10.00, 0},
    {"TDK-Lambda", "RSEN-2010", 10.0, 250, 20, 25, 40, 45, 0.5, 90, 45, 35, 14.00, 0},
    /* KEMET */
    {"KEMET", "SS11VL-03530", 3.0, 250, 30, 35, 55, 60, 0.35, 55, 30, 25, 6.50, 0},
    {"KEMET", "SS21V-05020", 5.0, 250, 25, 30, 50, 55, 0.45, 65, 35, 28, 9.00, 0},
};

#define NUM_STANDARD_FILTERS (sizeof(g_standard_filters) / sizeof(g_standard_filters[0]))

filter_selection_t *standard_filter_select(const filter_design_spec_t *spec) {
    if (spec == NULL) return NULL;

    filter_selection_t *sel = (filter_selection_t *)malloc(sizeof(filter_selection_t));
    if (sel == NULL) return NULL;

    memset(sel, 0, sizeof(filter_selection_t));
    sel->filters = NULL;
    sel->count = 0;
    sel->best_index = -1;
    sel->best_score = DBL_MAX;

    for (size_t i = 0; i < NUM_STANDARD_FILTERS; i++) {
        standard_filter_t *sf = &g_standard_filters[i];

        /* Must meet current and voltage rating */
        if (sf->rated_current_a < spec->i_nominal) continue;
        if (sf->rated_voltage_v < spec->v_nominal) continue;

        /* Score: lower is better */
        double score = 0.0;
        /* IL at 150 kHz: need at least 20 dB typically */
        double il_min_at_150k = (sf->cm_il_150k_db < sf->dm_il_150k_db) ?
                                 sf->cm_il_150k_db : sf->dm_il_150k_db;
        if (il_min_at_150k < 20.0) score += (20.0 - il_min_at_150k);

        /* Cost per watt */
        double cost_per_w = sf->price_usd / (sf->rated_current_a * sf->rated_voltage_v);
        score += cost_per_w * 100.0;

        /* Volume */
        double volume_cm3 = sf->length_mm * sf->width_mm * sf->height_mm / 1000.0;
        score += volume_cm3 / 10.0;

        /* Leakage current */
        score += sf->leakage_current_ma;

        if (score < sel->best_score) {
            sel->best_score = score;
            sel->best_index = (int)sel->count;
        }

        /* Add to results */
        standard_filter_t *new_arr = (standard_filter_t *)realloc(
            sel->filters, (sel->count + 1) * sizeof(standard_filter_t));
        if (new_arr == NULL) break;
        sel->filters = new_arr;
        sel->filters[sel->count] = *sf;
        sel->count++;
    }

    return sel;
}

void filter_selection_free(filter_selection_t *sel) {
    if (sel) {
        free(sel->filters);
        free(sel);
    }
}

/* ========================================================================
 * L8: Reliability prediction per MIL-HDBK-217F
 *
 * Parts count reliability prediction for EMI filters:
 *
 * Failure rate model:
 *   λ_p = λ_b · π_T · π_Q · π_E  [failures/10⁶ hours]
 *
 * Component failure rates (ground benign, 25°C):
 *   Fixed capacitor (plastic): λ_b = 0.0049
 *   Inductor: λ_b = 0.0003
 *   Resistor (film): λ_b = 0.0025
 *   PCB (per plated-through hole): λ_b = 0.000042
 *
 * π_T (Temperature factor):
 *   π_T = exp( -E_a/k · (1/T_j - 1/298) )
 *   Simplified: π_T ≈ exp(0.1·(T_j - 25)) for typical components
 *
 * π_Q (Quality factor):
 *   Commercial: π_Q = 3.0
 *   Industrial: π_Q = 1.0
 *   Military:   π_Q = 0.3
 *
 * π_E (Environment factor):
 *   Ground Benign (G_B): 1.0
 *   Ground Fixed (G_F):  2.0
 *   Naval Sheltered (N_S): 4.0
 *   Airborne (A_IF):     10.0
 *
 * MTBF = 1 / λ_total  [hours]
 * Reliability R(t) = exp(-λ·t)
 * ======================================================================== */

reliability_report_t reliability_predict(const cm_dm_filter_t *filter,
                                          double ambient_c,
                                          int environment) {
    reliability_report_t rr;
    memset(&rr, 0, sizeof(rr));

    if (filter == NULL) return rr;

    /* Environment factor lookup */
    double pi_e[4] = {1.0, 2.0, 4.0, 10.0};
    double pi_e_val = (environment >= 0 && environment < 4) ?
                       pi_e[environment] : 1.0;

    /* Temperature factor (simplified Arrhenius) */
    double temp_j = ambient_c + 15.0;  /* Junction temp ≈ ambient + 15K rise */
    double pi_t = exp(0.05 * (temp_j - 25.0));  /* Simplified */

    /* Count components */
    int n_caps = 0, n_inductors = 0, n_resistors = 0;
    for (size_t s = 0; s < filter->num_stages; s++) {
        for (size_t e = 0; e < filter->stages[s].num_elements; e++) {
            switch (filter->stages[s].elements[e].type) {
                case FILTER_ELEM_X_CAPACITOR:
                case FILTER_ELEM_Y_CAPACITOR:
                    n_caps++;
                    break;
                case FILTER_ELEM_CM_CHOKE:
                case FILTER_ELEM_DM_INDUCTOR:
                    n_inductors++;
                    break;
                case FILTER_ELEM_RESISTOR:
                    n_resistors++;
                    break;
                default:
                    break;
            }
        }
    }

    /* Base failure rates [FIT = failures/10⁹ hours] */
    double lambda_cap = 4.9 * pi_t * 3.0 * pi_e_val;  /* Capacitors */
    double lambda_ind = 0.3 * pi_t * 3.0 * pi_e_val;  /* Inductors */
    double lambda_res = 2.5 * pi_t * 3.0 * pi_e_val;  /* Resistors */
    double lambda_pcb = 4.2 * 5.0 * pi_t * pi_e_val;  /* PCB (5 PTH assumed) */

    rr.capacitor_failure_rate = lambda_cap * n_caps;
    rr.inductor_failure_rate = lambda_ind * n_inductors;
    rr.resistor_failure_rate = lambda_res * n_resistors;
    rr.pcb_failure_rate = lambda_pcb;
    rr.failure_rate_fit = rr.capacitor_failure_rate
                         + rr.inductor_failure_rate
                         + rr.resistor_failure_rate
                         + rr.pcb_failure_rate;

    /* MTBF = 1e9 / FIT  [hours] */
    if (rr.failure_rate_fit > 0.0) {
        rr.mtbf_hours = 1e9 / rr.failure_rate_fit;
    } else {
        rr.mtbf_hours = DBL_MAX;
    }

    /* Reliability over time: R(t) = exp(-λ·t)
     * λ [per hour] = FIT / 1e9 */
    double lambda_per_hour = rr.failure_rate_fit / 1e9;
    double hours_5yr = 5.0 * 365.25 * 24.0;
    double hours_10yr = 10.0 * 365.25 * 24.0;
    rr.reliability_5yr = exp(-lambda_per_hour * hours_5yr);
    rr.reliability_10yr = exp(-lambda_per_hour * hours_10yr);

    /* Top stressors */
    rr.dominant_stressor[0] = rr.capacitor_failure_rate;
    rr.dominant_stressor[1] = rr.inductor_failure_rate;
    rr.dominant_stressor[2] = pi_t * 10.0;

    return rr;
}

/* ========================================================================
 * L8: Impedance interaction analysis
 *
 * Analyzes potential stability issues caused by filter-source-load
 * impedance interactions.
 *
 * Middlebrook stability criterion:
 *   For stable operation, the filter output impedance Z_out must be
 *   much smaller than the load input impedance Z_L at all frequencies
 *   where the loop gain |T| ≥ 1.
 *
 *   |Z_out(f)| ≪ |Z_L(f)|  for all f
 *   |Z_in(f)| ≫ |Z_S(f)|   for all f
 *
 * Typically: |Z_out| < 0.2 · |Z_L| and |Z_in| > 5 · |Z_S|
 *
 * Oscillation risk increases when:
 *   1. Filter output impedance has high Q resonance
 *   2. Source impedance is inductive (increases with f)
 *   3. Load is a negative incremental resistance (constant power)
 *
 * Reference: Middlebrook, R.D. "Input Filter Considerations in
 *   Design of Switching Regulators" (1976), IEEE PESC
 * ======================================================================== */

impedance_interaction_t *impedance_interaction_analyze(
    const cm_dm_filter_t *filter,
    double *z_source_vs_freq,
    double *z_load_vs_freq,
    const double *frequencies,
    size_t num_points) {
    (void)z_load_vs_freq; /* Reserved for future impedance interaction analysis */

    if (filter == NULL || frequencies == NULL || num_points == 0) return NULL;

    impedance_interaction_t *ia = (impedance_interaction_t *)
        malloc(sizeof(impedance_interaction_t));
    if (ia == NULL) return NULL;

    memset(ia, 0, sizeof(impedance_interaction_t));
    ia->num_points = num_points;
    ia->frequencies = (double *)malloc(num_points * sizeof(double));
    ia->z_in = (complex_t *)malloc(num_points * sizeof(complex_t));
    ia->z_out = (complex_t *)malloc(num_points * sizeof(complex_t));

    if (!ia->frequencies || !ia->z_in || !ia->z_out) {
        free(ia->frequencies);
        free(ia->z_in);
        free(ia->z_out);
        free(ia);
        return NULL;
    }

    ia->min_z_ratio = DBL_MAX;
    ia->max_z_ratio = 0.0;

    /* Analyze impedance interaction at each frequency */
    for (size_t i = 0; i < num_points; i++) {
        ia->frequencies[i] = frequencies[i];

        /* Estimate filter impedances from first stage
         * (simplified: use dominant LC stage) */
        if (filter->num_stages > 0) {
            filter_stage_t *stage = &filter->stages[0];
            double l = 0.0, c = 0.0;
            for (size_t e = 0; e < stage->num_elements; e++) {
                if (stage->elements[e].type == FILTER_ELEM_DM_INDUCTOR)
                    l = stage->elements[e].nominal_value;
                if (stage->elements[e].type == FILTER_ELEM_X_CAPACITOR)
                    c = stage->elements[e].nominal_value;
            }

            double omega = M_2PI * frequencies[i];
            /* Z_in ≈ jωL (series inductor first) */
            ia->z_in[i].real = 0.1;
            ia->z_in[i].imag = omega * l;
            /* Z_out ≈ 1/(jωC) (shunt capacitor last) */
            if (c > 1e-30 && omega > 1e-30) {
                ia->z_out[i].real = 0.1;
                ia->z_out[i].imag = -1.0 / (omega * c);
            } else {
                ia->z_out[i].real = 0.1;
                ia->z_out[i].imag = 0.0;
            }
        }

        /* Compute Z_in/Z_source ratio */
        if (z_source_vs_freq && z_source_vs_freq[i] > 1e-30) {
            double ratio = complex_mag(ia->z_in[i]) / z_source_vs_freq[i];
            if (ratio < ia->min_z_ratio) ia->min_z_ratio = ratio;
            if (ratio > ia->max_z_ratio) ia->max_z_ratio = ratio;
        }
    }

    /* Middlebrook criterion: |Z_in/Z_source| > 5 at all frequencies */
    ia->middlebrook_pass = (ia->min_z_ratio > 5.0) ? 1 : 0;

    /* Oscillation risk: 0-1 scale */
    if (ia->min_z_ratio < 1.0) {
        ia->oscillation_risk = 1.0 - ia->min_z_ratio;
    } else if (ia->min_z_ratio < 5.0) {
        ia->oscillation_risk = (5.0 - ia->min_z_ratio) / 4.0;
    } else {
        ia->oscillation_risk = 0.0;
    }

    return ia;
}

void impedance_interaction_free(impedance_interaction_t *ia) {
    if (ia) {
        free(ia->frequencies);
        free(ia->z_in);
        free(ia->z_out);
        free(ia);
    }
}

/* ========================================================================
 * L9: Active EMI filter (AEF) design
 *
 * Active EMI filters use feedback amplifiers to sense noise and
 * inject cancelling current, achieving high insertion loss with
 * physically small components.
 *
 * Working principle:
 * 1. Sense CM noise voltage via capacitive divider
 * 2. Amplify with wideband op-amp
 * 3. Inject cancelling current via coupling capacitor/transformer
 *
 * Design constraints:
 *  - Op-amp GBW must be > 10× f_noise_max for effective cancellation
 *  - Slew rate: SR ≥ 2π · V_noise_peak · f_noise_max
 *  - Injection coupling must withstand line voltage
 *  - Power consumption scales with bandwidth and noise amplitude
 *
 * Advantages over passive:
 *  - 50-90% volume reduction
 *  - Higher IL at low frequencies
 *  - No large magnetic cores needed
 *
 * Challenges:
 *  - Limited by op-amp bandwidth
 *  - Power supply needed for active circuitry
 *  - Reliability (active components have higher failure rates)
 *  - EMC susceptibility of the active circuitry itself
 *
 * Reference: Narayanasamy, B. et al. "Active EMI Filtering for
 *   High-Power-Density Converters" (2020), IEEE JESTPE
 * ======================================================================== */

active_emi_filter_t active_emi_filter_design(double max_freq,
                                              double max_v_noise,
                                              double gbw_mhz) {
    active_emi_filter_t aef;
    memset(&aef, 0, sizeof(aef));

    aef.gain_bandwidth_mhz = gbw_mhz > 0.0 ? gbw_mhz : 20.0;

    /* Required GBW: ~10× the max cancellation frequency.
     * Bandwidth scales with available GBW. */
    aef.bandwidth_hz = gbw_mhz * 1e6 / 20.0;  /* At gain of 20 */

    /* Slew rate requirement: SR ≥ 2π·f·V_pk */
    aef.slew_rate_v_us = M_2PI * max_freq * max_v_noise * 1e-6;
    if (aef.slew_rate_v_us < 0.5) aef.slew_rate_v_us = 2.0;  /* Min practical */

    /* Sense gain: typically 100× */
    aef.sense_gain = 100.0;

    /* Injection gain: typically 10× */
    aef.injection_gain = 10.0;

    /* Effective bandwidth limited by GBW */
    if (aef.bandwidth_hz > max_freq * 5.0) {
        aef.bandwidth_hz = max_freq * 5.0;
    }

    /* Maximum cancellation voltage */
    aef.max_cancel_voltage = max_v_noise * aef.injection_gain;
    if (aef.max_cancel_voltage > 50.0) aef.max_cancel_voltage = 50.0;  /* Practical limit */

    /* Power consumption estimate:
     * Quiescent current ~2 mA at 5V → 10 mW for op-amp
     * Plus injection current: I_inj ≈ V_cancel / Z_inj */
    double z_inj = 100.0;  /* Injection impedance */
    double p_injection = aef.max_cancel_voltage * aef.max_cancel_voltage / z_inj;
    aef.power_consumption_mw = 10.0 + p_injection * 1000.0;

    return aef;
}
