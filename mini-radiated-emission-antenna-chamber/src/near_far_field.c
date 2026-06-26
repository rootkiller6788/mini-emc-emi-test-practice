/* =========================================================================
 * near_far_field.c — Near-Field / Far-Field Field Computation
 *
 * L1: Near-field and far-field region definitions
 * L3: Hertzian dipole full-field equations, Maxwell-based field computation
 * L4: Image theory, reciprocity
 * L5: Field region classification, distance extrapolation
 * L6: Antenna-to-antenna coupling in near-field
 * L7: Near-field scanning for EMC diagnostics (e.g., near-field probes)
 *
 * References:
 *   Balanis (2016) Antenna Theory, chapters 2, 4
 *   Jackson (1999) Classical Electrodynamics, chapter 9
 *   Paul (2006) EMC chapter 3 (Fields and Antennas)
 *   CISPR 16-1-4 Annex D (Near-field corrections)
 * ========================================================================= */

#include "field_propagation.h"
#include "antenna_types.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define SPEED_OF_LIGHT 299792458.0
#define PI              3.14159265358979323846
#define EPSILON_0       8.854187817e-12
#define MU_0            1.2566370614e-6

/* -------------------------------------------------------------------------
 * L1: Free-space impedance.
 * eta_0 = sqrt(mu_0 / epsilon_0) ≈ 376.73 ohm
 * Equivalently: eta_0 = mu_0 * c = 119.9169832 * pi
 * ------------------------------------------------------------------------- */
double fp_free_space_impedance(void)
{
    return sqrt(MU_0 / EPSILON_0);
}

/* -------------------------------------------------------------------------
 * L3: Wavenumber k.
 * k = 2*pi/lambda = 2*pi*f/c = omega/c
 * ------------------------------------------------------------------------- */
double fp_wavenumber(double frequency_hz)
{
    if (frequency_hz <= 0.0) return 0.0;
    return 2.0 * PI * frequency_hz / SPEED_OF_LIGHT;
}

/* -------------------------------------------------------------------------
 * L3+L4: Complete EM field of a Hertzian (electrically short) dipole.
 *
 * Full-field equations including all 1/r, 1/r^2, 1/r^3 terms:
 *
 * E_r = eta * (I0*L/(2*pi*r^2)) * cos(theta)
 *       * (1 + 1/(j*k*r)) * exp(-j*k*r)
 *
 * E_theta = j * eta * (k*I0*L/(4*pi*r)) * sin(theta)
 *           * (1 + 1/(j*k*r) - 1/(k^2*r^2)) * exp(-j*k*r)
 *
 * E_phi = 0 (symmetry)
 *
 * H_phi = j * (k*I0*L/(4*pi*r)) * sin(theta)
 *         * (1 + 1/(j*k*r)) * exp(-j*k*r)
 *
 * H_r = H_theta = 0
 *
 * Reference: Balanis (2016) equations 4-4, 4-5, 4-6
 *            Jackson (1999) equation 9.18
 *
 * @param dipole Hertzian dipole parameters
 * @param result Output: complete field components
 * ------------------------------------------------------------------------- */
