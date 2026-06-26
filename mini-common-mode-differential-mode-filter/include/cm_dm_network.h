/**
 * @file cm_dm_network.h
 * @brief CM/DM Network Analysis — S-parameters, Mixed-Mode, Cascading
 *
 * Advanced network analysis for EMI filters including:
 *  - Two-port network parameters (Z, Y, ABCD, S)
 *  - Mixed-mode S-parameters (S_cc, S_cd, S_dc, S_dd)
 *  - Mode conversion analysis (CM→DM, DM→CM)
 *  - Cascaded network analysis via ABCD matrices
 *  - Modal decomposition and decoupling
 *
 * Reference:
 *  - Bockelman, D.E. "Combined Differential and Common-Mode Scattering
 *    Parameters" (1995), IEEE Trans. MTT
 *  - Paul, C.R. "Introduction to EMC" — Chapter 7 (Transmission lines
 *    and S-parameters)
 *  - Pozar, D.M. "Microwave Engineering" (2012) — Chapter 4 (Network analysis)
 */

#ifndef CM_DM_NETWORK_H
#define CM_DM_NETWORK_H

#include "cm_dm_filter.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * L1-L2: Network parameter matrices
 * ======================================================================== */

/** @brief 2×2 complex impedance matrix Z */
typedef struct {
    complex_t z11, z12;         /**< Z_ij = V_i/I_j | I_k=0 (k≠j) */
    complex_t z21, z22;
} z_matrix_t;

/** @brief 2×2 complex admittance matrix Y */
typedef struct {
    complex_t y11, y12;         /**< Y_ij = I_i/V_j | V_k=0 (k≠j) */
    complex_t y21, y22;
} y_matrix_t;

/** @brief 2×2 ABCD (transmission) matrix */
typedef struct {
    complex_t A, B;             /**< V₁ = A·V₂ - B·I₂ */
    complex_t C, D;             /**< I₁ = C·V₂ - D·I₂ */
} abcd_matrix_t;

/** @brief 2×2 S-parameter matrix (referenced to Z₀) */
typedef struct {
    complex_t s11, s12;         /**< Port 1 reflection, reverse gain */
    complex_t s21, s22;         /**< Forward gain, port 2 reflection */
    double z0;                  /**< Reference impedance [Ω] */
} s_matrix_t;

/* ========================================================================
 * L3: Mixed-mode S-parameters
 *
 * For a 4-port single-ended network (ports 1-2 = balanced input,
 * ports 3-4 = balanced output), we define mixed-mode S-parameters:
 *
 *   [b_d1]   [S_dd11  S_dd12 | S_dc11  S_dc12] [a_d1]
 *   [b_d2] = [S_dd21  S_dd22 | S_dc21  S_dc22] [a_d2]
 *   [b_c1]   [S_cd11  S_cd12 | S_cc11  S_cc12] [a_c1]
 *   [b_c2]   [S_cd21  S_cd22 | S_cc21  S_cc22] [a_c2]
 *
 * Where:
 *   S_dd = differential-mode S-parameters
 *   S_cc = common-mode S-parameters
 *   S_cd = CM-to-DM conversion (mode conversion)
 *   S_dc = DM-to-CM conversion (mode conversion)
 *
 * Transformation from standard 4-port S-parameters:
 *   S_mm = M · S_std · M⁻¹
 *   where M is the modal transformation matrix.
 * ======================================================================== */

typedef struct {
    /* Differential-mode block */
    complex_t s_dd11, s_dd12;
    complex_t s_dd21, s_dd22;
    /* Common-mode block */
    complex_t s_cc11, s_cc12;
    complex_t s_cc21, s_cc22;
    /* Mode conversion blocks */
    complex_t s_cd11, s_cd12;   /**< CM input → DM output */
    complex_t s_cd21, s_cd22;
    complex_t s_dc11, s_dc12;   /**< DM input → CM output */
    complex_t s_dc21, s_dc22;
    double z0_diff;             /**< Differential reference impedance [Ω] (typ 100) */
    double z0_comm;             /**< Common-mode reference impedance [Ω] (typ 25) */
} mixed_mode_s_t;

