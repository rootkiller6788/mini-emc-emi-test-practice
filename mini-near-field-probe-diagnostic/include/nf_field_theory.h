#ifndef NF_FIELD_THEORY_H
#define NF_FIELD_THEORY_H
/**
 * @file nf_field_theory.h
 * @brief Near-Field Electromagnetic Theory (L3-L4)
 *
 * L3: Mathematical structures — Maxwell equations, vector potentials,
 *     Green's functions, plane wave spectrum
 * L4: Fundamental laws — Poynting theorem, boundary conditions,
 *     equivalence principle, image theory
 *
 * Ref: Constantine A. Balanis, "Advanced Engineering Electromagnetics", 2nd ed, 2012
 *      Roger F. Harrington, "Time-Harmonic Electromagnetic Fields", 2001
 *      John D. Jackson, "Classical Electrodynamics", 3rd ed, 1999
 */

#include <complex.h>
#include <math.h>
#include "nf_probe_core.h"
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <stddef.h>

/* ============================================================================
 * L3 — Mathematical Structures
 * ============================================================================
 */

/* ---------- Complex Vector in 3D ---------- */

typedef struct {
    double complex x;
    double complex y;
    double complex z;
} nf_complex_vector_t;

/* ---------- Real Vector in 3D ---------- */

typedef struct {
    double x;
    double y;
    double z;
} nf_real_vector_t;

/* ---------- 3x3 Tensor for Anisotropic Media ---------- */

typedef struct {
    double complex xx, xy, xz;
    double complex yx, yy, yz;
    double complex zx, zy, zz;
} nf_tensor33_t;

/* ---------- Plane Wave Parameters ---------- */

typedef struct {
    double complex amplitude;
    nf_real_vector_t propagation_dir;
    nf_real_vector_t polarization;
    double freq_hz;
    double wave_impedance;
} nf_plane_wave_t;

/* ---------- Plane Wave Spectrum (L3) ---------- */

typedef struct {
    double kx_min, kx_max, ky_min, ky_max;
    size_t nkx, nky;
    double dkx, dky;
    double complex *spectrum_Ex;
    double complex *spectrum_Ey;
    double complex *spectrum_Hx;
    double complex *spectrum_Hy;
    size_t total_points;
} nf_plane_wave_spectrum_t;

/* ---------- Spherical Wave Expansion (L3) ---------- */

typedef struct {
    double r_m;
    double theta_rad;
    double phi_rad;
    int    l_max;
    int    m_max;
    double complex *a_lm_TE;
    double complex *a_lm_TM;
    size_t num_modes;
} nf_spherical_expansion_t;

/* ---------- Green's Function for Free Space (L3) ---------- */

typedef struct {
    double complex k0;
    nf_real_vector_t src_pos;
    nf_real_vector_t obs_pos;
    double complex scalar_green;
    double complex dyadic_green_xx, dyadic_green_xy, dyadic_green_xz;
    double complex dyadic_green_yx, dyadic_green_yy, dyadic_green_yz;
    double complex dyadic_green_zx, dyadic_green_zy, dyadic_green_zz;
} nf_greens_dyadic_t;

/* ---------- Poynting Vector Components (L3) ---------- */

typedef struct {
    double complex Sx;
    double complex Sy;
    double complex Sz;
    double         active_power;
    double         reactive_power;
} nf_poynting_t;

/* ---------- Equivalent Sources (L3) ---------- */

typedef struct {
    nf_real_vector_t position;
    double complex electric_dipole_moment;
    double complex magnetic_dipole_moment;
    double complex electric_quadrupole_xx, electric_quadrupole_xy, electric_quadrupole_xz;
    double complex electric_quadrupole_yy, electric_quadrupole_yz, electric_quadrupole_zz;
} nf_equivalent_source_t;

typedef struct {
    nf_equivalent_source_t *sources;
    size_t num_sources;
    double freq_hz;
    nf_real_vector_t domain_min;
    nf_real_vector_t domain_max;
} nf_equivalent_source_array_t;

/* ============================================================================
 * L4 — Fundamental Laws (implemented as verifiable computations)
 * ============================================================================
 */