void fp_hertzian_dipole_field(const fp_hertzian_dipole_t *dipole,
                               fp_field_result_t *result)
{
    if (!dipole || !result) return;
    memset(result, 0, sizeof(*result));

    double r     = dipole->position_r_m;
    double theta = dipole->theta_rad;
    double freq  = dipole->frequency_hz;
    double I0_dl = dipole->current_moment_am; /* I0 * dl product */

    if (r <= 0.0 || freq <= 0.0 || I0_dl == 0.0) {
        if (r <= 0.0) result->region = FP_STATIC_FIELD;
        return;
    }

    double k     = fp_wavenumber(freq);
    double eta   = fp_free_space_impedance();
    double omega = 2.0 * PI * freq;

    /* kr product */
    double kr = k * r;

    /* Compute exponential factor exp(-j*k*r) */
    double exp_re = cos(kr);
    double exp_im = -sin(kr);  /* exp(-j*kr) = cos(kr) - j*sin(kr) */

    /* Common amplitude factors */
    double A = (I0_dl) / (4.0 * PI);
    double sin_theta = sin(theta);
    double cos_theta = cos(theta);

    /* --- E_r component ---
     * E_r = eta * (I0*L/(2*pi*r^2)) * cos(theta) * (1 + 1/(j*kr)) * exp(-j*kr)
     *     = eta * (I0*L/(2*pi*r^2)) * cos(theta) * (1 - j/(kr)) * exp(-j*kr)
     */
    double er_amp = eta * I0_dl * cos_theta / (2.0 * PI * r * r);
    double er_1   = 1.0;
    double er_2   = -1.0 / kr;  /* 1/(j*kr) = -j/(kr) */

    /* E_r = er_amp * (er_1 + j*er_2) * (exp_re + j*exp_im) */
    {
        double real_part = er_1 * exp_re - er_2 * exp_im;
        double imag_part = er_1 * exp_im + er_2 * exp_re;
        result->fields.e_r.re = er_amp * real_part;
        result->fields.e_r.im = er_amp * imag_part;
    }

    /* --- E_theta component ---
     * E_theta = j*eta*(k*I0*L/(4*pi*r))*sin(theta)
     *           *(1 + 1/(j*kr) - 1/(kr)^2)*exp(-j*kr)
     * Multiply by j: adds 90-degree phase
     */
    double et_amp = eta * k * I0_dl * sin_theta / (4.0 * PI * r);
    /* Terms inside parentheses: (1 - j/(kr) - 1/(kr)^2) */
    double et_1 = 1.0 - 1.0 / (kr * kr);  /* real part */
    double et_2 = -1.0 / kr;               /* imag part from 1/(j*kr) */

    /* Multiply by j: j*(a+jb) = -b + j*a */
    {
        double real_tmp = et_1 * exp_re - et_2 * exp_im;
        double imag_tmp = et_1 * exp_im + et_2 * exp_re;
        result->fields.e_theta.re = et_amp * (-imag_tmp);
        result->fields.e_theta.im = et_amp * real_tmp;
    }

    /* --- E_phi = 0 --- */
    result->fields.e_phi.re = 0.0;
    result->fields.e_phi.im = 0.0;

    /* --- H_phi component ---
     * H_phi = j*(k*I0*L/(4*pi*r))*sin(theta)*(1+1/(j*kr))*exp(-j*kr)
     */
    double hp_amp = k * I0_dl * sin_theta / (4.0 * PI * r);
    double hp_1   = 1.0;          /* real part */
    double hp_2   = -1.0 / kr;    /* imag part (1/(j*kr) = -j/(kr)) */

    {
        double real_tmp = hp_1 * exp_re - hp_2 * exp_im;
        double imag_tmp = hp_1 * exp_im + hp_2 * exp_re;
        /* Multiply by j */
        result->fields.h_phi.re = hp_amp * (-imag_tmp);
        result->fields.h_phi.im = hp_amp * real_tmp;
    }

    /* --- H_r = H_theta = 0 --- */
    result->fields.h_r.re     = 0.0;
    result->fields.h_r.im     = 0.0;
    result->fields.h_theta.re = 0.0;
    result->fields.h_theta.im = 0.0;

    /* Compute field magnitudes */
    double e2 = result->fields.e_r.re * result->fields.e_r.re
              + result->fields.e_r.im * result->fields.e_r.im
              + result->fields.e_theta.re * result->fields.e_theta.re
              + result->fields.e_theta.im * result->fields.e_theta.im;

    double h2 = result->fields.h_phi.re * result->fields.h_phi.re
              + result->fields.h_phi.im * result->fields.h_phi.im;

    result->e_magnitude_vm      = sqrt(e2);
    result->h_magnitude_am      = sqrt(h2);

    if (result->h_magnitude_am > 1e-30) {
        result->wave_impedance_ohm = result->e_magnitude_vm
                                     / result->h_magnitude_am;
    } else {
        result->wave_impedance_ohm = fp_free_space_impedance();
    }

    /* Poynting vector magnitude |S| = |E x H| ≈ |E|*|H| */
    result->poynting_vector_wm2 = result->e_magnitude_vm
                                  * result->h_magnitude_am;

    /* Total radiated power: P_rad = eta*(pi/3)*|I0*dl/lambda|^2 */
    double lambda = SPEED_OF_LIGHT / freq;
    result->total_radiated_power_w = eta * (PI / 3.0)
                                     * (I0_dl / lambda) * (I0_dl / lambda);

    /* Classify field region */
    result->region = fp_classify_region(r, 0.1, freq); /* Assume D~0.1m */
}

