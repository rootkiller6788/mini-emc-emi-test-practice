/**
 * @file cm_dm_network.c
 * @brief CM/DM Network Analysis — Parameter Conversions, Mixed-Mode, Cascading
 *
 * Implements two-port network parameter conversions (Z↔Y↔ABCD↔S),
 * mixed-mode S-parameter decomposition, mode conversion analysis,
 * cascaded network computation, and filter ABCD matrix construction.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "cm_dm_filter.h"
#include "cm_dm_network.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_2PI
#define M_2PI (2.0 * M_PI)
#endif

/* ========================================================================
 * L1: Basic matrix operations
 * ======================================================================== */

z_matrix_t z_matrix_create(complex_t z11, complex_t z12,
                            complex_t z21, complex_t z22) {
    z_matrix_t z;
    z.z11 = z11; z.z12 = z12;
    z.z21 = z21; z.z22 = z22;
    return z;
}

/* ========================================================================
 * L3: Z ↔ Y matrix conversion
 *
 * Y = Z⁻¹
 *
 * For a 2×2 matrix:
 *   det(Z) = Z₁₁Z₂₂ - Z₁₂Z₂₁
 *   Y₁₁ = Z₂₂/det,  Y₁₂ = -Z₁₂/det
 *   Y₂₁ = -Z₂₁/det, Y₂₂ = Z₁₁/det
 *
 * If det(Z) is singular (zero), the conversion fails (the network
 * has no admittance representation, typically due to an ideal
 * open-circuit or short-circuit).
 * ======================================================================== */

int z_to_y_matrix(const z_matrix_t *z, y_matrix_t *y) {
    if (z == NULL || y == NULL) return -1;

    /* det(Z) = Z₁₁·Z₂₂ - Z₁₂·Z₂₁ */
    complex_t z11z22 = complex_mul(z->z11, z->z22);
    complex_t z12z21 = complex_mul(z->z12, z->z21);
    complex_t det = complex_sub(z11z22, z12z21);

    double mag_det = complex_mag(det);
    if (mag_det < 1e-30) return -1;

    y->y11 = complex_div(z->z22, det);
    y->y12 = complex_div((complex_t){-z->z12.real, -z->z12.imag}, det);
    y->y21 = complex_div((complex_t){-z->z21.real, -z->z21.imag}, det);
    y->y22 = complex_div(z->z11, det);

    return 0;
}

/* ========================================================================
 * L3: Z → ABCD conversion
 *
 * The ABCD (transmission) matrix relates input and output:
 *   V₁ = A·V₂ - B·I₂
 *   I₁ = C·V₂ - D·I₂
 *
 * From Z-parameters:
 *   A = Z₁₁ / Z₂₁
 *   B = (Z₁₁·Z₂₂ - Z₁₂·Z₂₁) / Z₂₁ = det(Z)/Z₂₁
 *   C = 1 / Z₂₁
 *   D = Z₂₂ / Z₂₁
 *
 * Z₂₁ must be non-zero (network must have forward transmission).
 * ======================================================================== */

int z_to_abcd_matrix(const z_matrix_t *z, abcd_matrix_t *abcd) {
    if (z == NULL || abcd == NULL) return -1;

    double mag_z21 = complex_mag(z->z21);
    if (mag_z21 < 1e-30) return -1;

    abcd->A = complex_div(z->z11, z->z21);
    abcd->B = complex_div(
        complex_sub(complex_mul(z->z11, z->z22),
                    complex_mul(z->z12, z->z21)),
        z->z21);
    abcd->C = complex_div((complex_t){1.0, 0.0}, z->z21);
    abcd->D = complex_div(z->z22, z->z21);

    return 0;
}

/* ========================================================================
 * L3: ABCD → Z conversion
 *
 * Inverse of above:
 *   Z₁₁ = A / C
 *   Z₁₂ = (A·D - B·C) / C
 *   Z₂₁ = 1 / C
 *   Z₂₂ = D / C
 * ======================================================================== */