/* ========================================================================
 * L1: Mode conversion metrics
 * ======================================================================== */

/**
 * @brief Mode conversion is the key non-ideality in EMI filters:
 *
 * Longitudinal Conversion Loss (LCL):
 *   LCL = 20·log10( |V_dm_out| / |V_cm_in| )  [dB]
 *   Measures how much CM is converted to DM (undesirable).
 *
 * Transverse Conversion Loss (TCL):
 *   TCL = 20·log10( |V_cm_out| / |V_dm_in| )  [dB]
 *   Measures how much DM is converted to CM (undesirable).
 *
 * For an ideal balanced system: LCL → ∞, TCL → ∞.
 * In practice, imbalances (component tolerance, layout asymmetry)
 * cause finite LCL/TCL, typically 40–80 dB.
 */
typedef struct {
    double lcl_db;              /**< Longitudinal Conversion Loss [dB] */
    double tcl_db;              /**< Transverse Conversion Loss [dB] */
    double cmrr_db;             /**< CMRR = LCL [dB] */
    double dm_cm_isolation_db;  /**< CM-DM isolation [dB] */
    double frequency;           /**< Evaluation frequency [Hz] */
    double imbalance_pct;       /**< Estimated imbalance percentage */
} mode_conversion_t;

/* ========================================================================
 * L2: 4-port network for balanced pair
 * ======================================================================== */

/**
 * @brief Standard 4-port S-parameter matrix.
 *
 * Port numbering:
 *   Port 1: Line in    (balanced input +)
 *   Port 2: Neutral in (balanced input -)
 *   Port 3: Line out   (balanced output +)
 *   Port 4: Neutral out (balanced output -)
 */
typedef struct {
    complex_t s[4][4];          /**< S_ij scattering parameters */
    double z0;                  /**< Single-ended reference impedance [Ω] */
} s_4port_t;

/* ========================================================================
 * L6: Cascaded network analysis
 * ======================================================================== */

/**
 * @brief Cascade of N two-port networks.
 *
 * ABCD matrices multiply in reverse order:
 *   [ABCD]_total = [ABCD]_1 × [ABCD]_2 × ... × [ABCD]_N
 *
 * This is the fundamental method for analyzing multi-stage EMI filters.
 */
typedef struct {
    abcd_matrix_t *stages;      /**< ABCD matrix for each stage */
    size_t num_stages;          /**< Number of cascaded stages */
    abcd_matrix_t total;        /**< Total cascaded ABCD matrix */
    s_matrix_t total_s;         /**< Total S-parameters (Z₀ reference) */
} cascade_network_t;

/* ========================================================================
 * L3: Imbalance and symmetry analysis
 * ======================================================================== */

/**
 * @brief Impedance imbalance between L and N paths.
 *
 * This is the root cause of mode conversion in practical filters.
 *
 * If Z_L_path = Z_N_path, then there is no mode conversion.
 * If Z_L_path ≠ Z_N_path, CM current splits unequally → DM voltage
 * appears across the line pair.
 */
typedef struct {
    double z_line;              /**< L-path impedance [Ω] */
    double z_neutral;           /**< N-path impedance [Ω] */
    double delta_z;             /**< |Z_L - Z_N| [Ω] */
    double imbalance_ratio;     /**< ΔZ / Z_avg */
    double expected_lcl_db;     /**< Estimated LCL from imbalance */
} path_imbalance_t;

/* ========================================================================
 * API FUNCTIONS
 * ======================================================================== */

/**
 * @brief Create Z-matrix from 2-port impedance measurements.
 */
z_matrix_t z_matrix_create(complex_t z11, complex_t z12,
                            complex_t z21, complex_t z22);