/* -------------------------------------------------------------------------
 * Far-field only computation (simplified, faster).
 *
 * E_theta = j*eta*(k*I0*L/(4*pi*r))*sin(theta)*exp(-j*k*r)
 * H_phi   = E_theta / eta
 *
 * Far-field condition: r >> lambda, r >> D, r >> 2*D^2/lambda
 * ------------------------------------------------------------------------- */
void fp_hertzian_dipole_far_field(const fp_hertzian_dipole_t *dipole,
                                   fp_field_result_t *result)
{
    if (!dipole || !result) return;
    memset(result, 0, sizeof(*result));

    double r     = dipole->position_r_m;
    double theta = dipole->theta_rad;
    double freq  = dipole->frequency_hz;
    double I0_dl = dipole->current_moment_am;

    if (r <= 0.0 || freq <= 0.0 || I0_dl == 0.0) return;

    double k     = fp_wavenumber(freq);
    double eta   = fp_free_space_impedance();
    double kr    = k * r;

    /* Far-field E_theta amplitude */
    double amp = eta * k * I0_dl * sin(theta) / (4.0 * PI * r);

    /* Phase: j * exp(-j*kr) = exp(-j*kr + j*pi/2) */
    double exp_re = cos(kr);
    double exp_im = -sin(kr);

    /* j*(exp_re + j*exp_im) = -exp_im + j*exp_re */
    result->fields.e_theta.re = amp * (-exp_im);
    result->fields.e_theta.im = amp * exp_re;

    /* H_phi = E_theta / eta */
    result->fields.h_phi.re = result->fields.e_theta.re / eta;
    result->fields.h_phi.im = result->fields.e_theta.im / eta;

    result->e_magnitude_vm = amp;
    result->h_magnitude_am = amp / eta;
    result->wave_impedance_ohm = eta;
    result->poynting_vector_wm2 = amp * amp / eta;
    result->region = FP_FAR_FIELD;
}

/* -------------------------------------------------------------------------
 * Field of electrically small loop antenna (magnetic dipole).
 *
 * Dual of Hertzian dipole via duality principle:
 *   E_dipole -> H_loop * eta
 *   H_dipole -> -E_loop / eta
 *
 * H_r = (k^2*a^2*I/(2r))*cos(theta)*(1 + 1/(jkr))*exp(-jkr)
 * H_theta = -(k^2*a^2*I/(4r))*sin(theta)*(1 + 1/(jkr) - 1/(kr)^2)*exp(-jkr)
 * E_phi = eta*(k^2*a^2*I/(4r))*sin(theta)*(1 + 1/(jkr))*exp(-jkr)
 *
 * where a = loop radius, area S = pi*a^2
 *
 * Reference: Balanis (2016) section 5.2
 * ------------------------------------------------------------------------- */