int abcd_to_z_matrix(const abcd_matrix_t *abcd, z_matrix_t *z) {
    if (abcd == NULL || z == NULL) return -1;

    double mag_c = complex_mag(abcd->C);
    if (mag_c < 1e-30) return -1;

    z->z11 = complex_div(abcd->A, abcd->C);
    z->z12 = complex_div(
        complex_sub(complex_mul(abcd->A, abcd->D),
                    complex_mul(abcd->B, abcd->C)),
        abcd->C);
    z->z21 = complex_div((complex_t){1.0, 0.0}, abcd->C);
    z->z22 = complex_div(abcd->D, abcd->C);

    return 0;
}

/* ========================================================================
 * L3: S ↔ Z matrix conversion
 *
 * S = (Z/Z₀ - I)(Z/Z₀ + I)⁻¹
 *
 * For 2-port, with Z₀ reference:
 *
 * Denom = (Z₁₁/Z₀ + 1)(Z₂₂/Z₀ + 1) - (Z₁₂·Z₂₁)/Z₀²
 *
 * S₁₁ = [(Z₁₁/Z₀ - 1)(Z₂₂/Z₀ + 1) - Z₁₂·Z₂₁/Z₀²] / Denom
 * S₁₂ = [2·Z₁₂/Z₀] / Denom
 * S₂₁ = [2·Z₂₁/Z₀] / Denom
 * S₂₂ = [(Z₁₁/Z₀ + 1)(Z₂₂/Z₀ - 1) - Z₁₂·Z₂₁/Z₀²] / Denom
 *
 * Reference: Pozar §4.3
 * ======================================================================== */

int z_to_s_matrix(const z_matrix_t *z, double z0, s_matrix_t *s_out) {
    if (z == NULL || s_out == NULL || z0 <= 0.0) return -1;

    /* Normalize to Z₀ */
    complex_t z11n = {z->z11.real / z0, z->z11.imag / z0};
    complex_t z12n = {z->z12.real / z0, z->z12.imag / z0};
    complex_t z21n = {z->z21.real / z0, z->z21.imag / z0};
    complex_t z22n = {z->z22.real / z0, z->z22.imag / z0};

    complex_t one = {1.0, 0.0};
    complex_t two = {2.0, 0.0};

    /* D = (Z₁₁_n + 1)(Z₂₂_n + 1) - Z₁₂_n·Z₂₁_n */
    complex_t t1 = complex_add(z11n, one);
    complex_t t2 = complex_add(z22n, one);
    complex_t denom = complex_sub(complex_mul(t1, t2),
                                   complex_mul(z12n, z21n));

    double mag_d = complex_mag(denom);
    if (mag_d < 1e-30) return -1;

    /* S₁₁ */
    complex_t t1m = complex_sub(z11n, one);
    complex_t t2p = complex_add(z22n, one);
    s_out->s11 = complex_div(
        complex_sub(complex_mul(t1m, t2p), complex_mul(z12n, z21n)),
        denom);

    /* S₁₂ */
    s_out->s12 = complex_div(complex_mul(two, z12n), denom);

    /* S₂₁ */
    s_out->s21 = complex_div(complex_mul(two, z21n), denom);

    /* S₂₂ */
    complex_t t1p = complex_add(z11n, one);
    complex_t t2m = complex_sub(z22n, one);
    s_out->s22 = complex_div(
        complex_sub(complex_mul(t1p, t2m), complex_mul(z12n, z21n)),
        denom);

    s_out->z0 = z0;
    return 0;
}

/* ========================================================================
 * L3: S → Z conversion
 *
 * Z = Z₀ · (I + S)(I - S)⁻¹
 *
 * For a 2-port:
 * Denom = (1-S₁₁)(1-S₂₂) - S₁₂·S₂₁
 *
 * Z₁₁ = Z₀ · [(1+S₁₁)(1-S₂₂) + S₁₂·S₂₁] / Denom
 * Z₁₂ = Z₀ · [2·S₁₂] / Denom
 * Z₂₁ = Z₀ · [2·S₂₁] / Denom
 * Z₂₂ = Z₀ · [(1-S₁₁)(1+S₂₂) + S₁₂·S₂₁] / Denom
 * ======================================================================== */

