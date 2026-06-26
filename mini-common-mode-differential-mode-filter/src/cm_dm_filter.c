/**
 * @file cm_dm_filter.c
 * @brief CM/DM Filter Core Implementation
 *
 * Implements the fundamental CM/DM decomposition, filter element operations,
 * insertion loss calculation, transfer function analysis, and filter design
 * algorithms.
 *
 * Every function in this file implements a distinct knowledge point from
 * the 9-level knowledge hierarchy (L1-L9) as specified in SKILL.md.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <float.h>
#include <assert.h>

#include "cm_dm_filter.h"
#include "cm_choke_model.h"
#include "dm_filter_model.h"
#include "cm_dm_network.h"
#include "insertion_loss.h"
#include "filter_design_params.h"

/* ========================================================================
 * Internal constants
 * ======================================================================== */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define M_2PI (2.0 * M_PI)
#define M_PI_2 (M_PI / 2.0)

/* ========================================================================
 * L1: Complex number arithmetic
 *
 * These are the foundational operations for all frequency-domain analysis
 * of CM/DM filters. Every transfer function, impedance, and S-parameter
 * computation depends on complex arithmetic.
 * ======================================================================== */

complex_t complex_add(complex_t a, complex_t b) {
    complex_t r;
    r.real = a.real + b.real;
    r.imag = a.imag + b.imag;
    return r;
}

complex_t complex_sub(complex_t a, complex_t b) {
    complex_t r;
    r.real = a.real - b.real;
    r.imag = a.imag - b.imag;
    return r;
}

complex_t complex_mul(complex_t a, complex_t b) {
    /* (a_r + j·a_i)(b_r + j·b_i) = (a_r·b_r - a_i·b_i) + j(a_r·b_i + a_i·b_r) */
    complex_t r;
    r.real = a.real * b.real - a.imag * b.imag;
    r.imag = a.real * b.imag + a.imag * b.real;
    return r;
}

complex_t complex_div(complex_t a, complex_t b) {
    /* a/b = (a_r + j·a_i)/(b_r + j·b_i) = [(a_r·b_r + a_i·b_i) + j(a_i·b_r - a_r·b_i)] / (b_r² + b_i²) */
    double denom = b.real * b.real + b.imag * b.imag;
    complex_t r;
    if (denom < 1e-30) {
        /* Division by zero: return infinity representation */
        r.real = a.real > 0 ? DBL_MAX : (a.real < 0 ? -DBL_MAX : 0.0);
        r.imag = a.imag > 0 ? DBL_MAX : (a.imag < 0 ? -DBL_MAX : 0.0);
        return r;
    }
    r.real = (a.real * b.real + a.imag * b.imag) / denom;
    r.imag = (a.imag * b.real - a.real * b.imag) / denom;
    return r;
}

double complex_mag(complex_t z) {
    return sqrt(z.real * z.real + z.imag * z.imag);
}

double complex_phase_deg(complex_t z) {
    if (fabs(z.real) < 1e-30 && fabs(z.imag) < 1e-30) return 0.0;
    return atan2(z.imag, z.real) * 180.0 / M_PI;
}

complex_t complex_from_polar(double mag, double phase_rad) {
    complex_t r;
    r.real = mag * cos(phase_rad);
    r.imag = mag * sin(phase_rad);
    return r;
}

complex_t complex_conjugate(complex_t z) {
    complex_t r;
    r.real = z.real;
    r.imag = -z.imag;
    return r;
}

/* ========================================================================
 * L1: CM/DM Decomposition — The foundational mathematical operation
 *
 * Single-phase decomposition:
 *   V_cm = (V_L + V_N) / 2
 *   V_dm = V_L - V_N
 *   I_cm = I_L + I_N
 *   I_dm = (I_L - I_N) / 2
 *
 * Theorem (Paul, 2006 §3.2): Any set of N conductor voltages and currents
 * can be uniquely decomposed into N-1 differential-mode components and
 * 1 common-mode component. This is the fundamental decomposition that
 * underpins all EMI filter analysis.
 * ======================================================================== */

int cm_dm_decompose(double v_line, double v_neutral,
                    double i_line, double i_neutral,
                    double freq, cm_dm_decomposition_t *result) {
    if (result == NULL) return -1;
    if (freq <= 0.0) return -1;

    result->v_line = v_line;
    result->v_neutral = v_neutral;
    result->i_line = i_line;
    result->i_neutral = i_neutral;

    /* CM quantities: average for voltage, sum for current */
    result->cm.voltage_cm = 0.5 * (v_line + v_neutral);
    result->cm.current_cm = i_line + i_neutral;
    result->cm.frequency = freq;

    /* DM quantities: difference for voltage, half-difference for current */
    result->dm.voltage_dm = v_line - v_neutral;
    result->dm.current_dm = 0.5 * (i_line - i_neutral);
    result->dm.frequency = freq;

    /* Compute CM and DM impedances */
    if (fabs(result->cm.current_cm) > 1e-30) {
        result->cm.impedance_cm = result->cm.voltage_cm / result->cm.current_cm;
    } else {
        result->cm.impedance_cm = DBL_MAX;
    }

    if (fabs(result->dm.current_dm) > 1e-30) {
        result->dm.impedance_dm = result->dm.voltage_dm / result->dm.current_dm;
    } else {
        result->dm.impedance_dm = DBL_MAX;
    }

    return 0;
}

/* ========================================================================
 * L1: Three-phase CM/DM decomposition using Fortescue/Clarke transforms
 *
 * For three-phase systems:
 *   Zero-sequence (CM): V₀ = (V_a + V_b + V_c) / 3
 *   Positive-sequence (DM-α): V_α = (2V_a - V_b - V_c) / 3
 *   Negative-sequence (DM-β): V_β = (V_b - V_c) / √3
 *
 * The Clarke transform preserves power invariance with factor √(2/3).
 * ======================================================================== */

int cm_dm_decompose_3phase(double va, double vb, double vc,
                           double ia, double ib, double ic,
                           double freq,
                           cm_quantities_t *cm_out,
                           dm_quantities_t *dm_alpha,
                           dm_quantities_t *dm_beta) {
    if (cm_out == NULL || dm_alpha == NULL || dm_beta == NULL) return -1;
    if (freq <= 0.0) return -1;

    /* Zero-sequence (common-mode): average of all three phases */
    cm_out->voltage_cm = (va + vb + vc) / 3.0;
    cm_out->current_cm = ia + ib + ic;
    cm_out->frequency = freq;

    /* Clarke transform: α-axis (in-phase with phase A) */
    dm_alpha->voltage_dm = (2.0 * va - vb - vc) / 3.0;
    dm_alpha->current_dm = (2.0 * ia - ib - ic) / 3.0;
    dm_alpha->frequency = freq;

    /* Clarke transform: β-axis (quadrature) */
    const double sqrt3 = 1.7320508075688772;
    dm_beta->voltage_dm = (vb - vc) / sqrt3;
    dm_beta->current_dm = (ib - ic) / sqrt3;
    dm_beta->frequency = freq;

    /* Impedances */
    cm_out->impedance_cm = (fabs(cm_out->current_cm) > 1e-30) ?
        cm_out->voltage_cm / cm_out->current_cm : DBL_MAX;

    dm_alpha->impedance_dm = (fabs(dm_alpha->current_dm) > 1e-30) ?
        dm_alpha->voltage_dm / dm_alpha->current_dm : DBL_MAX;

    dm_beta->impedance_dm = (fabs(dm_beta->current_dm) > 1e-30) ?
        dm_beta->voltage_dm / dm_beta->current_dm : DBL_MAX;

    return 0;
}

/* ========================================================================
 * L1: Common-Mode Rejection Ratio (CMRR)
 *
 * CMRR = 20·log10( |Z_cm| / |Z_dm| )  [dB]
 *
 * This is the single most important figure of merit for a CM choke.
 * It quantifies how much more impedance the choke presents to CM
 * currents than to DM currents.
 *
 * Ideal CM choke: Z_dm → 0 → CMRR → ∞
 * Practical CM chokes: CMRR 30–80 dB (limited by leakage inductance
 *   and parasitic capacitance)
 *
 * Reference: Paul §6.4.2 "Common-Mode Choke Performance"
 * ======================================================================== */