void fp_loop_antenna_field(const fp_loop_antenna_t *loop,
                            double r, double theta,
                            fp_field_result_t *result)
{
    if (!loop || !result) return;
    memset(result, 0, sizeof(*result));

    if (r <= 0.0 || loop->frequency_hz <= 0.0) return;

    double freq = loop->frequency_hz;
    double k    = fp_wavenumber(freq);
    double eta  = fp_free_space_impedance();
    double S    = loop->loop_area_m2;
    double I    = loop->current_a;
    double N    = (double)loop->num_turns;
    double kr   = k * r;

    /* Magnetic moment: M = N * I * S */
    double M = N * I * S;

    /* H-field components (dual of E-field of Hertzian dipole) */
    double A_h = k * k * M / (4.0 * PI * r);
    double sin_t = sin(theta);
    double cos_t = cos(theta);
    double exp_re = cos(kr);
    double exp_im = -sin(kr);

    /* H_theta */
    double ht_amp = -A_h * sin_t;
    double ht_1 = 1.0 - 1.0 / (kr * kr);
    double ht_2 = -1.0 / kr;
    {
        double rt = ht_1 * exp_re - ht_2 * exp_im;
        double it = ht_1 * exp_im + ht_2 * exp_re;
        result->fields.h_theta.re = ht_amp * (-it);  /* multiply by j */
        result->fields.h_theta.im = ht_amp * rt;
    }

    /* E_phi (far-field dominant term for loop) */
    double ep_amp = eta * k * k * M * sin_t / (4.0 * PI * r);
    double ep_1 = 1.0;
    double ep_2 = -1.0 / kr;
    {
        double rt = ep_1 * exp_re - ep_2 * exp_im;
        double it = ep_1 * exp_im + ep_2 * exp_re;
        result->fields.e_phi.re = ep_amp * (-it);
        result->fields.e_phi.im = ep_amp * rt;
    }

    /* Compute magnitudes */
    double e2 = result->fields.e_phi.re * result->fields.e_phi.re
              + result->fields.e_phi.im * result->fields.e_phi.im;
    double h2 = result->fields.h_theta.re * result->fields.h_theta.re
              + result->fields.h_theta.im * result->fields.h_theta.im;

    result->e_magnitude_vm = sqrt(e2);
    result->h_magnitude_am = sqrt(h2);
    result->wave_impedance_ohm = result->h_magnitude_am > 1e-30 ?
        result->e_magnitude_vm / result->h_magnitude_am : eta;
    result->poynting_vector_wm2 = result->e_magnitude_vm * result->h_magnitude_am;
    result->region = fp_classify_region(r, 0.1, freq);
}

/* -------------------------------------------------------------------------
 * L1: Classify field region per IEEE Std 145-2013.
 *
 * Reactive near-field:  r < 0.62 * sqrt(D^3 / lambda)
 * Radiating near-field: 0.62*sqrt(D^3/lambda) < r < 2*D^2/lambda
 * Far-field (Fraunhofer): r > 2*D^2/lambda
 *
 * For D < lambda/2 (electrically small): all r > lambda/(2*pi) is far-field.
 * ------------------------------------------------------------------------- */
fp_field_region_t fp_classify_region(double r, double D, double frequency_hz)
{
    if (r <= 0.0 || frequency_hz <= 0.0) return FP_STATIC_FIELD;

    double lambda = SPEED_OF_LIGHT / frequency_hz;

    /* For electrically small antenna (D < lambda/2):
     * Far-field starts at r > lambda/(2*pi) */
    if (D < lambda / 2.0) {
        double r_ff_small = lambda / (2.0 * PI);
        if (r < r_ff_small) return FP_REACTIVE_NEAR_FIELD;
        return FP_FAR_FIELD;
    }

    /* General case */
    double r_nf = 0.62 * sqrt(D * D * D / lambda);
    double r_ff = 2.0 * D * D / lambda;

    if (r < r_nf) return FP_REACTIVE_NEAR_FIELD;
    if (r < r_ff) return FP_RADIATING_NEAR_FIELD;
    return FP_FAR_FIELD;
}

/* -------------------------------------------------------------------------
 * L3: Wave impedance magnitude vs distance.
 *
 * For Hertzian dipole:
 * |Zw(r)| = eta_0 * sqrt((1 + 1/(kr)^2 + 1/(kr)^4) / (1 + 1/(kr)^2))
 *
 * Properties:
 * - In reactive near-field (kr << 1): Zw >> eta_0 (high impedance for E-field source)
 * - In far-field (kr >> 1): Zw -> eta_0 = 377 ohm
 *
 * For magnetic dipole (loop): Zw << eta_0 in near-field (low impedance)
 *
 * Reference: Paul (2006) section 3.2.1, Fig 3-3
 * ------------------------------------------------------------------------- */
double fp_wave_impedance_magnitude(double r, double frequency_hz)
{
    if (r <= 0.0 || frequency_hz <= 0.0) return fp_free_space_impedance();

    double k  = fp_wavenumber(frequency_hz);
    double kr = k * r;

    if (kr > 100.0) {
        /* Deep far-field: Zw = eta_0 exactly */
        return fp_free_space_impedance();
    }

    if (kr < 0.01) {
        /* Deep reactive near-field: asymptotic limit */
        double eta_0 = fp_free_space_impedance();
        /* |Zw| ≈ eta_0 / (kr) for very small kr */
        return eta_0 / kr;
    }

    double eta_0 = fp_free_space_impedance();
    double kr2 = kr * kr;
    double kr4 = kr2 * kr2;

    double num = 1.0 + 1.0 / kr2 + 1.0 / kr4;
    double den = 1.0 + 1.0 / kr2;

    return eta_0 * sqrt(num / den);
}