/* ---------- Maxwell Curl Equations (Frequency Domain) ---------- */

/**
 * ∇×E = -jωμH - M_s   (Faraday's Law)
 * ∇×H =  jωεE + J_s   (Ampere's Law)
 *
 * M_s: magnetic current density (fictitious, for equivalence)
 * J_s: electric current density
 */
typedef struct {
    double complex Jx, Jy, Jz;
    double complex Mx, My, Mz;
    double complex eps;
    double complex mu;
    double omega;
    nf_complex_vector_t curl_E;
    nf_complex_vector_t curl_H;
} nf_maxwell_curl_t;

/* ---------- Boundary Conditions (L4) ---------- */

typedef enum {
    NF_BC_PEC = 0,
    NF_BC_PMC = 1,
    NF_BC_IMPEDANCE = 2,
    NF_BC_RADIATION = 3,
    NF_BC_PERIODIC = 4
} nf_boundary_condition_type_t;

typedef struct {
    nf_boundary_condition_type_t type;
    nf_real_vector_t normal;
    double complex surface_impedance;
    nf_complex_vector_t tangential_E;
    nf_complex_vector_t tangential_H;
} nf_boundary_condition_t;

/* ---------- Equivalence Principle (L4) ---------- */

typedef struct {
    nf_real_vector_t surface_point;
    nf_real_vector_t normal_outward;
    nf_complex_vector_t E_total;
    nf_complex_vector_t H_total;
    nf_complex_vector_t J_equivalent;
    nf_complex_vector_t M_equivalent;
    int interior_is_null;
} nf_equivalence_principle_t;

/* ---------- Image Theory for PEC Ground Plane (L4) ---------- */

typedef struct {
    nf_real_vector_t source_position;
    nf_real_vector_t ground_plane_normal;
    double ground_plane_z;
    nf_real_vector_t image_position;
    int is_horizontal_dipole;
    double complex image_multiplier;
} nf_image_theory_t;

/* ---------- Uniqueness Theorem Verification (L4) ---------- */

typedef struct {
    nf_complex_vector_t E_field_boundary;
    nf_complex_vector_t H_field_boundary;
    double complex omega;
    double complex eps;
    double complex mu;
    int uniqueness_holds;
    double energy_difference;
} nf_uniqueness_check_t;

/* ---------- Poynting Theorem (L4) ---------- */

typedef struct {
    double complex S_in_avg;
    double complex S_out_avg;
    double dissipated_power;
    double stored_electric_energy;
    double stored_magnetic_energy;
    double power_balance_error;
    int    theorem_holds;
} nf_poynting_theorem_check_t;

/* ============================================================================
 * API — L3 Math Operations
 * ============================================================================
 */

/* Vector algebra */
nf_complex_vector_t nf_cvec_add(nf_complex_vector_t a, nf_complex_vector_t b);
nf_complex_vector_t nf_cvec_sub(nf_complex_vector_t a, nf_complex_vector_t b);
nf_complex_vector_t nf_cvec_scale(nf_complex_vector_t a, double complex s);
double complex      nf_cvec_dot(nf_complex_vector_t a, nf_complex_vector_t b);
nf_complex_vector_t nf_cvec_cross(nf_complex_vector_t a, nf_complex_vector_t b);
double              nf_cvec_magnitude(nf_complex_vector_t a);

nf_real_vector_t nf_rvec_add(nf_real_vector_t a, nf_real_vector_t b);
nf_real_vector_t nf_rvec_sub(nf_real_vector_t a, nf_real_vector_t b);
nf_real_vector_t nf_rvec_scale(nf_real_vector_t a, double s);
double           nf_rvec_dot(nf_real_vector_t a, nf_real_vector_t b);
nf_real_vector_t nf_rvec_cross(nf_real_vector_t a, nf_real_vector_t b);
double           nf_rvec_magnitude(nf_real_vector_t a);
nf_real_vector_t nf_rvec_unit(nf_real_vector_t a);