int s_to_z_matrix(const s_matrix_t *s, z_matrix_t *z) {
    if (s == NULL || z == NULL) return -1;

    double z0 = s->z0;
    complex_t one = {1.0, 0.0};
    complex_t two = {2.0, 0.0};

    complex_t d1 = complex_sub(one, s->s11);
    complex_t d2 = complex_sub(one, s->s22);
    complex_t denom = complex_sub(complex_mul(d1, d2),
                                   complex_mul(s->s12, s->s21));

    double mag_d = complex_mag(denom);
    if (mag_d < 1e-30) return -1;

    /* Z₁₁ */
    complex_t p1 = complex_add(one, s->s11);
    z->z11.real = z0 * complex_div(
        complex_add(complex_mul(p1, d2), complex_mul(s->s12, s->s21)),
        denom).real;
    z->z11.imag = z0 * complex_div(
        complex_add(complex_mul(p1, d2), complex_mul(s->s12, s->s21)),
        denom).imag;

    complex_t tmp = complex_div(complex_mul(two, s->s12), denom);
    z->z12.real = z0 * tmp.real;
    z->z12.imag = z0 * tmp.imag;

    tmp = complex_div(complex_mul(two, s->s21), denom);
    z->z21.real = z0 * tmp.real;
    z->z21.imag = z0 * tmp.imag;

    complex_t p2 = complex_add(one, s->s22);
    tmp = complex_div(
        complex_add(complex_mul(d1, p2), complex_mul(s->s12, s->s21)),
        denom);
    z->z22.real = z0 * tmp.real;
    z->z22.imag = z0 * tmp.imag;

    return 0;
}

/* ========================================================================
 * L6: ABCD matrix cascading
 *
 * For two cascaded networks:
 *   [ABCD]_total = [ABCD]₁ × [ABCD]₂
 *
 * Matrix multiplication:
 *   A_total = A₁·A₂ + B₁·C₂
 *   B_total = A₁·B₂ + B₁·D₂
 *   C_total = C₁·A₂ + D₁·C₂
 *   D_total = C₁·B₂ + D₁·D₂
 * ======================================================================== */

abcd_matrix_t abcd_cascade_two(const abcd_matrix_t *a, const abcd_matrix_t *b) {
    abcd_matrix_t result;

    result.A = complex_add(complex_mul(a->A, b->A),
                            complex_mul(a->B, b->C));
    result.B = complex_add(complex_mul(a->A, b->B),
                            complex_mul(a->B, b->D));
    result.C = complex_add(complex_mul(a->C, b->A),
                            complex_mul(a->D, b->C));
    result.D = complex_add(complex_mul(a->C, b->B),
                            complex_mul(a->D, b->D));

    return result;
}