cmrr_result_t *cmrr_calculate(double z_cm, double z_dm, double freq) {
    cmrr_result_t *r = (cmrr_result_t *)malloc(sizeof(cmrr_result_t));
    if (r == NULL) return NULL;

    r->cm_impedance = z_cm;
    r->dm_impedance = z_dm;
    r->frequency = freq;

    /* A_dm is the DM gain (how much DM passes through) */
    /* A_cm is the CM gain (how much CM passes through) */
    /* In a simple model, A_dm ∝ 1/(1 + Z_dm/Z₀), A_cm ∝ 1/(1 + Z_cm/Z₀) */

    if (z_dm < 1e-30) {
        /* Leakage inductance effectively zero */
        r->cmrr_db = DBL_MAX;
        r->a_dm = 1.0;
        r->a_cm = DBL_MIN;
    } else {
        r->cmrr_db = 20.0 * log10(z_cm / z_dm);
        /* Normalize gains relative to reference impedance */
        const double z0 = 50.0;  /* Standard reference */
        r->a_dm = z0 / (z0 + z_dm);
        r->a_cm = z0 / (z0 + z_cm);
    }

    return r;
}

void cmrr_free(cmrr_result_t *r) {
    free(r);
}

/* ========================================================================
 * L1: Insertion Loss — The primary EMI filter metric
 *
 * IL = 20·log10( |V_without_filter| / |V_with_filter| )  [dB]
 *
 * For a series impedance Z_filter in a circuit with source Z_S and
 * load Z_L:
 *
 *   Without filter: V_L = V_S · Z_L / (Z_S + Z_L)
 *   With filter:    V_L = V_S · Z_L / (Z_S + Z_filter + Z_L)
 *
 *   IL = 20·log10( (Z_S + Z_filter + Z_L) / (Z_S + Z_L) )
 *      = 20·log10( 1 + Z_filter / (Z_S + Z_L) )
 *
 * For a shunt impedance Y_filter across the line:
 *   IL = 20·log10( 1 + Y_filter · Z_S · Z_L / (Z_S + Z_L) )
 *
 * Reference: CISPR 17, Paul §6.3.1
 * ======================================================================== */

insertion_loss_result_t *insertion_loss_calc(double z_source, double z_load,
                                             double z_filter_cm,
                                             double z_filter_dm,
                                             double freq, int is_cm) {
    insertion_loss_result_t *r = (insertion_loss_result_t *)
        malloc(sizeof(insertion_loss_result_t));
    if (r == NULL) return NULL;

    r->frequency = freq;
    r->z_source = z_source;
    r->z_load = z_load;
    r->is_cm = is_cm;

    double z_filter = is_cm ? z_filter_cm : z_filter_dm;

    if (z_source <= 0.0 || z_load <= 0.0) {
        r->il_db = 0.0;
        r->v_without = 1.0;
        r->v_with = 1.0;
        return r;
    }

    /* Series impedance model */
    double v_without = z_load / (z_source + z_load);
    double v_with = z_load / (z_source + z_filter + z_load);

    r->v_without = fabs(v_without);
    r->v_with = fabs(v_with);

    if (r->v_with < 1e-30) {
        r->il_db = DBL_MAX;
    } else {
        r->il_db = 20.0 * log10(r->v_without / r->v_with);
    }

    return r;
}

void insertion_loss_free(insertion_loss_result_t *r) {
    free(r);
}

/* ========================================================================
 * L2: Filter element creation and manipulation
 *
 * Each element has a nominal value (L, C, or R), tolerance, and
 * parasitic elements that limit high-frequency performance.
 * ======================================================================== */

filter_element_t *filter_element_create(filter_elem_type_t type,
                                         double nominal, double tol_pct) {
    if (tol_pct < 0.0 || tol_pct > 100.0) return NULL;

    filter_element_t *elem = (filter_element_t *)malloc(sizeof(filter_element_t));
    if (elem == NULL) return NULL;

    elem->type = type;
    elem->nominal_value = nominal;
    elem->tolerance_pct = tol_pct;

    /* Initialize parasitics to zero (ideal component) */
    elem->esr = 0.0;
    elem->esl = (type == FILTER_ELEM_X_CAPACITOR || type == FILTER_ELEM_Y_CAPACITOR
                  || type == FILTER_ELEM_FEEDTHROUGH) ? 1e-9 : 0.0;  /* 1 nH typical */
    elem->epc = (type == FILTER_ELEM_CM_CHOKE || type == FILTER_ELEM_DM_INDUCTOR
                 || type == FILTER_ELEM_FERRITE_BEAD) ? 1e-12 : 0.0;  /* 1 pF typical */
    elem->r_parallel = 1e9;     /* 1 GΩ (essentially open) */
    elem->self_resonant_freq = DBL_MAX;
    elem->rated_voltage = 250.0;
    elem->rated_current = 1.0;
    elem->temperature_coef = 0.0;

    return elem;
}

void filter_element_set_parasitics(filter_element_t *elem,
                                   double esr, double esl,
                                   double epc, double r_parallel) {
    if (elem == NULL) return;
    elem->esr = esr > 0.0 ? esr : 0.0;
    elem->esl = esl > 0.0 ? esl : 0.0;
    elem->epc = epc > 0.0 ? epc : 0.0;
    elem->r_parallel = r_parallel > 0.0 ? r_parallel : 1e9;

    /* Calculate self-resonant frequency */
    if (elem->type == FILTER_ELEM_CM_CHOKE || elem->type == FILTER_ELEM_DM_INDUCTOR) {
        if (elem->epc > 1e-30 && elem->nominal_value > 1e-30) {
            /* Parallel resonance: f_res = 1/(2π√(L·C_p)) */
            elem->self_resonant_freq = 1.0 / (M_2PI *
                sqrt(elem->nominal_value * elem->epc));
        }
    } else if (elem->type == FILTER_ELEM_X_CAPACITOR ||
               elem->type == FILTER_ELEM_Y_CAPACITOR ||
               elem->type == FILTER_ELEM_FEEDTHROUGH) {
        if (elem->esl > 1e-30 && elem->nominal_value > 1e-30) {
            /* Series resonance: f_res = 1/(2π√(L_s·C)) */
            elem->self_resonant_freq = 1.0 / (M_2PI *
                sqrt(elem->esl * elem->nominal_value));
        }
    }
}

complex_t filter_element_impedance(const filter_element_t *elem, double freq) {
    complex_t z = {0.0, 0.0};
    if (elem == NULL || freq < 0.0) return z;

    double omega = M_2PI * freq;
    double omega_l, omega_c;

    switch (elem->type) {
        case FILTER_ELEM_CM_CHOKE:
        case FILTER_ELEM_DM_INDUCTOR:
        case FILTER_ELEM_FERRITE_BEAD:
            /* Z_L = jωL + ESR + (R_parallel || 1/(jωC_parallel))
             * Simplification: series ESR + inductive reactance,
             * with parasitic parallel capacitance */
            {
                double l = elem->nominal_value;
                omega_l = omega * l;
                /* Series RL impedance */
                z.real = elem->esr;
                z.imag = omega_l;

                /* Parallel parasitic capacitance modifies impedance */
                if (elem->epc > 1e-30) {
                    /* Z = (R_series + jωL) || (1/(jωC_parallel))
                     *   = (R + jωL) / (1 - ω²LC + jωRC) */
                    double w2_lc = omega * omega * l * elem->epc;
                    double w_rc = omega * elem->esr * elem->epc;
                    double denom = (1.0 - w2_lc) * (1.0 - w2_lc) + w_rc * w_rc;
                    if (denom > 1e-60) {
                        double new_real = (elem->esr) / denom;
                        double new_imag = omega_l * (1.0 - w2_lc) / denom
                                        - w_rc * omega_l / denom;
                        z.real = new_real;
                        z.imag = new_imag;
                    }
                }
            }
            break;

        case FILTER_ELEM_X_CAPACITOR:
        case FILTER_ELEM_Y_CAPACITOR:
        case FILTER_ELEM_FEEDTHROUGH:
            /* Z_C = 1/(jωC) + ESR + jω·ESL */
            {
                double c = elem->nominal_value;
                if (c > 1e-30 && omega > 1e-30) {
                    omega_c = -1.0 / (omega * c);  /* -j/(ωC) → imag = -1/(ωC) */
                } else {
                    omega_c = -DBL_MAX;
                }
                z.real = elem->esr;
                z.imag = omega * elem->esl + omega_c;
            }
            break;

        case FILTER_ELEM_RESISTOR:
            z.real = elem->nominal_value;
            z.imag = omega * elem->esl;
            break;

        case FILTER_ELEM_TVS:
            /* TVS: modeled as open circuit below clamping voltage */
            z.real = 1e9;
            z.imag = 0.0;
            break;

        default:
            break;
    }

    return z;
}

void filter_element_free(filter_element_t *elem) {
    free(elem);
}