/**
 * @brief Convert Z-matrix to Y-matrix: Y = Z⁻¹
 *
 * For 2×2: det(Z) = Z₁₁Z₂₂ - Z₁₂Z₂₁
 *   Y = 1/det · [ Z₂₂  -Z₁₂; -Z₂₁  Z₁₁ ]
 *
 * @return 0 on success, -1 if det(Z)=0 (singular)
 */
int z_to_y_matrix(const z_matrix_t *z, y_matrix_t *y);

/**
 * @brief Convert Z-matrix to ABCD matrix.
 *
 * ABCD parameters from Z:
 *   A = Z₁₁/Z₂₁
 *   B = (Z₁₁Z₂₂ - Z₁₂Z₂₁)/Z₂₁ = det(Z)/Z₂₁
 *   C = 1/Z₂₁
 *   D = Z₂₂/Z₂₁
 *
 * @return 0 on success, -1 if Z₂₁=0 (no transmission)
 */
int z_to_abcd_matrix(const z_matrix_t *z, abcd_matrix_t *abcd);

/**
 * @brief Convert ABCD matrix to Z-matrix.
 */
int abcd_to_z_matrix(const abcd_matrix_t *abcd, z_matrix_t *z);

/**
 * @brief Convert S-matrix to Z-matrix.
 *
 * Z = Z₀ · (I + S) · (I - S)⁻¹
 *
 * Where I is the 2×2 identity matrix.
 *
 * @param s     S-parameter matrix
 * @param z_out Output Z-matrix
 * @return 0 on success, -1 if (I-S) is singular
 */
int s_to_z_matrix(const s_matrix_t *s, z_matrix_t *z);

/**
 * @brief Convert Z-matrix to S-matrix.
 *
 * S = (Z/Z₀ - I) · (Z/Z₀ + I)⁻¹
 *
 * @param z     Z-matrix
 * @param z0    Reference impedance [Ω]
 * @param s_out Output S-matrix
 * @return 0 on success
 */
int z_to_s_matrix(const z_matrix_t *z, double z0, s_matrix_t *s_out);

/**
 * @brief Cascade two ABCD networks: ABD_total = ABCD₁ × ABCD₂
 *
 * Matrix multiplication: [A₁ B₁; C₁ D₁] × [A₂ B₂; C₂ D₂]
 */
abcd_matrix_t abcd_cascade_two(const abcd_matrix_t *a, const abcd_matrix_t *b);

/**
 * @brief Compute total ABCD matrix for a cascade of N networks.
 *
 * @param abcd_array Array of N ABCD matrices
 * @param n          Number of matrices
 * @param total      Output total ABCD matrix
 */
void abcd_cascade_n(const abcd_matrix_t *abcd_array, size_t n,
                     abcd_matrix_t *total);

/**
 * @brief Compute S-parameters from ABCD matrix.
 *
 * Given ABCD and reference impedance Z₀:
 *   Δ = A + B/Z₀ + C·Z₀ + D
 *   S₁₁ = (A + B/Z₀ - C·Z₀ - D) / Δ
 *   S₁₂ = 2·(A·D - B·C) / Δ
 *   S₂₁ = 2 / Δ
 *   S₂₂ = (-A + B/Z₀ - C·Z₀ + D) / Δ
 */
s_matrix_t abcd_to_s_matrix(const abcd_matrix_t *abcd, double z0);

/**
 * @brief Compute insertion loss from S-parameters.
 *
 * IL = -20·log10(|S₂₁|)  [dB]
 */
double s_to_insertion_loss(const s_matrix_t *s);

/**
 * @brief Compute return loss from S-parameters.
 *
 * RL = -20·log10(|S₁₁|)  [dB]  (input)
 * RL = -20·log10(|S₂₂|)  [dB]  (output)
 */
double s_to_return_loss(const s_matrix_t *s, int port);

/**
 * @brief Convert 4-port single-ended S-parameters to mixed-mode.
 *
 * L3: Applies the modal transformation:
 *   a_d = (a₁ - a₂)/√2
 *   a_c = (a₁ + a₂)/√2
 *
 * The transformation matrix M is:
 *   1/√2 · [ 1 -1  0  0
 *             0  0  1 -1
 *             1  1  0  0
 *             0  0  1  1 ]
 */