/* -------------------------------------------------------------------------
 * L5: Extrapolate field strength between measurement distances.
 *
 * Far-field (r > lambda/2*pi): E ~ 1/r
 * Near-field depends on source type:
 *   - Electric dipole: E ~ 1/r^3 in reactive near-field
 *   - Magnetic dipole: H ~ 1/r^3, E ~ 1/r^2
 *
 * CISPR 16-2-3 allows 1/r extrapolation for r >= lambda/2*pi
 *
 * General extrapolation:
 *   E_2 = E_1 * (r_1 / r_2)^n
 *   where n = 1 (far-field), 2 (radiating near), 3 (reactive near)
 *
 * @param e_field_vm     Known field [V/m]
 * @param distance_known Known distance [m]
 * @param distance_new   Target distance [m]
 * @param frequency_hz   Frequency [Hz]
 * @param source_type    0=electric dipole, 1=magnetic dipole
 * @return Estimated field at new distance [V/m]
 * ------------------------------------------------------------------------- */
double fp_extrapolate_distance(double e_field_vm, double distance_known,
                                double distance_new, double frequency_hz,
                                int source_type)
{
    if (distance_known <= 0.0 || distance_new <= 0.0) return -9999.0;
    if (e_field_vm < 0.0) return -9999.0;

    double lambda = SPEED_OF_LIGHT / frequency_hz;
    double r_transition = lambda / (2.0 * PI);

    /* Determine exponent based on distance relative to transition */
    int exponent;

    if (distance_known >= r_transition && distance_new >= r_transition) {
        /* Both in far-field: 1/r */
        exponent = 1;
    } else if (distance_known < r_transition && distance_new < r_transition) {
        /* Both in near-field: depends on source type */
        if (source_type == 0) {
            exponent = 3; /* Electric dipole: E ~ 1/r^3 */
        } else {
            exponent = 2; /* Magnetic dipole: E ~ 1/r^2 */
        }
    } else {
        /* Mixed: use 1/r with warning (CISPR 16-2-3 practice) */
        exponent = 1;
    }

    return e_field_vm * pow(distance_known / distance_new, (double)exponent);
}

/* -------------------------------------------------------------------------
 * L3: Poynting vector from E-field.
 *
 * In far-field: |S| = |E|^2 / eta_0
 *
 * In near-field: S is complex; |S| is computed from full fields.
 * ------------------------------------------------------------------------- */
double fp_poynting_from_efield(double e_field_vm)
{
    if (e_field_vm < 0.0) return -1.0;
    double eta_0 = fp_free_space_impedance();
    return e_field_vm * e_field_vm / eta_0;
}

/* -------------------------------------------------------------------------
 * L4: Far-field from transmitter power (Friis-based).
 *
 * E = sqrt(30 * P_tx * G_tx) / r   [V/m]
 *
 * where 30 comes from: eta_0 / (4*pi) = 120*pi / (4*pi) = 30
 *
 * Reference: Balanis (2016) eqn 2-117
 * ------------------------------------------------------------------------- */
double fp_field_from_power(double power_tx_w, double gain_tx_linear,
                            double distance_m)
{
    if (distance_m <= 0.0 || power_tx_w < 0.0) return -1.0;
    if (gain_tx_linear <= 0.0) return -1.0;

    return sqrt(30.0 * power_tx_w * gain_tx_linear) / distance_m;
}

/* -------------------------------------------------------------------------
 * Compute far-field boundary for common EMC antennas.
 *
 * Convenience function that calls at_far_field_boundary.
 * ------------------------------------------------------------------------- */
void at_far_field_boundary(double D, double frequency_hz,
                            double *r_nf, double *r_ff)
{
    if (!r_nf || !r_ff) return;

    if (D <= 0.0 || frequency_hz <= 0.0) {
        *r_nf = 0.0;
        *r_ff = 0.0;
        return;
    }

    double lambda = SPEED_OF_LIGHT / frequency_hz;

    *r_nf = 0.62 * sqrt(D * D * D / lambda);
    *r_ff = 2.0 * D * D / lambda;
}