/* ========================================================================
 * L4: CM Choke Creation from Core Material and Geometry
 *
 * Faraday's law for a toroidal core:
 *   L = N² · μ₀ · μ_r · A_e / l_e = N² · AL  [H]
 *
 * where:
 *   μ₀ = 4π×10⁻⁷ H/m (vacuum permeability)
 *   μ_r = relative permeability of core material
 *   A_e = effective cross-sectional area [m²]
 *   l_e = effective magnetic path length [m]
 *   N   = number of turns
 *   AL  = inductance factor [H/turn²]
 *
 * For CM: both windings produce flux in SAME direction
 *   → fluxes ADD → high inductance = k · N² · AL
 *
 * For DM: windings produce flux in OPPOSITE direction
 *   → fluxes CANCEL → low inductance = (1-k) · N² · AL (leakage only)
 *
 * where k is the coupling coefficient (0.98 to 0.999 for bifilar winding).
 *
 * Reference: Paul §6.4, McLyman §3
 * ======================================================================== */

cm_choke_model_t *cm_choke_create(const core_material_t *mat,
                                   const core_geometry_t *geom,
                                   double n, double k) {
    if (mat == NULL || geom == NULL || n <= 0.0 || k <= 0.0 || k > 1.0) {
        return NULL;
    }

    cm_choke_model_t *choke = (cm_choke_model_t *)malloc(sizeof(cm_choke_model_t));
    if (choke == NULL) return NULL;

    /* Copy core material */
    choke->material = (core_material_t *)malloc(sizeof(core_material_t));
    if (choke->material == NULL) { free(choke); return NULL; }
    memcpy(choke->material, mat, sizeof(core_material_t));

    /* Copy core geometry */
    choke->geometry = (core_geometry_t *)malloc(sizeof(core_geometry_t));
    if (choke->geometry == NULL) {
        free(choke->material); free(choke); return NULL;
    }
    memcpy(choke->geometry, geom, sizeof(core_geometry_t));

    choke->num_turns = n;
    choke->coupling_coef = k;

    /* Calculate AL value if not pre-computed */
    const double mu0 = 4.0e-7 * M_PI;
    double al;
    if (geom->al_value > 0.0) {
        al = geom->al_value * 1e-9;  /* nH → H per turn² */
    } else {
        al = mu0 * mat->mu_i * geom->ae / geom->le;
    }

    /* CM inductance (flux addition) */
    choke->l_cm = k * n * n * al;

    /* DM leakage inductance (flux cancellation)
     * L_leak = (1-k) * N² * AL (simplified model)
     * More accurate: L_leak ≈ μ₀ · N² · A_leak / l_leak
     * where A_leak is the leakage flux path area.
     */
    choke->l_dm_leakage = (1.0 - k) * n * n * al;

    /* Winding resistance estimation
     * R ≈ ρ · N · MLT / A_wire
     * where MLT = mean length per turn, A_wire = wire cross-section area
     * For now use a simple estimate: 1 Ω per 10 turns per mm²
     */
    double mlt_estimate = M_PI * ((geom->od + geom->id) / 2.0);
    choke->winding_resistance = 1.68e-8 * n * mlt_estimate / 1e-6; /* ~1mm² wire */

    /* Inter-winding capacitance estimate
     * C_w ≈ ε₀ · ε_r · A_overlap / d_separation
     * Typically 1-100 pF for small toroids
     */
    choke->winding_capacitance = 5e-12;  /* 5 pF typical */

    /* Steinmetz parameters from material data */
    choke->core_loss_coef = 1e-6;  /* W/(Hz^x·T^y·m³) default for MnZn */
    choke->steinmetz_x = 1.4;      /* Frequency exponent */
    choke->steinmetz_y = 2.5;      /* Flux density exponent */

    /* Rating estimates */
    choke->rated_current_dm = mat->b_sat * geom->le / (n * mu0 * mat->mu_i);
    choke->rated_voltage_cm = M_2PI * 100e3 * n * geom->ae * mat->b_sat;

    return choke;
}

/* ========================================================================
 * L3: CM choke CM impedance at frequency f
 *
 * Z_cm(jω) = jωL_cm + R_winding + R_core || 1/(jωC_winding)
 *
 * The parallel combination of core loss resistance and winding capacitance
 * causes the impedance to peak at self-resonance and then decrease at
 * higher frequencies.
 *
 * Core loss resistance is frequency-dependent:
 *   R_core(f) ≈ K / (f^α · B^β) — increases with frequency
 * ======================================================================== */

complex_t cm_choke_cm_impedance(const cm_choke_model_t *choke, double freq) {
    complex_t z = {0.0, 0.0};
    if (choke == NULL) return z;

    double omega = M_2PI * freq;

    /* Inductive reactance: jωL_cm */
    double x_l = omega * choke->l_cm;

    /* Core loss equivalent parallel resistance
     * R_core_loss ≈ (V²) / P_core_loss
     * At low frequency, R_core is high (low loss).
     * At high frequency, R_core decreases (eddy currents increase).
     *
     * Simplified model: R_core(f) = R_core0 / √(f / f_ref)
     */
    double f_ref = 100e3;
    double r_core_base = 1e5;  /* 100 kΩ at 100 kHz for small core */
    double r_core = r_core_base * sqrt(f_ref / (freq > 1.0 ? freq : 1.0));
    if (r_core < 100.0) r_core = 100.0;
    if (r_core > 1e9) r_core = 1e9;

    /* Winding capacitance reactance: -j/(ωC) */
    double x_c = -1.0 / (omega * choke->winding_capacitance);
    (void)x_c; /* Pre-computed for admittance model */

    /* Parallel combination of (jX_L + R_winding) || R_core || (-jX_C)
     *
     * Z_series = R_winding + jωL
     * Z_parallel = 1/(1/R_core + jωC)
     * Z_total = Z_series || Z_parallel
     *
     * Using admittances:
     * Y_series = 1/(R_w + jωL) = (R_w - jωL)/(R_w² + ω²L²)
     * Y_parallel = 1/R_core + jωC + 1/R_leak
     * Y_total = Y_series + Y_parallel
     * Z_total = 1/Y_total
     */

    double x_l2 = choke->winding_resistance * choke->winding_resistance + x_l * x_l;
    complex_t y_total;

    if (x_l2 > 1e-60) {
        y_total.real = choke->winding_resistance / x_l2 + 1.0 / r_core + 1.0 / choke->geometry->al_value;
        y_total.imag = -x_l / x_l2 + omega * choke->winding_capacitance;
    } else {
        y_total.real = 1.0 / r_core;
        y_total.imag = omega * choke->winding_capacitance;
    }

    z = complex_div((complex_t){1.0, 0.0}, y_total);

    return z;
}

/* ========================================================================
 * L4: CM choke DM impedance (leakage inductance only)
 *
 * For DM currents, the main flux cancels and only leakage inductance
 * remains. This is the key property that allows CM chokes to pass
 * power-line current while blocking CM noise.
 *
 * Z_dm(jω) = jω · L_leakage + R_winding
 * ======================================================================== */

complex_t cm_choke_dm_impedance(const cm_choke_model_t *choke, double freq) {
    complex_t z = {0.0, 0.0};
    if (choke == NULL) return z;

    double omega = M_2PI * freq;
    z.real = choke->winding_resistance;
    z.imag = omega * choke->l_dm_leakage;

    return z;
}

/* ========================================================================
 * L6: CM Choke Saturation Check
 *
 * B_max = V_cm_max / (2π · f · N · A_e)
 *
 * If B_max > B_sat, the core saturates:
 *  → μ_r drops from ~2000 to ~1
 *  → CM inductance collapses
 *  → Filter fails catastrophically
 *
 * This is a critical practical limitation: the CM choke must be
 * sized to handle the worst-case CM voltage without saturating,
 * while also handling the full DM load current without overheating.
 *
 * Safety factor of 2 is applied: B_peak ≤ B_sat / 2.
 *
 * Reference: Paul §6.4.5 "Saturation of Common-Mode Chokes"
 * ======================================================================== */

int cm_choke_check_saturation(const cm_choke_model_t *choke,
                              double v_cm_max, double freq) {
    if (choke == NULL || freq <= 0.0) return -1;

    /* B = V_cm / (ω · N · A_e) */
    double omega = M_2PI * freq;
    double b_peak = v_cm_max / (omega * choke->num_turns * choke->geometry->ae);

    /* Apply safety factor: keep B_peak ≤ B_sat / 2 */
    double b_safe = choke->material->b_sat / 2.0;

    return (b_peak > b_safe || b_peak > choke->material->b_sat);
}