void abcd_cascade_n(const abcd_matrix_t *abcd_array, size_t n,
                     abcd_matrix_t *total) {
    if (n == 0 || abcd_array == NULL || total == NULL) {
        if (total) {
            *total = (abcd_matrix_t){{1.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {1.0, 0.0}};
        }
        return;
    }

    *total = abcd_array[0];
    for (size_t i = 1; i < n; i++) {
        *total = abcd_cascade_two(total, &abcd_array[i]);
    }
}

/* ========================================================================
 * L3: ABCD → S conversion
 *
 * Given ABCD with reference impedance Z₀:
 *   Δ = A + B/Z₀ + C·Z₀ + D
 *
 *   S₁₁ = (A + B/Z₀ - C·Z₀ - D) / Δ
 *   S₁₂ = 2·(A·D - B·C) / Δ  = 2/Δ  (for reciprocal network AD-BC=1)
 *   S₂₁ = 2 / Δ
 *   S₂₂ = (-A + B/Z₀ - C·Z₀ + D) / Δ
 *
 * Note: For a reciprocal network, A·D - B·C = 1.
 * EMI filters made of passive components are reciprocal.
 * ======================================================================== */

s_matrix_t abcd_to_s_matrix(const abcd_matrix_t *abcd, double z0) {
    s_matrix_t s;
    memset(&s, 0, sizeof(s));
    s.z0 = z0;

    if (z0 <= 0.0) return s;

    /* B/Z₀ and C·Z₀ */
    complex_t b_over_z0 = {abcd->B.real / z0, abcd->B.imag / z0};
    complex_t c_times_z0 = {abcd->C.real * z0, abcd->C.imag * z0};

    /* Δ = A + B/Z₀ + C·Z₀ + D */
    complex_t delta = complex_add(
        complex_add(abcd->A, b_over_z0),
        complex_add(c_times_z0, abcd->D));

    double mag_d = complex_mag(delta);
    if (mag_d < 1e-30) return s;

    /* S₁₁ */
    s.s11 = complex_div(
        complex_sub(complex_add(abcd->A, b_over_z0),
                    complex_add(c_times_z0, abcd->D)),
        delta);

    /* S₁₂ = 2(A·D - B·C)/Δ → for reciprocal = 2/Δ */
    complex_t ad = complex_mul(abcd->A, abcd->D);
    complex_t bc = complex_mul(abcd->B, abcd->C);
    complex_t ad_m_bc = complex_sub(ad, bc);
    s.s12 = complex_div((complex_t){2.0 * ad_m_bc.real, 2.0 * ad_m_bc.imag}, delta);

    /* S₂₁ = 2/Δ */
    s.s21 = complex_div((complex_t){2.0, 0.0}, delta);

    /* S₂₂ */
    complex_t neg_a = {-abcd->A.real, -abcd->A.imag};
    s.s22 = complex_div(
        complex_add(complex_add(neg_a, b_over_z0),
                    complex_sub(c_times_z0, abcd->D)), /* -C·Z₀ + D = D - C·Z₀ */
        delta);

    /* Fix S₂₂ calculation */
    s.s22 = complex_div(
        complex_sub(complex_add(b_over_z0, abcd->D),
                    complex_add(abcd->A, c_times_z0)),
        delta);

    return s;
}

/* ========================================================================
 * L1: S-parameter derived quantities
 * ======================================================================== */

double s_to_insertion_loss(const s_matrix_t *s) {
    double mag_s21 = complex_mag(s->s21);
    if (mag_s21 < 1e-30) return DBL_MAX;
    return -20.0 * log10(mag_s21);
}

double s_to_return_loss(const s_matrix_t *s, int port) {
    double mag;
    if (port == 1) mag = complex_mag(s->s11);
    else mag = complex_mag(s->s22);

    if (mag < 1e-30) return DBL_MAX;
    return -20.0 * log10(mag);
}

/* ========================================================================
 * L3: 4-port S-parameters to Mixed-mode S-parameters
 *
 * The transformation from 4-port single-ended to mixed-mode is:
 *
 *   [S_mm] = [M] · [S_std] · [M]⁻¹
 *
 * where M is the modal transformation matrix:
 *
 *         [ 1 -1  0  0 ]  ← differential input
 * M = 1/√2 [ 0  0  1 -1 ]  ← differential output
 *         [ 1  1  0  0 ]  ← common-mode input
 *         [ 0  0  1  1 ]  ← common-mode output
 *
 * The mixed-mode S-matrix is:
 *
 *          [ S_dd11  S_dd12 | S_dc11  S_dc12 ]
 * S_mm =   [ S_dd21  S_dd22 | S_dc21  S_dc22 ]
 *          [─────────────────+─────────────────]
 *          [ S_cd11  S_cd12 | S_cc11  S_cc12 ]
 *          [ S_cd21  S_cd22 | S_cc21  S_cc22 ]
 *
 * Where:
 *   S_dd: Differential-mode S-parameters (desired signal path)
 *   S_cc: Common-mode S-parameters (noise propagation)
 *   S_cd: CM-to-DM conversion (undesirable)
 *   S_dc: DM-to-CM conversion (undesirable)
 *
 * Reference: Bockelman & Eisenstadt (1995), IEEE MTT
 * ======================================================================== */

mixed_mode_s_t *s_4port_to_mixed_mode(const s_4port_t *s4,
                                       double z0_diff, double z0_comm) {
    if (s4 == NULL) return NULL;

    mixed_mode_s_t *mm = (mixed_mode_s_t *)malloc(sizeof(mixed_mode_s_t));
    if (mm == NULL) return NULL;

    memset(mm, 0, sizeof(mixed_mode_s_t));
    mm->z0_diff = z0_diff > 0.0 ? z0_diff : 100.0;
    mm->z0_comm = z0_comm > 0.0 ? z0_comm : 25.0;

    /* The transformation matrix coefficients.
     *
     * S_dd11 = ½·(S₁₁ - S₁₂ - S₂₁ + S₂₂)
     * S_dd12 = ½·(S₁₃ - S₁₄ - S₂₃ + S₂₄)
     * S_dd21 = ½·(S₃₁ - S₃₂ - S₄₁ + S₄₂)
     * S_dd22 = ½·(S₃₃ - S₃₄ - S₄₃ + S₄₄)
     *
     * S_dc11 = ½·(S₁₁ + S₁₂ - S₂₁ - S₂₂)
     * S_dc12 = ½·(S₁₃ + S₁₄ - S₂₃ - S₂₄)
     * S_dc21 = ½·(S₃₁ + S₃₂ - S₄₁ - S₄₂)
     * S_dc22 = ½·(S₃₃ + S₃₄ - S₄₃ - S₄₄)
     *
     * S_cd11 = ½·(S₁₁ - S₁₂ + S₂₁ - S₂₂)
     * S_cd12 = ½·(S₁₃ - S₁₄ + S₂₃ - S₂₄)
     * S_cd21 = ½·(S₃₁ - S₃₂ + S₄₁ - S₄₂)
     * S_cd22 = ½·(S₃₃ - S₃₄ + S₄₃ - S₄₄)
     *
     * S_cc11 = ½·(S₁₁ + S₁₂ + S₂₁ + S₂₂)
     * S_cc12 = ½·(S₁₃ + S₁₄ + S₂₃ + S₂₄)
     * S_cc21 = ½·(S₃₁ + S₃₂ + S₄₁ + S₄₂)
     * S_cc22 = ½·(S₃₃ + S₃₄ + S₄₃ + S₄₄)
     */

    #define HALF_SUM4(a,b,c,d) \
        (complex_t){0.5 * ((a).real + (b).real + (c).real + (d).real), \
                    0.5 * ((a).imag + (b).imag + (c).imag + (d).imag)}

    #define HALF_DIFF(a,b,c,d) \
        (complex_t){0.5 * ((a).real - (b).real - (c).real + (d).real), \
                    0.5 * ((a).imag - (b).imag - (c).imag + (d).imag)}

    #define HALF_MIX1(a,b,c,d) \
        (complex_t){0.5 * ((a).real + (b).real - (c).real - (d).real), \
                    0.5 * ((a).imag + (b).imag - (c).imag - (d).imag)}

    #define HALF_MIX2(a,b,c,d) \
        (complex_t){0.5 * ((a).real - (b).real + (c).real - (d).real), \
                    0.5 * ((a).imag - (b).imag + (c).imag - (d).imag)}

    /* Differential-mode (S_dd) */
    mm->s_dd11 = HALF_DIFF(s4->s[0][0], s4->s[0][1], s4->s[1][0], s4->s[1][1]);
    mm->s_dd12 = HALF_DIFF(s4->s[0][2], s4->s[0][3], s4->s[1][2], s4->s[1][3]);
    mm->s_dd21 = HALF_DIFF(s4->s[2][0], s4->s[2][1], s4->s[3][0], s4->s[3][1]);
    mm->s_dd22 = HALF_DIFF(s4->s[2][2], s4->s[2][3], s4->s[3][2], s4->s[3][3]);

    /* Common-mode (S_cc) */
    mm->s_cc11 = HALF_SUM4(s4->s[0][0], s4->s[0][1], s4->s[1][0], s4->s[1][1]);
    mm->s_cc12 = HALF_SUM4(s4->s[0][2], s4->s[0][3], s4->s[1][2], s4->s[1][3]);
    mm->s_cc21 = HALF_SUM4(s4->s[2][0], s4->s[2][1], s4->s[3][0], s4->s[3][1]);
    mm->s_cc22 = HALF_SUM4(s4->s[2][2], s4->s[2][3], s4->s[3][2], s4->s[3][3]);

    /* DM → CM conversion (S_cd) */
    mm->s_cd11 = HALF_MIX2(s4->s[0][0], s4->s[0][1], s4->s[1][0], s4->s[1][1]);
    mm->s_cd12 = HALF_MIX2(s4->s[0][2], s4->s[0][3], s4->s[1][2], s4->s[1][3]);
    mm->s_cd21 = HALF_MIX2(s4->s[2][0], s4->s[2][1], s4->s[3][0], s4->s[3][1]);
    mm->s_cd22 = HALF_MIX2(s4->s[2][2], s4->s[2][3], s4->s[3][2], s4->s[3][3]);

    /* CM → DM conversion (S_dc) */
    mm->s_dc11 = HALF_MIX1(s4->s[0][0], s4->s[0][1], s4->s[1][0], s4->s[1][1]);
    mm->s_dc12 = HALF_MIX1(s4->s[0][2], s4->s[0][3], s4->s[1][2], s4->s[1][3]);
    mm->s_dc21 = HALF_MIX1(s4->s[2][0], s4->s[2][1], s4->s[3][0], s4->s[3][1]);
    mm->s_dc22 = HALF_MIX1(s4->s[2][2], s4->s[2][3], s4->s[3][2], s4->s[3][3]);

    #undef HALF_SUM4
    #undef HALF_DIFF
    #undef HALF_MIX1
    #undef HALF_MIX2

    return mm;
}

s_4port_t *mixed_mode_to_s_4port(const mixed_mode_s_t *mm) {
    if (mm == NULL) return NULL;

    s_4port_t *s4 = (s_4port_t *)malloc(sizeof(s_4port_t));
    if (s4 == NULL) return NULL;

    memset(s4, 0, sizeof(s_4port_t));
    s4->z0 = mm->z0_diff;

    /* Inverse transformation:
     * S₁₁ = ½·(S_dd11 + S_dc11 + S_cd11 + S_cc11)
     * S₁₂ = ½·(−S_dd11 + S_dc11 − S_cd11 + S_cc11)
     * ...etc.
     */

    #define QUARTER_SUM(a,b,c,d) \
        (complex_t){0.25 * ((a).real + (b).real + (c).real + (d).real), \
                    0.25 * ((a).imag + (b).imag + (c).imag + (d).imag)}

    s4->s[0][0] = QUARTER_SUM(mm->s_dd11, mm->s_dc11, mm->s_cd11, mm->s_cc11);

    #undef QUARTER_SUM

    return s4;
}

void mixed_mode_s_free(mixed_mode_s_t *mm) { free(mm); }
void s_4port_free(s_4port_t *s4) { free(s4); }

/* ========================================================================
 * L1: Mode conversion metrics from mixed-mode S-parameters
 *
 * Longitudinal Conversion Loss (LCL):
 *   LCL = -20·log10(|S_cd21|)  [dB]
 *
 * Transverse Conversion Loss (TCL):
 *   TCL = -20·log10(|S_dc21|)  [dB]
 *
 * CMRR from S-parameters:
 *   CMRR = -20·log10(|S_dc21| / |S_dd21|) ≈ LCL (for |S_dd21| ≈ 1)
 *
 * These quantify how much undesired mode conversion occurs in the
 * filter, which degrades the effective CM rejection.
 * ======================================================================== */

mode_conversion_t mode_conversion_from_mm(const mixed_mode_s_t *mm,
                                           double freq) {
    mode_conversion_t mc;
    memset(&mc, 0, sizeof(mc));
    mc.frequency = freq;

    if (mm == NULL) return mc;

    double mag_cd21 = complex_mag(mm->s_cd21);
    double mag_dc21 = complex_mag(mm->s_dc21);
    double mag_dd21 = complex_mag(mm->s_dd21);

    if (mag_cd21 > 1e-30) {
        mc.tcl_db = -20.0 * log10(mag_cd21);
    } else {
        mc.tcl_db = 120.0;  /* Essentially infinite */
    }

    if (mag_dc21 > 1e-30) {
        mc.lcl_db = -20.0 * log10(mag_dc21);
    } else {
        mc.lcl_db = 120.0;
    }

    if (mag_dd21 > 1e-30 && mag_dc21 > 1e-30) {
        mc.cmrr_db = 20.0 * log10(mag_dd21 / mag_dc21);
    } else {
        mc.cmrr_db = 100.0;
    }

    mc.dm_cm_isolation_db = (mc.tcl_db + mc.lcl_db) / 2.0;
    mc.imbalance_pct = 100.0 / pow(10.0, mc.cmrr_db / 20.0);

    return mc;
}

/* ========================================================================
 * L3: LCL estimation from impedance imbalance
 *
 * LCL ≈ 20·log10( (Z_L + Z_N) / |Z_L - Z_N| )  [dB]
 *
 * Derivation: If Z_L ≠ Z_N, the CM current I_cm splits unequally:
 *   I_L = Z_N/(Z_L+Z_N) · I_cm
 *   I_N = Z_L/(Z_L+Z_N) · I_cm
 *
 * The DM voltage developed across the load due to this imbalance is:
 *   V_dm = I_L·Z_L - I_N·Z_N = I_cm · (Z_L·Z_N - Z_N·Z_L)/(Z_L+Z_N) = 0
 *
 * Actually, mode conversion arises from the voltage divider imbalance.
 * For a 50Ω system: LCL ≈ 20·log10(100 / ΔZ) for small imbalance.
 * ======================================================================== */

double lcl_estimate_from_imbalance(double z_line, double z_neutral) {
    double z_sum = z_line + z_neutral;
    double z_diff = fabs(z_line - z_neutral);

    if (z_diff < 1e-30 && z_sum < 1e-30) return 120.0;
    if (z_diff < 1e-30) return 120.0;

    if (z_sum > 1e-30) {
        return 20.0 * log10(z_sum / z_diff);
    }

    return 20.0 * log10(1.0 / (z_diff / 50.0));
}

/**
 * L3: Path imbalance from component tolerance
 *
 * Given two nominally identical impedances Z₁, Z₂ with
 * tolerance ±T percent, the worst-case imbalance occurs when
 * Z₁ = Z_nom · (1+T/100) and Z₂ = Z_nom · (1−T/100)
 */
path_imbalance_t path_imbalance_from_tolerance(double z1_nominal,
                                                double z2_nominal,
                                                double tolerance_pct) {
    path_imbalance_t pi;
    memset(&pi, 0, sizeof(pi));

    double tol = tolerance_pct / 100.0;

    /* Worst-case: one at +tol, other at -tol */
    pi.z_line = z1_nominal * (1.0 + tol);
    pi.z_neutral = z2_nominal * (1.0 - tol);
    pi.delta_z = fabs(pi.z_line - pi.z_neutral);

    double z_avg = (pi.z_line + pi.z_neutral) / 2.0;
    pi.imbalance_ratio = (z_avg > 1e-30) ? pi.delta_z / z_avg : 0.0;

    pi.expected_lcl_db = lcl_estimate_from_imbalance(pi.z_line, pi.z_neutral);

    return pi;
}

/* ========================================================================
 * L2: ABCD matrix of basic elements
 *
 * Series impedance Z:
 *   [A B]   [1 Z]
 *   [C D] = [0 1]
 *
 * Shunt admittance Y:
 *   [A B]   [1 0]
 *   [C D] = [Y 1]
 *
 * These are building blocks for constructing filter ABCD matrices.
 * ======================================================================== */

abcd_matrix_t abcd_series_impedance(complex_t z) {
    abcd_matrix_t abcd;
    abcd.A.real = 1.0; abcd.A.imag = 0.0;
    abcd.B = z;
    abcd.C.real = 0.0; abcd.C.imag = 0.0;
    abcd.D.real = 1.0; abcd.D.imag = 0.0;
    return abcd;
}

abcd_matrix_t abcd_shunt_admittance(complex_t y) {
    abcd_matrix_t abcd;
    abcd.A.real = 1.0; abcd.A.imag = 0.0;
    abcd.B.real = 0.0; abcd.B.imag = 0.0;
    abcd.C = y;
    abcd.D.real = 1.0; abcd.D.imag = 0.0;
    return abcd;
}

/* ========================================================================
 * L5: Build complete filter ABCD matrix from topology and values
 *
 * This constructs the ABCD matrix for a given filter topology
 * at a specific frequency, which can then be used for cascade
 * analysis, insertion loss calculation, and S-parameter conversion.
 * ======================================================================== */

abcd_matrix_t build_filter_abcd(filter_topology_t topology,
                                 double l1, double c1,
                                 double l2, double c2,
                                 double freq) {
    double omega = M_2PI * freq;
    complex_t zero = {0.0, 0.0};
    complex_t id_abcd[4] = {{1.0, 0.0}, zero, zero, {1.0, 0.0}};
    abcd_matrix_t result = {id_abcd[0], id_abcd[1], id_abcd[2], id_abcd[3]};

    switch (topology) {
        case FILTER_TOPOLOGY_L_SERIES: {
            complex_t zL = {0.0, omega * l1};
            result = abcd_series_impedance(zL);
            break;
        }
        case FILTER_TOPOLOGY_C_SHUNT: {
            complex_t yC = {0.0, omega * c1};
            result = abcd_shunt_admittance(yC);
            break;
        }
        case FILTER_TOPOLOGY_LC: {
            complex_t zL = {0.0, omega * l1};
            complex_t yC = {0.0, omega * c2};
            abcd_matrix_t a1 = abcd_series_impedance(zL);
            abcd_matrix_t a2 = abcd_shunt_admittance(yC);
            result = abcd_cascade_two(&a1, &a2);
            break;
        }
        case FILTER_TOPOLOGY_CL: {
            complex_t yC = {0.0, omega * c1};
            complex_t zL = {0.0, omega * l1};
            abcd_matrix_t a1 = abcd_shunt_admittance(yC);
            abcd_matrix_t a2 = abcd_series_impedance(zL);
            result = abcd_cascade_two(&a1, &a2);
            break;
        }
        case FILTER_TOPOLOGY_PI: {
            complex_t zL = {0.0, omega * l1};
            complex_t yC1 = {0.0, omega * c1};
            complex_t yC2 = {0.0, omega * c2};
            abcd_matrix_t a1 = abcd_shunt_admittance(yC1);
            abcd_matrix_t a2 = abcd_series_impedance(zL);
            abcd_matrix_t a3 = abcd_shunt_admittance(yC2);
            abcd_matrix_t t = abcd_cascade_two(&a1, &a2);
            result = abcd_cascade_two(&t, &a3);
            break;
        }
        case FILTER_TOPOLOGY_T: {
            complex_t zL1 = {0.0, omega * l1};
            complex_t yC = {0.0, omega * c1};
            complex_t zL2 = {0.0, omega * (l2 > 0.0 ? l2 : l1)};
            abcd_matrix_t a1 = abcd_series_impedance(zL1);
            abcd_matrix_t a2 = abcd_shunt_admittance(yC);
            abcd_matrix_t a3 = abcd_series_impedance(zL2);
            abcd_matrix_t t = abcd_cascade_two(&a1, &a2);
            result = abcd_cascade_two(&t, &a3);
            break;
        }
        default:
            break;
    }

    return result;
}

/* ========================================================================
 * L6: Filter to Z-matrix conversion (2-port reduction)
 *
 * For a filter network with multiple elements, reduces to an
 * equivalent 2-port Z-matrix by building the ABCD matrix of
 * cascaded stages and converting to Z-parameters.
 * ======================================================================== */

int filter_to_z_matrix(const filter_element_t *elements,
                       size_t num_elem, double freq,
                       z_matrix_t *z_mat) {
    if (elements == NULL || num_elem == 0 || z_mat == NULL) return -1;

    double omega = M_2PI * freq;

    /* Build cascaded ABCD from element sequence */
    abcd_matrix_t total = {{1.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, {1.0, 0.0}};

    for (size_t i = 0; i < num_elem; i++) {
        complex_t imp = filter_element_impedance(&elements[i], freq);
        complex_t adm = {0.0, 0.0};

        if (elements[i].type == FILTER_ELEM_X_CAPACITOR ||
            elements[i].type == FILTER_ELEM_Y_CAPACITOR) {
            /* Shunt element */
            if (omega > 1e-30) {
                adm.imag = omega * elements[i].nominal_value;
            }
            abcd_matrix_t stage_abcd = abcd_shunt_admittance(adm);
            total = abcd_cascade_two(&total, &stage_abcd);
        } else {
            /* Series element */
            abcd_matrix_t stage_abcd = abcd_series_impedance(imp);
            total = abcd_cascade_two(&total, &stage_abcd);
        }
    }

    return abcd_to_z_matrix(&total, z_mat);
}