/* Plane wave spectrum operations */
int  nf_pws_allocate(nf_plane_wave_spectrum_t *pws, size_t nkx, size_t nky,
                     double kx_min, double kx_max,
                     double ky_min, double ky_max);
void nf_pws_free(nf_plane_wave_spectrum_t *pws);
int  nf_pws_propagate_to_plane(const nf_plane_wave_spectrum_t *src,
                                nf_plane_wave_spectrum_t *dst,
                                double z_distance, double k0);
int  nf_pws_evaluate_field(const nf_plane_wave_spectrum_t *pws,
                            double x, double y, double z,
                            nf_complex_vector_t *E, nf_complex_vector_t *H);

/* Green's function computation */
int  nf_greens_dyadic_compute(nf_greens_dyadic_t *gf,
                               const nf_real_vector_t *src,
                               const nf_real_vector_t *obs,
                               double freq_hz);
void nf_greens_dyadic_field(const nf_greens_dyadic_t *gf,
                             const nf_complex_vector_t *J_src,
                             nf_complex_vector_t *E_out,
                             nf_complex_vector_t *H_out);

/* Equivalent source methods */
int  nf_equivalent_source_radiated_field(const nf_equivalent_source_t *src,
                                          const nf_real_vector_t *obs,
                                          double freq_hz,
                                          nf_complex_vector_t *E,
                                          nf_complex_vector_t *H);

/* Spherical wave expansion */
int  nf_spherical_expansion_init(nf_spherical_expansion_t *exp,
                                  int l_max, int m_max);
void nf_spherical_expansion_free(nf_spherical_expansion_t *exp);
int  nf_spherical_expansion_evaluate(const nf_spherical_expansion_t *exp,
                                      double r, double theta, double phi,
                                      nf_complex_vector_t *E,
                                      nf_complex_vector_t *H);

/* ============================================================================
 * API — L4 Fundamental Law Verifications
 * ============================================================================
 */

/* Maxwell curl equation verification */
int  nf_maxwell_curl_check(const nf_complex_vector_t *E,
                            const nf_complex_vector_t *H,
                            double complex eps, double complex mu,
                            double omega, double tolerance);

/* Boundary condition satisfaction */
int  nf_boundary_condition_check(const nf_boundary_condition_t *bc,
                                  const nf_complex_vector_t *E,
                                  const nf_complex_vector_t *H,
                                  double tolerance);

/* Poynting theorem verification over a closed surface */
int  nf_poynting_theorem_verify(const nf_complex_vector_t *E_surface,
                                 const nf_complex_vector_t *H_surface,
                                 const nf_real_vector_t *normals,
                                 const double *areas,
                                 size_t num_facets,
                                 double volume,
                                 double complex eps, double complex mu,
                                 double omega,
                                 nf_poynting_theorem_check_t *result);

/* Image theory computation */
int  nf_image_theory_compute(nf_image_theory_t *img,
                              const nf_real_vector_t *source_pos,
                              const nf_real_vector_t *ground_normal,
                              double ground_z,
                              int is_horizontal);

/* Uniqueness verification */
int  nf_uniqueness_verify(const nf_complex_vector_t *E_boundary,
                           const nf_complex_vector_t *H_boundary,
                           const nf_real_vector_t *boundary_points,
                           size_t num_points,
                           double complex eps, double complex mu,
                           double omega,
                           nf_uniqueness_check_t *result);

/* Equivalence principle application */
int  nf_equivalence_principle_apply(nf_equivalence_principle_t *ep,
                                     const nf_complex_vector_t *E,
                                     const nf_complex_vector_t *H,
                                     const nf_real_vector_t *normal,
                                     int null_interior);

/* Helmholtz equation solver (finite difference, 1D test case) */
int  nf_helmholtz_1d_solve(double complex *u, size_t N, double dx,
                             double complex k, double complex bc_left,
                             double complex bc_right);

/* Wave impedance in stratified media (transmission line analogy) */
double complex nf_stratified_input_impedance(const double complex *Z_layers,
                                              const double *d_layers,
                                              const double complex *k_layers,
                                              size_t num_layers,
                                              double complex Z_load);

#endif /* NF_FIELD_THEORY_H */