/* ========================================================================
 * L3: Core loss calculation using Steinmetz equation
 *
 * Original Steinmetz Equation (1892):
 *   P_v = k · f^α · B_peak^β  [W/m³]
 *
 * where:
 *   k   = material coefficient
 *   α   = frequency exponent (typically 1.0–1.8)
 *   β   = flux density exponent (typically 2.0–3.0)
 *   B_peak = peak AC flux density [T]
 *   f      = frequency [Hz]
 *
 * Total core loss: P_core = P_v · V_e  [W]
 *
 * Reference: Steinmetz, C.P. "On the Law of Hysteresis" (1892),
 *            Reinert, J. et al. "Calculation of Losses in Ferro- and
 *            Ferrimagnetic Materials" (2001) — iGSE extension
 * ======================================================================== */

double cm_choke_core_loss(const cm_choke_model_t *choke,
                          double b_peak, double freq) {
    if (choke == NULL) return 0.0;

    double p_v = choke->core_loss_coef *
                 pow(freq, choke->steinmetz_x) *
                 pow(b_peak, choke->steinmetz_y);

    double volume = choke->geometry->ve;
    return p_v * volume;
}

void cm_choke_free(cm_choke_model_t *choke) {
    if (choke) {
        free(choke->material);
        free(choke->geometry);
        free(choke);
    }
}

/* ========================================================================
 * L2: Filter Stage Transfer Function
 *
 * For a general LC filter stage, the transfer function H(s) is
 * derived from nodal analysis of the filter network.
 *
 * For a Π-filter (C1—L—C2) with source Z_S and load Z_L:
 *
 * Using ABCD parameters:
 *   [A  B]   [1  0]   [1  Z_L]   [1  0]
 *   [C  D] = [Y1 1] × [0   1 ] × [Y2 1]
 *
 * where Y1 = s·C1, Y2 = s·C2, Z_L = s·L
 *
 * Then: H(s) = Z_L / (A·Z_L + B + C·Z_S·Z_L + D·Z_S)
 *
 * This function evaluates H(jω) for a user-specified frequency.
 * ======================================================================== */

transfer_function_point_t filter_stage_transfer(const filter_stage_t *stage,
                                                 double frequency,
                                                 double z_source,
                                                 double z_load) {
    transfer_function_point_t result;
    memset(&result, 0, sizeof(result));

    if (stage == NULL) return result;

    double omega = M_2PI * frequency;
    complex_t s = {0.0, omega};  /* s = jω (steady-state sinusoidal) */
    result.s = s;

    /* Build the filter ABCD matrix and compute transfer function */
    double l1 = 0.0, c1 = 0.0, l2 = 0.0, c2 = 0.0;

    for (size_t i = 0; i < stage->num_elements; i++) {
        filter_element_t *e = &stage->elements[i];
        switch (e->type) {
            case FILTER_ELEM_CM_CHOKE:
            case FILTER_ELEM_DM_INDUCTOR:
            case FILTER_ELEM_FERRITE_BEAD:
                if (l1 < 1e-30) l1 = e->nominal_value;
                else l2 = e->nominal_value;
                break;
            case FILTER_ELEM_X_CAPACITOR:
            case FILTER_ELEM_Y_CAPACITOR:
            case FILTER_ELEM_FEEDTHROUGH:
                if (c1 < 1e-30) c1 = e->nominal_value;
                else c2 = e->nominal_value;
                break;
            default:
                break;
        }
    }

    /* Build ABCD matrix */
    abcd_matrix_t abcd = build_filter_abcd(stage->topology,
                                            l1, c1, l2, c2, frequency);

    /* Compute H(jω) = V_L / V_S from ABCD parameters */
    complex_t aZ_load = {abcd.A.real * z_load, abcd.A.imag * z_load};
    complex_t cZ_src_load = {abcd.C.real * z_source * z_load,
                              abcd.C.imag * z_source * z_load};
    complex_t dZ_src = {abcd.D.real * z_source, abcd.D.imag * z_source};

    /* Denominator: A·Z_L + B + C·Z_S·Z_L + D·Z_S */
    complex_t denom = complex_add(
        complex_add(aZ_load, abcd.B),
        complex_add(cZ_src_load, dZ_src)
    );

    /* Numerator: Z_L */
    complex_t num = {z_load, 0.0};

    /* H = num / denom */
    result.h = complex_div(num, denom);
    result.magnitude = complex_mag(result.h);
    result.phase_deg = complex_phase_deg(result.h);

    /* Group delay: τ_g = -dφ/dω ≈ -Δφ/Δω */
    double omega2 = M_2PI * (frequency * 1.001);
    abcd_matrix_t abcd2 = build_filter_abcd(stage->topology,
                                             l1, c1, l2, c2, frequency * 1.001);
    /* Quick group delay estimate via derivative approximation */
    complex_t num2 = {z_load, 0.0};
    complex_t aZ_load2 = {abcd2.A.real * z_load, abcd2.A.imag * z_load};
    complex_t cZ_src_load2 = {abcd2.C.real * z_source * z_load,
                               abcd2.C.imag * z_source * z_load};
    complex_t dZ_src2 = {abcd2.D.real * z_source, abcd2.D.imag * z_source};
    complex_t denom2 = complex_add(
        complex_add(aZ_load2, abcd2.B),
        complex_add(cZ_src_load2, dZ_src2)
    );
    complex_t h2 = complex_div(num2, denom2);
    double phase1 = atan2(result.h.imag, result.h.real);
    double phase2 = atan2(h2.imag, h2.real);
    double dphi = phase2 - phase1;
    if (dphi > M_PI) dphi -= M_2PI;
    if (dphi < -M_PI) dphi += M_2PI;
    double domega = omega2 - omega;
    result.group_delay = -(dphi / domega);

    return result;
}

/* ========================================================================
 * L3: Bode Plot Generation
 *
 * Computes the frequency response magnitude and phase over a
 * logarithmically-spaced frequency range. This is the standard
 * visualization for filter frequency response.
 *
 * Frequency points are logarithmically spaced:
 *   f_i = f_start · (f_end/f_start)^(i/(N-1))  for i = 0..N-1
 * ======================================================================== */

bode_plot_t *filter_stage_bode(const filter_stage_t *stage,
                                double f_start, double f_end,
                                size_t num_pts,
                                double z_source, double z_load) {
    if (stage == NULL || f_start <= 0.0 || f_end <= f_start || num_pts < 2) {
        return NULL;
    }

    bode_plot_t *bp = (bode_plot_t *)malloc(sizeof(bode_plot_t));
    if (bp == NULL) return NULL;

    bp->f_start = f_start;
    bp->f_end = f_end;
    bp->num_points = num_pts;

    bp->frequencies = (double *)malloc(num_pts * sizeof(double));
    bp->magnitude_db = (double *)malloc(num_pts * sizeof(double));
    bp->phase_deg = (double *)malloc(num_pts * sizeof(double));

    if (bp->frequencies == NULL || bp->magnitude_db == NULL || bp->phase_deg == NULL) {
        free(bp->frequencies);
        free(bp->magnitude_db);
        free(bp->phase_deg);
        free(bp);
        return NULL;
    }

    double log_ratio = log10(f_end / f_start);
    for (size_t i = 0; i < num_pts; i++) {
        double frac = (double)i / (double)(num_pts - 1);
        bp->frequencies[i] = f_start * pow(10.0, frac * log_ratio);

        transfer_function_point_t tf = filter_stage_transfer(
            stage, bp->frequencies[i], z_source, z_load);

        bp->magnitude_db[i] = 20.0 * log10(tf.magnitude > 1e-30 ? tf.magnitude : 1e-30);
        bp->phase_deg[i] = tf.phase_deg;
    }

    return bp;
}

void bode_plot_free(bode_plot_t *bp) {
    if (bp) {
        free(bp->frequencies);
        free(bp->magnitude_db);
        free(bp->phase_deg);
        free(bp);
    }
}

/* ========================================================================
 * L6: Cascaded Multi-stage Filter Transfer Function
 *
 * For N cascaded filter stages, the total transfer function is the
 * product of individual transfer functions:
 *
 *   H_total(s) = H₁(s) · H₂(s) · ... · H_N(s)
 *
 * This assumes inter-stage isolation (output impedance of stage n
 * is much less than input impedance of stage n+1). In practice,
 * modern filter design ensures this by using alternating shunt/series
 * elements.
 *
 * The ABCD matrix cascade approach is more accurate:
 *   [ABCD]_total = [ABCD]₁ × [ABCD]₂ × ... × [ABCD]_N
 * ======================================================================== */