mixed_mode_s_t *s_4port_to_mixed_mode(const s_4port_t *s4,
                                       double z0_diff, double z0_comm);

/**
 * @brief Convert mixed-mode S-parameters back to 4-port single-ended.
 */
s_4port_t *mixed_mode_to_s_4port(const mixed_mode_s_t *mm);

/**
 * @brief Free mixed-mode S-parameter data.
 */
void mixed_mode_s_free(mixed_mode_s_t *mm);

/**
 * @brief Free 4-port S-parameter data.
 */
void s_4port_free(s_4port_t *s4);

/**
 * @brief Compute mode conversion metrics from mixed-mode S-parameters.
 *
 * LCL = -20·log10(|S_cd21|)  [dB]  (CM input → DM output)
 * TCL = -20·log10(|S_dc21|)  [dB]  (DM input → CM output)
 */
mode_conversion_t mode_conversion_from_mm(const mixed_mode_s_t *mm,
                                           double freq);

/**
 * @brief Estimate LCL from path impedance imbalance.
 *
 * LCL ≈ 20·log10( (Z_L + Z_N) / |Z_L - Z_N| )
 *
 * A 1% impedance imbalance → LCL ≈ 46 dB.
 * A 5% impedance imbalance → LCL ≈ 32 dB.
 * A 10% impedance imbalance → LCL ≈ 26 dB.
 */
double lcl_estimate_from_imbalance(double z_line, double z_neutral);

/**
 * @brief Calculate path imbalance from component tolerances.
 *
 * Given two nominal impedances Z₁, Z₂ with tolerances ±tol%,
 * estimate the worst-case impedance imbalance.
 */
path_imbalance_t path_imbalance_from_tolerance(double z1_nominal,
                                                double z2_nominal,
                                                double tolerance_pct);

/**
 * @brief Compute ABCD parameters of a series impedance.
 *
 * Series element Z between port 1 and port 2:
 *   A=1, B=Z, C=0, D=1
 */
abcd_matrix_t abcd_series_impedance(complex_t z);

/**
 * @brief Compute ABCD parameters of a shunt admittance.
 *
 * Shunt element Y from port 1 to ground:
 *   A=1, B=0, C=Y, D=1
 */
abcd_matrix_t abcd_shunt_admittance(complex_t y);

/**
 * @brief Build ABCD matrix for a complete LC filter section.
 *
 * π-filter (C1-shunt → L-series → C2-shunt):
 *   ABCD_π = ABCD_shunt(C₁) × ABCD_series(L) × ABCD_shunt(C₂)
 *
 * T-filter (L1-series → C-shunt → L2-series):
 *   ABCD_T = ABCD_series(L₁) × ABCD_shunt(C) × ABCD_series(L₂)
 */
abcd_matrix_t build_filter_abcd(filter_topology_t topology,
                                 double l1, double c1,
                                 double l2, double c2,
                                 double freq);

/**
 * @brief Compute the mesh impedance matrix for an N-node filter network.
 *
 * L3: For a network with N nodes, set up the N×N complex nodal
 * admittance matrix Y, where Y_ij represents the admittance between
 * nodes i and j. The matrix is solved for node voltages given
 * excitation current sources.
 *
 * This function returns the reduced 2×2 matrix after eliminating
 * internal nodes (Kron reduction), yielding the effective 2-port
 * parameters of the filter.
 *
 * @param elements Array of filter elements
 * @param num_elem Number of elements
 * @param freq     Frequency [Hz]
 * @param z_mat    Output 2×2 Z-matrix
 * @return 0 on success, -1 on singular network
 */
int filter_to_z_matrix(const filter_element_t *elements,
                       size_t num_elem, double freq,
                       z_matrix_t *z_mat);

#ifdef __cplusplus
}
#endif

#endif /* CM_DM_NETWORK_H */