transfer_function_point_t filter_cascade_transfer(const cm_dm_filter_t *filter,
                                                   double frequency,
                                                   double z_source,
                                                   double z_load) {
    transfer_function_point_t result;
    memset(&result, 0, sizeof(result));

    if (filter == NULL || filter->num_stages == 0) return result;

    double omega = M_2PI * frequency;
    result.s.real = 0.0;
    result.s.imag = omega;

    /* Start with identity ABCD matrix: [1 0; 0 1] */
    abcd_matrix_t total = {{1.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {1.0, 0.0}};

    /* Cascade each stage */
    for (size_t n = 0; n < filter->num_stages; n++) {
        filter_stage_t *stage = &filter->stages[n];

        double l1 = 0.0, c1 = 0.0, l2 = 0.0, c2 = 0.0;
        for (size_t i = 0; i < stage->num_elements; i++) {
            switch (stage->elements[i].type) {
                case FILTER_ELEM_CM_CHOKE:
                case FILTER_ELEM_DM_INDUCTOR:
                    if (l1 < 1e-30) l1 = stage->elements[i].nominal_value;
                    else l2 = stage->elements[i].nominal_value;
                    break;
                case FILTER_ELEM_X_CAPACITOR:
                case FILTER_ELEM_Y_CAPACITOR:
                    if (c1 < 1e-30) c1 = stage->elements[i].nominal_value;
                    else c2 = stage->elements[i].nominal_value;
                    break;
                default:
                    break;
            }
        }

        abcd_matrix_t stage_abcd = build_filter_abcd(
            stage->topology, l1, c1, l2, c2, frequency);

        total = abcd_cascade_two(&total, &stage_abcd);
    }

    /* Compute H(s) from total ABCD */
    complex_t num = {z_load, 0.0};
    complex_t aZ = {total.A.real * z_load, total.A.imag * z_load};
    complex_t cZSZ = {total.C.real * z_source * z_load,
                       total.C.imag * z_source * z_load};
    complex_t dZ = {total.D.real * z_source, total.D.imag * z_source};
    complex_t denom = complex_add(
        complex_add(aZ, total.B),
        complex_add(cZSZ, dZ)
    );

    result.h = complex_div(num, denom);
    result.magnitude = complex_mag(result.h);
    result.phase_deg = complex_phase_deg(result.h);

    return result;
}

/* ========================================================================
 * L6: Resonance and Damping Analysis
 *
 * Undamped LC filters have a resonance peak that can AMPLIFY noise
 * at the resonant frequency rather than attenuate it. This is a
 * critical practical problem in EMI filter design.
 *
 * Series RLC resonance:
 *   f_res = 1/(2π√(LC))
 *   Q = (1/R) · √(L/C)  — quality factor
 *
 * Optimal damping:
 *   For a series R-C damper in parallel with C:
 *     R_damp = √(L/C)  — optimal damping
 *     C_damp = 4·C     — damper capacitance (4x main C)
 *
 * Critical damping: R_crit = 2·√(L/C)
 * ======================================================================== */

double filter_optimal_damping_resistance(double l, double c) {
    if (l <= 0.0 || c <= 0.0) return 0.0;
    return sqrt(l / c);
}

double filter_resonance_frequency(double l, double c) {
    if (l <= 0.0 || c <= 0.0) return 0.0;
    return 1.0 / (M_2PI * sqrt(l * c));
}

double filter_quality_factor(double l, double c, double r, int is_parallel) {
    if (l <= 0.0 || c <= 0.0 || r <= 0.0) return 0.0;
    double sqrt_lc = sqrt(l / c);
    if (is_parallel) {
        return r / sqrt_lc;  /* Q = R / √(L/C) */
    } else {
        return sqrt_lc / r;  /* Q = √(L/C) / R */
    }
}

/* ========================================================================
 * L5: Filter Design for EMC Compliance
 *
 * The core design algorithm:
 *
 * 1. Measure or estimate noise spectrum
 * 2. Determine required attenuation:
 *    IL_req(f) = Noise(f) - Limit(f) + Margin
 * 3. Select filter order based on required attenuation:
 *    Order ≈ IL_req_max / (40 · log10(f_noise / f_cutoff))
 * 4. Design each stage:
 *    - Choose cutoff frequency: f_c = f_noise / 10^(IL_req/40)
 *    - Select L, C: f_c = 1/(2π√(LC))
 *    - Check CM choke saturation under DM load
 *    - Add damping to avoid resonance amplification
 * 5. Verify design against worst-case impedances
 * 6. Iterate to meet volume/cost constraints
 *
 * Each LC pair provides ~40 dB/decade asymptotic roll-off.
 *
 * Reference: Ozenbaugh "EMI Filter Design" (2011), Chapter 3-5
 * ======================================================================== */

filter_design_result_t *filter_design_emi(const filter_design_spec_t *spec,
                                           const noise_spectrum_t *noise) {
    if (spec == NULL) return NULL;

    filter_design_result_t *result = (filter_design_result_t *)
        malloc(sizeof(filter_design_result_t));
    if (result == NULL) return NULL;

    memset(result, 0, sizeof(filter_design_result_t));
    result->spec = *spec;
    result->converged = 0;
    result->iterations = 0;

    /* Get EMC limit */
    emc_limit_summary_t limit = emc_limit_summary_get(spec->standard, 1);

    /* If noise spectrum is provided, calculate required attenuation */
    double il_req_max = 20.0;  /* Default: need at least 20 dB */
    double f_worst = spec->f_switching;  /* Worst frequency: switching fundamental */

    if (noise != NULL && noise->num_bins > 0) {
        il_req_max = 0.0;
        for (size_t i = 0; i < noise->num_bins; i++) {
            double noise_level = noise->amplitude_dbuV[i];
            double limit_level;
            if (noise->frequencies[i] < 500e3)
                limit_level = limit.limit_150k_qp_dbuV;
            else if (noise->frequencies[i] < 5e6)
                limit_level = limit.limit_5M_qp_dbuV;
            else
                limit_level = limit.limit_30M_qp_dbuV;

            double required = noise_level - limit_level + spec->design_margin_db;
            if (required > il_req_max) {
                il_req_max = required;
                f_worst = noise->frequencies[i];
            }
        }
    }

    /* Determine filter order: each LC stage = 40 dB/decade */
    double ratio = f_worst / spec->f_switching;
    double decades = (ratio > 1.0) ? log10(ratio) : 0.0;
    double attenuation_per_stage = 40.0 * decades;
    int n_stages;

    if (attenuation_per_stage < 1.0) {
        n_stages = (int)ceil(il_req_max / 20.0);
        if (n_stages < 1) n_stages = 1;
    } else {
        n_stages = (int)ceil(il_req_max / attenuation_per_stage);
        if (n_stages < 1) n_stages = 1;
    }

    /* Cap at reasonable maximum */
    if (n_stages > 4) n_stages = 4;

    /* Allocate filter stages */
    result->filter.stages = (filter_stage_t *)malloc(
        n_stages * sizeof(filter_stage_t));
    if (result->filter.stages == NULL) {
        free(result);
        return NULL;
    }
    result->filter.num_stages = n_stages;

    /* Design each stage */
    double f_c_stage = spec->f_switching;
    for (int s = 0; s < n_stages; s++) {
        filter_stage_t *stage = &result->filter.stages[s];
        memset(stage, 0, sizeof(filter_stage_t));

        /* Calculate cutoff frequency for this stage
         * f_c = f_noise / 10^(IL_required/40)
         */
        double il_per_stage = il_req_max / n_stages;
        f_c_stage = f_worst / pow(10.0, il_per_stage / 40.0);
        if (f_c_stage > f_worst * 0.5) f_c_stage = f_worst * 0.5;
        if (f_c_stage < spec->f_switching * 0.1) f_c_stage = spec->f_switching * 0.1;

        stage->cutoff_freq_design = f_c_stage;
        stage->topology = (n_stages > 1 || il_req_max > 40.0) ?
                          FILTER_TOPOLOGY_PI : FILTER_TOPOLOGY_LC;

        /* Allocate elements for this stage */
        int n_elem = (stage->topology == FILTER_TOPOLOGY_PI) ? 3 : 2;
        stage->elements = (filter_element_t *)malloc(
            n_elem * sizeof(filter_element_t));
        if (stage->elements == NULL) {
            /* Cleanup partial allocation */
            for (int j = 0; j < s; j++) {
                free(result->filter.stages[j].elements);
            }
            free(result->filter.stages);
            free(result);
            return NULL;
        }
        stage->num_elements = n_elem;

        /* Select L and C values */
        double l_val, c_val;
        filter_select_lc(f_c_stage, spec->z_source_dm, spec->z_lisn,
                         spec->i_nominal, spec->v_nominal, &l_val, &c_val);

        if (stage->topology == FILTER_TOPOLOGY_PI) {
            /* π-filter: C1 — L — C2 */
            stage->elements[0].type = FILTER_ELEM_X_CAPACITOR;
            stage->elements[0].nominal_value = c_val;
            stage->elements[0].tolerance_pct = 20.0;

            stage->elements[1].type = FILTER_ELEM_DM_INDUCTOR;
            stage->elements[1].nominal_value = l_val;
            stage->elements[1].tolerance_pct = 20.0;

            stage->elements[2].type = FILTER_ELEM_X_CAPACITOR;
            stage->elements[2].nominal_value = c_val;
            stage->elements[2].tolerance_pct = 20.0;
        } else {
            /* Simple LC */
            stage->elements[0].type = FILTER_ELEM_DM_INDUCTOR;
            stage->elements[0].nominal_value = l_val;
            stage->elements[0].tolerance_pct = 20.0;

            stage->elements[1].type = FILTER_ELEM_X_CAPACITOR;
            stage->elements[1].nominal_value = c_val;
            stage->elements[1].tolerance_pct = 20.0;
        }

        stage->is_dm_stage = 1;
        result->iterations++;
    }

    /* Estimate insertion loss */
    double estimated_il = n_stages * 40.0 * decades;
    result->filter.dm_insertion_loss = estimated_il;
    result->filter.cm_insertion_loss = estimated_il * 0.7; /* CM harder */
    result->filter.dm_cutoff_freq = f_c_stage;
    result->filter.cm_cutoff_freq = f_c_stage * 2.0;
    result->filter.rated_voltage = spec->v_nominal;
    result->filter.rated_current = spec->i_nominal;
    result->filter.power_loss = spec->i_nominal * spec->i_nominal * 0.1; /* est. */
    result->filter.cmrr_estimate = 40.0; /* Typical CM choke CMRR */
    result->filter.compliance_margin_db = (int)(estimated_il - il_req_max);
    result->achieved_margin_db = estimated_il - il_req_max;
    result->estimated_volume_cm3 = n_stages * 10.0;
    result->estimated_cost_usd = n_stages * 1.5;
    result->converged = (result->filter.compliance_margin_db >= 0) ? 1 : 0;

    return result;
}

void filter_design_result_free(filter_design_result_t *result) {
    if (result) {
        for (size_t i = 0; i < result->filter.num_stages; i++) {
            free(result->filter.stages[i].elements);
        }
        free(result->filter.stages);
        free(result);
    }
}

/* ========================================================================
 * L5: LC Component Value Selection
 *
 * Given a cutoff frequency f_c, there are infinitely many (L, C)
 * pairs satisfying f_c = 1/(2π√(LC)). We optimize by minimizing
 * the stored energy:
 *
 *   E_stored = ½·L·I² + ½·C·V²
 *
 * Subject to impedance matching constraints:
 *   Characteristic impedance: Z₀ = √(L/C)
 *   Ideal: Z₀ = √(Z_source · Z_load)
 *
 * This gives:
 *   L = Z₀ / (2π·f_c)
 *   C = 1 / (2π·f_c·Z₀)
 * ======================================================================== */

void filter_select_lc(double f_c, double z_source, double z_load,
                      double i_rated, double v_rated,
                      double *l_out, double *c_out) {
    (void)i_rated; /* Future: inductor saturation constraint */
    (void)v_rated; /* Future: capacitor voltage derating */
    if (f_c <= 0.0) {
        *l_out = 1e-6;
        *c_out = 1e-6;
        return;
    }

    /* Characteristic impedance: geometric mean of source and load */
    double z0 = sqrt(z_source * z_load);
    if (z0 < 0.1) z0 = 0.1;
    if (z0 > 1000.0) z0 = 1000.0;

    /* L = Z₀ / ω_c,  C = 1 / (ω_c · Z₀) */
    double omega_c = M_2PI * f_c;
    double l_opt = z0 / omega_c;
    double c_opt = 1.0 / (omega_c * z0);

    /* Adjust for rated current and voltage constraints */
    /* Inductor must not saturate at I_rated */
    double l_min = 1e-9;  /* 1 nH minimum */
    double c_max = 1e-3;  /* 1 mF maximum (X-cap limits) */

    if (l_opt < l_min) l_opt = l_min;
    if (c_opt > c_max) c_opt = c_max;

    /* Re-balance: if C is too large, increase L proportionally */
    if (c_opt > c_max) {
        c_opt = c_max;
        l_opt = 1.0 / (omega_c * omega_c * c_opt);
    }

    *l_out = l_opt;
    *c_out = c_opt;
}

/* ========================================================================
 * L7: Application-Specific Filter Presets
 *
 * Provides reasonable default design specifications for common
 * applications based on industry practice and standards.
 *
 * Key references:
 *  - AC-DC SMPS: IEC 61000-3-2 (harmonics), CISPR 32 Class B
 *  - Motor Drive: IEC 61800-3 (adjustable speed drives)
 *  - USB: USB-IF compliance spec
 *  - Ethernet: IEEE 802.3
 * ======================================================================== */

filter_design_spec_t *filter_preset_get(application_preset_t preset) {
    filter_design_spec_t *spec = (filter_design_spec_t *)
        malloc(sizeof(filter_design_spec_t));
    if (spec == NULL) return NULL;

    memset(spec, 0, sizeof(filter_design_spec_t));

    switch (preset) {
        case APP_AC_DC_SMPS:
            /* Typical 65W laptop adapter */
            spec->v_nominal = 230.0;
            spec->i_nominal = 0.3;
            spec->f_grid = 50.0;
            spec->f_switching = 65e3;     /* 65 kHz typical flyback */
            spec->noise_freq_start = 150e3; /* CISPR 32 start */
            spec->noise_freq_end = 30e6;    /* CISPR 32 end */
            spec->standard = EMC_STD_CISPR32;
            spec->design_margin_db = 6.0;
            spec->max_temperature = 65.0;
            spec->is_three_phase = 0;
            spec->z_lisn = 50.0;
            spec->z_source_cm = 50.0;
            spec->z_source_dm = 50.0;
            spec->max_volume_cm3 = 50.0;
            spec->max_cost_usd = 2.0;
            break;

        case APP_MOTOR_DRIVE:
            /* 1 HP (750W) VFD */
            spec->v_nominal = 400.0;
            spec->i_nominal = 2.5;
            spec->f_grid = 50.0;
            spec->f_switching = 8e3;       /* 8 kHz typical VFD */
            spec->noise_freq_start = 150e3;
            spec->noise_freq_end = 30e6;
            spec->standard = EMC_STD_CISPR11;
            spec->design_margin_db = 6.0;
            spec->max_temperature = 85.0;
            spec->is_three_phase = 1;
            spec->z_lisn = 50.0;
            spec->z_source_cm = 150.0;     /* Motor CM impedance higher */
            spec->z_source_dm = 10.0;
            spec->max_volume_cm3 = 500.0;
            spec->max_cost_usd = 15.0;
            break;

        case APP_USB_INTERFACE:
            /* USB 3.0 data lines — CM choke only */
            spec->v_nominal = 5.0;
            spec->i_nominal = 0.9;         /* USB 3.0 max */
            spec->f_grid = 0.0;            /* DC power */
            spec->f_switching = 480e6;     /* USB 2.0 HS data rate */
            spec->noise_freq_start = 30e6;
            spec->noise_freq_end = 1e9;    /* USB 3.0 SS: 5 Gbps */
            spec->standard = EMC_STD_CISPR32;
            spec->design_margin_db = 3.0;
            spec->max_temperature = 45.0;
            spec->is_three_phase = 0;
            spec->z_lisn = 50.0;
            spec->z_source_cm = 90.0;      /* USB differential impedance */
            spec->z_source_dm = 90.0;
            spec->max_volume_cm3 = 0.5;    /* Tiny SMD */
            spec->max_cost_usd = 0.15;
            break;

        case APP_ETHERNET_INTERFACE:
            /* 1000Base-T Ethernet — CM choke for each pair */
            spec->v_nominal = 3.3;
            spec->i_nominal = 0.1;
            spec->f_grid = 0.0;
            spec->f_switching = 125e6;      /* 125 MHz symbol rate */
            spec->noise_freq_start = 30e6;
            spec->noise_freq_end = 1e9;
            spec->standard = EMC_STD_CISPR32;
            spec->design_margin_db = 3.0;
            spec->max_temperature = 45.0;
            spec->is_three_phase = 0;
            spec->z_lisn = 50.0;
            spec->z_source_cm = 100.0;
            spec->z_source_dm = 100.0;
            spec->max_volume_cm3 = 0.3;
            spec->max_cost_usd = 0.10;
            break;

        case APP_MEDICAL_DEVICE:
            /* Medical-grade PSU per IEC 60601-1 */
            spec->v_nominal = 230.0;
            spec->i_nominal = 0.5;
            spec->f_grid = 50.0;
            spec->f_switching = 100e3;
            spec->noise_freq_start = 150e3;
            spec->noise_freq_end = 30e6;
            spec->standard = EMC_STD_CISPR11; /* Medical uses CISPR 11 */
            spec->design_margin_db = 10.0;     /* Higher safety margin */
            spec->max_temperature = 50.0;
            spec->is_three_phase = 0;
            spec->z_lisn = 50.0;
            spec->z_source_cm = 50.0;
            spec->z_source_dm = 50.0;
            spec->max_volume_cm3 = 100.0;
            spec->max_cost_usd = 5.0;
            break;

        case APP_INVERTER_SOLAR:
            /* Solar inverter, 5 kW */
            spec->v_nominal = 400.0;
            spec->i_nominal = 12.5;
            spec->f_grid = 50.0;
            spec->f_switching = 20e3;
            spec->noise_freq_start = 150e3;
            spec->noise_freq_end = 30e6;
            spec->standard = EMC_STD_CISPR11;
            spec->design_margin_db = 6.0;
            spec->max_temperature = 70.0;
            spec->is_three_phase = 1;
            spec->z_lisn = 50.0;
            spec->z_source_cm = 200.0;
            spec->z_source_dm = 5.0;
            spec->max_volume_cm3 = 2000.0;
            spec->max_cost_usd = 40.0;
            break;

        default:
            free(spec);
            return NULL;
    }

    return spec;
}

/* ========================================================================
 * L2: EMC Limit Database
 *
 * Built-in database of conducted emission limits for common standards.
 * Values per CISPR/FCC/MIL published standards.
 *
 * CISPR 32 Class B (residential):
 *   150 kHz – 500 kHz: 66 → 56 dBµV QP (decreasing)
 *   500 kHz – 5 MHz:   56 dBµV QP
 *   5 MHz – 30 MHz:    60 dBµV QP
 *
 * FCC Part 15 Class B:
 *   150 kHz – 500 kHz: 66 → 56 dBµV QP
 *   500 kHz – 5 MHz:   56 dBµV QP
 *   5 MHz – 30 MHz:    60 dBµV QP
 *
 * MIL-STD-461 (CE102):
 *   10 kHz – 500 kHz:  94 → 60 dBµV (decreasing)
 *   500 kHz – 10 MHz:  60 dBµV
 *
 * Reference: CISPR 32:2015, FCC Part 15.107, MIL-STD-461G
 * ======================================================================== */

emc_limit_t *emc_limit_get(emc_standard_t standard, int is_class_a) {
    emc_limit_t *limit = (emc_limit_t *)malloc(sizeof(emc_limit_t));
    if (limit == NULL) return NULL;

    limit->standard = standard;
    limit->is_class_a = is_class_a;

    /* Standard CISPR/FCC limits have 3 segments */
    limit->num_segments = 3;
    limit->segments = (emc_limit_segment_t *)malloc(
        3 * sizeof(emc_limit_segment_t));
    if (limit->segments == NULL) {
        free(limit);
        return NULL;
    }

    switch (standard) {
        case EMC_STD_CISPR32:
        case EMC_STD_CISPR22:
        case EMC_STD_FCC_PART15_A:
        case EMC_STD_FCC_PART15_B:
            if (is_class_a) {
                /* CISPR 32 Class A (commercial) */
                limit->segments[0] = (emc_limit_segment_t){
                    150e3, 500e3, 79.0, 79.0};  /* QP limit */
                limit->segments[1] = (emc_limit_segment_t){
                    500e3, 5e6, 73.0, 73.0};
                limit->segments[2] = (emc_limit_segment_t){
                    5e6, 30e6, 73.0, 73.0};
            } else {
                /* CISPR 32 Class B (residential) */
                limit->segments[0] = (emc_limit_segment_t){
                    150e3, 500e3, 66.0, 56.0};  /* Decreasing to 56 */
                limit->segments[1] = (emc_limit_segment_t){
                    500e3, 5e6, 56.0, 56.0};
                limit->segments[2] = (emc_limit_segment_t){
                    5e6, 30e6, 60.0, 60.0};
            }
            break;

        case EMC_STD_MIL_STD_461:
            /* MIL-STD-461 CE102 */
            limit->segments[0] = (emc_limit_segment_t){
                10e3, 500e3, 94.0, 60.0};  /* Sloped */
            limit->segments[1] = (emc_limit_segment_t){
                500e3, 10e6, 60.0, 60.0};
            limit->segments[2] = (emc_limit_segment_t){
                10e6, 30e6, 60.0, 60.0};
            break;

        case EMC_STD_CISPR11:
            if (is_class_a) {
                limit->segments[0] = (emc_limit_segment_t){
                    150e3, 500e3, 79.0, 79.0};
                limit->segments[1] = (emc_limit_segment_t){
                    500e3, 5e6, 73.0, 73.0};
                limit->segments[2] = (emc_limit_segment_t){
                    5e6, 30e6, 73.0, 73.0};
            } else {
                limit->segments[0] = (emc_limit_segment_t){
                    150e3, 500e3, 66.0, 56.0};
                limit->segments[1] = (emc_limit_segment_t){
                    500e3, 5e6, 56.0, 56.0};
                limit->segments[2] = (emc_limit_segment_t){
                    5e6, 30e6, 60.0, 60.0};
            }
            break;

        case EMC_STD_CISPR25:
            /* Automotive — different frequency plan */
            limit->segments[0] = (emc_limit_segment_t){
                150e3, 5e6, 65.0, 45.0};
            limit->segments[1] = (emc_limit_segment_t){
                5e6, 30e6, 55.0, 55.0};
            limit->segments[2] = (emc_limit_segment_t){
                30e6, 108e6, 45.0, 45.0};  /* Extended to 108 MHz */
            break;

        case EMC_STD_DO_160:
            /* Aerospace DO-160 */
            limit->segments[0] = (emc_limit_segment_t){
                150e3, 500e3, 76.0, 76.0};
            limit->segments[1] = (emc_limit_segment_t){
                500e3, 2e6, 70.0, 70.0};
            limit->segments[2] = (emc_limit_segment_t){
                2e6, 30e6, 60.0, 60.0};
            break;

        default:
            free(limit->segments);
            free(limit);
            return NULL;
    }

    return limit;
}

void emc_limit_free(emc_limit_t *limit) {
    if (limit) {
        free(limit->segments);
        free(limit);
    }
}

/**
 * @brief Check noise spectrum compliance against EMC limit.
 *
 * For each frequency bin, compares measured noise against the
 * applicable limit segment. Uses linear interpolation between
 * limit segment endpoints.
 *
 * Returns 0 if all bins pass, 1 if any bin fails.
 */
int emc_compliance_check(const noise_spectrum_t *noise,
                         const emc_limit_t *limit) {
    if (noise == NULL || limit == NULL) return -1;
    if (noise->num_bins == 0) return -1;

    int fail = 0;

    for (size_t i = 0; i < noise->num_bins; i++) {
        double f = noise->frequencies[i];
        double amp = noise->amplitude_dbuV[i];

        /* Find applicable segment */
        double limit_val = 0.0;
        int found = 0;
        for (size_t j = 0; j < limit->num_segments; j++) {
            if (f >= limit->segments[j].freq_start &&
                f <= limit->segments[j].freq_end) {
                /* Linear interpolation between segment endpoints */
                double t = (f - limit->segments[j].freq_start) /
                          (limit->segments[j].freq_end - limit->segments[j].freq_start);
                double qp_start = limit->segments[j].limit_dbuV_qp;
                double qp_end;
                /* Check if the next segment exists for interpolation */
                if (j + 1 < limit->num_segments) {
                    qp_end = limit->segments[j + 1].limit_dbuV_qp;
                } else {
                    qp_end = qp_start;
                }
                limit_val = qp_start + t * (qp_end - qp_start);
                found = 1;
                break;
            }
        }

        if (found && amp > limit_val) {
            fail = 1;
        }
    }

    return fail;
}

/* ========================================================================
 * L5: Required Attenuation Calculation
 *
 * IL_req(f) = max(0, Noise(f) − Limit(f) + Margin)  [dB]
 *
 * This function computes the required insertion loss at each frequency
 * to bring the measured noise below the regulatory limit with the
 * specified design margin.
 *
 * The maximum IL_req determines the filter order and cutoff frequency.
 * ======================================================================== */

double *required_attenuation(const noise_spectrum_t *noise,
                              const emc_limit_t *limit,
                              double margin_db, size_t *n_out) {
    if (noise == NULL || limit == NULL || n_out == NULL) return NULL;

    *n_out = noise->num_bins;
    double *il_req = (double *)malloc(*n_out * sizeof(double));
    if (il_req == NULL) return NULL;

    emc_limit_summary_t ls = emc_limit_summary_get(limit->standard,
                                                    limit->is_class_a);

    for (size_t i = 0; i < noise->num_bins; i++) {
        double f = noise->frequencies[i];
        double noise_level = noise->amplitude_dbuV[i];

        /* Find applicable limit at this frequency */
        double limit_level;
        if (f < 500e3) {
            limit_level = ls.limit_150k_qp_dbuV;
            /* Interpolate if limit changes in this band */
            if (limit_level != ls.limit_500k_qp_dbuV) {
                double t = (f - 150e3) / (500e3 - 150e3);
                limit_level = ls.limit_150k_qp_dbuV +
                    t * (ls.limit_500k_qp_dbuV - ls.limit_150k_qp_dbuV);
            }
        } else if (f < 5e6) {
            limit_level = ls.limit_500k_qp_dbuV;
        } else {
            limit_level = ls.limit_5M_qp_dbuV;
        }

        double required = noise_level - limit_level + margin_db;
        il_req[i] = (required > 0.0) ? required : 0.0;
    }

    return il_req;
}

/* ========================================================================
 * L5: Minimum Filter Order Determination
 *
 * The filter order determines the asymptotic roll-off rate:
 *   1st order (L or C only):     20 dB/decade
 *   2nd order (LC):              40 dB/decade
 *   3rd order (Π or T):          60 dB/decade
 *   Nth order:                   20·N dB/decade
 *
 * Required order: N ≥ IL_req / (20 · log10(f_noise / f_cutoff))
 *
 * ======================================================================== */

int minimum_filter_order(const double *il_req_db, const double *freqs,
                          size_t n, double f_cutoff) {
    if (il_req_db == NULL || freqs == NULL || n == 0 || f_cutoff <= 0.0) {
        return 1;
    }

    double max_il = 0.0;
    double f_worst = 1e3;

    for (size_t i = 0; i < n; i++) {
        if (il_req_db[i] > max_il) {
            max_il = il_req_db[i];
            f_worst = freqs[i];
        }
    }

    if (max_il <= 0.0) return 1;

    double decades = log10(f_worst / f_cutoff);
    if (decades <= 0.0) decades = 0.1;  /* Minimum one decade estimate */

    int order = (int)ceil(max_il / (20.0 * decades));
    if (order < 1) order = 1;
    if (order > 6) order = 6;  /* Practical limit */

    return order;
}

/* ========================================================================
 * L3: Admittance Matrix for Filter Network
 *
 * For a network with N nodes, the nodal admittance matrix Y is an
 * N×N complex matrix where Y_ij represents the admittance connecting
 * nodes i and j.
 *
 * The network equations are: Y·V = I
 *
 * Element contributions:
 *   Series impedance Z between nodes i and j:
 *     Y_ii += 1/Z, Y_jj += 1/Z, Y_ij -= 1/Z, Y_ji -= 1/Z
 *
 *   Shunt admittance Y from node i to ground:
 *     Y_ii += Y
 * ======================================================================== */

complex_t *build_admittance_matrix(const filter_element_t *elements,
                                    size_t n_elem, double freq,
                                    size_t *n_nodes) {
    if (elements == NULL || n_elem == 0 || n_nodes == NULL) return NULL;

    /* Simple two-port model: 2 nodes (input and output) */
    *n_nodes = 2;
    size_t n = *n_nodes;
    complex_t *y = (complex_t *)calloc(n * n, sizeof(complex_t));
    if (y == NULL) return NULL;

    /* For a simple 2-port Π filter: C1→gnd, L1→(i-o), C2→gnd */
    /* Node 0 = input, Node 1 = output */
    double c1 = 0.0, c2 = 0.0, l1 = 0.0;
    for (size_t i = 0; i < n_elem; i++) {
        if (elements[i].type == FILTER_ELEM_X_CAPACITOR ||
            elements[i].type == FILTER_ELEM_Y_CAPACITOR) {
            if (c1 < 1e-30) c1 = elements[i].nominal_value;
            else c2 = elements[i].nominal_value;
        } else if (elements[i].type == FILTER_ELEM_DM_INDUCTOR ||
                   elements[i].type == FILTER_ELEM_CM_CHOKE) {
            l1 = elements[i].nominal_value;
        }
    }

    double omega = M_2PI * freq;
    if (c1 > 1e-30) {
        /* Ground admittance at node 0 */
    }
    if (l1 > 1e-30 && omega > 1e-30) {
        complex_t y_series = {0.0, -1.0 / (omega * l1)};
        /* Y[0][0] += Y_series, Y[0][1] -= Y_series */
        y[0 * n + 0] = complex_add(y[0 * n + 0], y_series);
        y[0 * n + 1] = complex_sub(y[0 * n + 1], y_series);
        /* Y[1][0] -= Y_series, Y[1][1] += Y_series */
        y[1 * n + 0] = complex_sub(y[1 * n + 0], y_series);
        y[1 * n + 1] = complex_add(y[1 * n + 1], y_series);
    }

    /* Shunt capacitance to ground at node 0 */
    if (c1 > 1e-30) {
        complex_t y_c1 = {0.0, omega * c1};
        y[0 * n + 0] = complex_add(y[0 * n + 0], y_c1);
    }

    /* Shunt capacitance to ground at node 1 */
    if (c2 > 1e-30) {
        complex_t y_c2 = {0.0, omega * c2};
        y[1 * n + 1] = complex_add(y[1 * n + 1], y_c2);
    }

    return y;
}

/* ========================================================================
 * L3: Complex Linear System Solver (Gaussian Elimination)
 *
 * Solves Y·V = I for V, where Y is N×N complex matrix.
 * Uses Gaussian elimination with partial pivoting for numerical
 * stability.
 *
 * Complexity: O(N³)
 * Uses: Complex arithmetic implemented in this module.
 * ======================================================================== */

int solve_complex_linear_system(const complex_t *y_mat,
                                 const complex_t *i_vec,
                                 complex_t *v_out, size_t n) {
    if (y_mat == NULL || i_vec == NULL || v_out == NULL || n == 0) {
        return -1;
    }

    /* Create augmented matrix [Y | I] */
    complex_t *aug = (complex_t *)malloc(n * (n + 1) * sizeof(complex_t));
    if (aug == NULL) return -1;

    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < n; j++) {
            aug[i * (n + 1) + j] = y_mat[i * n + j];
        }
        aug[i * (n + 1) + n] = i_vec[i];
    }

    /* Forward elimination with partial pivoting */
    for (size_t col = 0; col < n; col++) {
        /* Find pivot row */
        size_t pivot = col;
        double max_mag = complex_mag(aug[col * (n + 1) + col]);
        for (size_t row = col + 1; row < n; row++) {
            double mag = complex_mag(aug[row * (n + 1) + col]);
            if (mag > max_mag) {
                max_mag = mag;
                pivot = row;
            }
        }

        if (max_mag < 1e-30) {
            free(aug);
            return -1;  /* Singular matrix */
        }

        /* Swap rows if needed */
        if (pivot != col) {
            for (size_t j = 0; j <= n; j++) {
                complex_t tmp = aug[col * (n + 1) + j];
                aug[col * (n + 1) + j] = aug[pivot * (n + 1) + j];
                aug[pivot * (n + 1) + j] = tmp;
            }
        }

        /* Eliminate rows below pivot */
        complex_t pivot_val = aug[col * (n + 1) + col];
        for (size_t row = col + 1; row < n; row++) {
            complex_t factor = complex_div(aug[row * (n + 1) + col], pivot_val);
            for (size_t j = col; j <= n; j++) {
                aug[row * (n + 1) + j] = complex_sub(
                    aug[row * (n + 1) + j],
                    complex_mul(factor, aug[col * (n + 1) + j])
                );
            }
        }
    }

    /* Back substitution */
    for (size_t i = n; i > 0; i--) {
        size_t row = i - 1;
        complex_t sum = {0.0, 0.0};
        for (size_t j = row + 1; j < n; j++) {
            sum = complex_add(sum,
                complex_mul(aug[row * (n + 1) + j], v_out[j]));
        }
        v_out[row] = complex_div(
            complex_sub(aug[row * (n + 1) + n], sum),
            aug[row * (n + 1) + row]
        );
    }

    free(aug);
    return 0;
}
