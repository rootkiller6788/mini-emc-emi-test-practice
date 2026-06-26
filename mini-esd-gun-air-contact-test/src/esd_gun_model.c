/**
 * @file esd_gun_model.c
 * @brief ESD gun equivalent circuit simulation and analysis
 *
 * Implements RLC circuit analysis for the IEC 61000-4-2 ESD
 * simulator, including RK4 time-domain simulation, analytical
 * solutions, and nonlinear arc resistance models.
 *
 * Knowledge coverage: L1-L6
 *   L3: RLC differential equations, state-space dynamics
 *   L4: Energy conservation, damping regimes
 *   L5: RK4 algorithm, analytical solution
 *   L6: Full ESD gun discharge simulation with arc models
 */

#include "esd_gun_model.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_E
#define M_E 2.71828182845904523536
#endif

/* ─── Default Parameter Initialization ─────────────────────────── */

/**
 * @brief Standard IEC 61000-4-2 gun RC parameters.
 *
 * L1 Definition: The IEC standard mandates:
 *   C_s = 150 pF ± 10%
 *   R_d = 330 Ω ± 10%
 *
 * These are the nominal values that define the energy storage
 * and discharge characteristics of the ESD simulator.
 */
esd_gun_rc_params_t esd_gun_rc_default(void)
{
    esd_gun_rc_params_t p;
    p.cs_pf  = 150.0;
    p.rd_ohm = 330.0;
    return p;
}

/**
 * @brief Standard IEC 61000-4-2 gun RLC parameters.
 *
 * L2 Concept: Real ESD guns have parasitic inductance that
 * critically affects the discharge waveform. A compliant gun
 * achieves the 0.7-1.0 ns rise time through careful control
 * of parasitic inductance.
 *
 * Typical parasitic inductance for a well-designed gun:
 *   L_par ≈ 0.05-0.10 µH for contact discharge
 *   L_par ≈ 0.1-0.5 µH for air discharge (longer path)
 *
 * Including a standard 2 Ω calibration target as load.
 */
esd_gun_rlc_params_t esd_gun_rlc_default(double v_charge_kv)
{
    esd_gun_rlc_params_t p;
    p.cs_pf       = 150.0;
    p.rd_ohm      = 330.0;
    p.l_par_nh    = 80.0;     /* 80 nH typical parasitic inductance */
    p.r_load_ohm  = 2.0;      /* Standard 2 Ω calibration target */
    p.v_charge_kv = v_charge_kv;
    return p;
}

/**
 * @brief Full parasitic model defaults.
 *
 * Includes mutual coupling to ground plane and stray capacitances
 * for a more complete model of the physical ESD gun.
 */
esd_gun_full_params_t esd_gun_full_default(void)
{
    esd_gun_full_params_t p;
    p.cs_pf        = 150.0;
    p.rd_ohm       = 330.0;
    p.l_self_nh    = 80.0;
    p.l_mutual_nh  = 10.0;
    p.c_stray_pf   = 5.0;
    p.r_ground_ohm = 0.1;
    p.l_ground_nh  = 20.0;
    return p;
}

/* ─── RLC Circuit Analysis ───────────────────────────────────── */

/**
 * @brief Compute natural frequency and damping of series RLC.
 *
 * L3 Mathematical Structure:
 *
 * The series RLC circuit is governed by:
 *
 *   L · d²i/dt² + R_total · di/dt + i/C = 0
 *
 * Characteristic equation:
 *   L·s² + R·s + 1/C = 0
 *   s = -R/(2L) ± √(R²/(4L²) - 1/(LC))
 *
 * Natural frequency (lossless):  ω₀ = 1/√(LC)
 * Damping ratio:                 ζ  = R/(2) · √(C/L)
 * Damped frequency (if ζ < 1):   ω_d = ω₀·√(1-ζ²)
 *
 * These three parameters completely characterize the discharge
 * dynamics.
 */
void esd_gun_rlc_natural(const esd_gun_rlc_params_t *params,
                          double *omega0, double *zeta, double *omega_d)
{
    if (!params) return;

    double c_farad = params->cs_pf * 1e-12;     /* pF → F */
    double l_henry = params->l_par_nh * 1e-9;    /* nH → H */
    double r_total = params->rd_ohm + params->r_load_ohm;

    /* Guard against divide by zero */
    if (l_henry <= 0.0 || c_farad <= 0.0) {
        if (omega0)  *omega0  = 0.0;
        if (zeta)    *zeta    = 0.0;
        if (omega_d) *omega_d = 0.0;
        return;
    }

    /* ω₀ = 1/√(LC) [rad/s] */
    double w0 = 1.0 / sqrt(l_henry * c_farad);
    if (omega0) *omega0 = w0;

    /* ζ = R/(2) · √(C/L) */
    double z = 0.5 * r_total * sqrt(c_farad / l_henry);
    if (zeta) *zeta = z;

    /* ω_d = ω₀·√(1-ζ²) if ζ < 1, else 0 */
    if (omega_d) {
        if (z < 1.0) {
            *omega_d = w0 * sqrt(1.0 - z * z);
        } else {
            *omega_d = 0.0;
        }
    }
}

/**
 * @brief Determine RLC damping regime.
 *
 * L2 Core Concept: The damping regime determines the waveform shape:
 *
 *   - Underdamped (ζ < 1):  Oscillatory with ringing.
 *     The ESD current overshoots and may reverse polarity.
 *     Can cause multiple-stress on protection devices.
 *
 *   - Critically damped (ζ = 1): Fastest monotonic decay.
 *     The ideal target for ESD gun design — fastest rise without
 *     ringing.
 *
 *   - Overdamped (ζ > 1): Slow monotonic decay.
 *     Rise time may be too slow to meet IEC spec.
 *
 * For the standard gun: R = 332 Ω, L = 80 nH, C = 150 pF
 *   ζ = 332/2 · √(150e-12 / 80e-9) = 166 · √(0.001875)
 *     = 166 · 0.0433 = 7.19  →  heavily overdamped!
 *
 * This means the standard IEC gun discharge is overdamped,
 * producing a smooth, non-oscillatory pulse. The fast rise
 * time comes from the initial dv/dt across the inductance,
 * not from RLC resonance.
 */
int esd_gun_rlc_damping_regime(const esd_gun_rlc_params_t *params)
{
    if (!params) return -1;

    double omega0, zeta, omega_d;
    esd_gun_rlc_natural(params, &omega0, &zeta, &omega_d);

    /* Critical damping threshold with tolerance */
    if (zeta > 1.001) {
        return 2;  /* Overdamped */
    } else if (zeta < 0.999) {
        return 0;  /* Underdamped */
    } else {
        return 1;  /* Critically damped */
    }
}

/**
 * @brief Theoretical peak current for RLC discharge.
 *
 * L4 Theorem: For an initially charged series RLC circuit
 * discharging into a resistive load, the peak current depends
 * on the damping regime.
 *
 * Underdamped (ζ < 1):
 *   I_peak = V₀/L · exp(-ζ·θ/√(1-ζ²)) / ω_d
 *   where θ = arctan(√(1-ζ²)/ζ)
 *
 * Critically damped (ζ = 1):
 *   I_peak = V₀/(e·R)  where e = 2.718...
 *
 * Overdamped (ζ > 1):
 *   I_peak ≈ V₀/R · (1 - exp(-R·t_peak/L))
 *   This is dominated by the initial di/dt = V₀/L
 *
 * For the heavily overdamped IEC gun (ζ ≈ 7), the peak is
 * approximately V₀/R_total, which underestimates the actual
 * peak because parasitic effects cause a faster initial rise.
 * The IEC-specified peak of 3.75 A/kV accounts for these effects.
 */
double esd_gun_rlc_peak_current(const esd_gun_rlc_params_t *params)
{
    if (!params) return 0.0;

    double c_farad = params->cs_pf * 1e-12;
    double l_henry = params->l_par_nh * 1e-9;
    double r_total = params->rd_ohm + params->r_load_ohm;
    double v0      = params->v_charge_kv * 1000.0;  /* kV → V */

    if (l_henry <= 0.0 || c_farad <= 0.0 || r_total <= 0.0) {
        return 0.0;
    }

    double w0 = 1.0 / sqrt(l_henry * c_farad);
    double zeta = 0.5 * r_total * sqrt(c_farad / l_henry);

    if (zeta < 0.999) {
        /* Underdamped: oscillatory peak */
        double wd = w0 * sqrt(1.0 - zeta * zeta);
        double theta = atan(sqrt(1.0 - zeta * zeta) / zeta);
        return v0 / l_henry * exp(-zeta * theta / sqrt(1.0 - zeta * zeta)) / wd;
    } else if (zeta <= 1.001) {
        /* Critically damped */
        return v0 / (M_E * r_total);
    } else {
        /* Overdamped: peak from initial slope approximation */
        /* For heavily overdamped, I_peak ≈ V₀ / R_total */
        return v0 / r_total;
    }
}

/**
 * @brief Estimate rise time for RLC discharge.
 *
 * L5 Algorithm: Approximate rise time estimation.
 *
 * For overdamped circuits, the rise time is dominated by
 * the L/R time constant:
 *   t_rise(10%-90%) ≈ 2.2 × L / R_total
 *
 * For the standard gun: t_rise ≈ 2.2 × 80 nH / 332 Ω ≈ 0.53 ns
 * This is slightly faster than the IEC spec (0.8 ns), showing
 * that other parasitics also contribute.
 */
double esd_gun_rlc_rise_time(const esd_gun_rlc_params_t *params)
{
    if (!params) return 0.0;

    double l_henry = params->l_par_nh * 1e-9;
    double r_total = params->rd_ohm + params->r_load_ohm;

    if (r_total <= 0.0) return 0.0;

    /* 2.2 × L/R gives approximate 10%-90% rise time */
    double tr_sec = 2.2 * l_henry / r_total;
    return tr_sec * 1e9;  /* Convert seconds to ns */
}

/* ─── RK4 Time-Domain Simulation ───────────────────────────────── */

/**
 * @brief RLC state derivatives for RK4 integration.
 *
 * L3 Mathematical Structure: State-space form of series RLC:
 *
 * State vector x = [Vc, I]
 *
 *   dVc/dt = -I / C
 *   dI/dt  = (Vc - R_total · I) / L
 *
 * These are the coupled first-order ODEs that replace the
 * single second-order ODE:
 *
 *   L · d²i/dt² + R · di/dt + i/C = 0
 *
 * The state-space form enables direct application of numerical
 * integration methods.
 *
 * @param state   [Vc, I]
 * @param derivs  [dVc/dt, dI/dt]
 * @param params  RLC parameters
 */
static void rlc_derivatives(const double state[2],
                             double derivs[2],
                             const esd_gun_rlc_params_t *params)
{
    double vc = state[0];
    double i  = state[1];

    double c_farad = params->cs_pf * 1e-12;
    double l_henry = params->l_par_nh * 1e-9;
    double r_total = params->rd_ohm + params->r_load_ohm;

    if (c_farad <= 0.0) c_farad = 1e-15;
    if (l_henry <= 0.0) l_henry = 1e-15;

    derivs[0] = -i / c_farad;
    derivs[1] = (vc - r_total * i) / l_henry;
}

/**
 * @brief Fourth-order Runge-Kutta (RK4) integration step.
 *
 * L5 Algorithm: Classical RK4 method for solving ODE systems.
 *
 * Given dy/dt = f(t, y), the RK4 step from y_n to y_{n+1} is:
 *
 *   k1 = h · f(t_n,       y_n)
 *   k2 = h · f(t_n + h/2, y_n + k1/2)
 *   k3 = h · f(t_n + h/2, y_n + k2/2)
 *   k4 = h · f(t_n + h,   y_n + k3)
 *
 *   y_{n+1} = y_n + (k1 + 2·k2 + 2·k3 + k4) / 6
 *
 * The method has O(h⁴) local truncation error and O(h⁵) global
 * error, making it well-suited for the smooth (non-stiff) RLC
 * discharge problem.
 *
 * @param state   Current state [Vc, I]
 * @param params  RLC parameters
 * @param dt      Time step
 * @param[out] next  State at next time step
 */
static void rk4_step(const double state[2],
                      const esd_gun_rlc_params_t *params,
                      double dt,
                      double next[2])
{
    double k1[2], k2[2], k3[2], k4[2];
    double temp[2];

    /* k1 */
    rlc_derivatives(state, k1, params);

    /* k2 */
    temp[0] = state[0] + 0.5 * dt * k1[0];
    temp[1] = state[1] + 0.5 * dt * k1[1];
    rlc_derivatives(temp, k2, params);

    /* k3 */
    temp[0] = state[0] + 0.5 * dt * k2[0];
    temp[1] = state[1] + 0.5 * dt * k2[1];
    rlc_derivatives(temp, k3, params);

    /* k4 */
    temp[0] = state[0] + dt * k3[0];
    temp[1] = state[1] + dt * k3[1];
    rlc_derivatives(temp, k4, params);

    /* Combine */
    next[0] = state[0] + (dt / 6.0) * (k1[0] + 2.0*k2[0] + 2.0*k3[0] + k4[0]);
    next[1] = state[1] + (dt / 6.0) * (k1[1] + 2.0*k2[1] + 2.0*k3[1] + k4[1]);
}

/**
 * @brief Simulate RLC discharge using RK4.
 *
 * L6 Canonical Problem: Full transient simulation of the ESD gun
 * discharge. This is the most complete time-domain model that
 * captures the capacitor voltage decay and current waveform
 * with high accuracy.
 *
 * The simulation starts at t=0 with Vc = V_charge and I = 0,
 * then integrates forward in time using RK4 with step dt_ns.
 *
 * Energy conservation check:
 *   E_cap_initial = ½·C·V²
 *   E_dissipated   = ∫ I²·R_total dt
 *   |E_cap - E_dissipated| / E_cap_initial < 1% → good simulation
 */
int esd_gun_simulate_rlc_rk4(const esd_gun_rlc_params_t *params,
                              double dt_ns,
                              double t_end_ns,
                              esd_gun_trajectory_t *traj)
{
    if (!params || !traj || dt_ns <= 0.0 || t_end_ns <= 0.0) {
        return -1;
    }

    size_t n_steps = (size_t)(t_end_ns / dt_ns) + 1;
    if (n_steps < 2) n_steps = 2;

    traj->time_ns = (double *)malloc(n_steps * sizeof(double));
    traj->voltage_kv = (double *)malloc(n_steps * sizeof(double));
    traj->current_a = (double *)malloc(n_steps * sizeof(double));
    traj->arc_resistance_ohm = (double *)malloc(n_steps * sizeof(double));

    if (!traj->time_ns || !traj->voltage_kv ||
        !traj->current_a || !traj->arc_resistance_ohm) {
        esd_gun_trajectory_free(traj);
        return -1;
    }

    traj->num_points = n_steps;

    /* Initial state */
    double state[2];
    state[0] = params->v_charge_kv * 1000.0;  /* kV → V */
    state[1] = 0.0;

    for (size_t k = 0; k < n_steps; k++) {
        double t = (double)k * dt_ns;
        traj->time_ns[k] = t;
        traj->voltage_kv[k] = state[0] * 0.001;  /* V → kV */
        traj->current_a[k] = state[1];
        traj->arc_resistance_ohm[k] = 0.0;  /* No arc in simple RLC model */

        if (k < n_steps - 1) {
            double next[2];
            rk4_step(state, params, dt_ns * 1e-9, next);  /* ns → s for dt */
            state[0] = next[0];
            state[1] = next[1];
        }
    }

    return 0;
}

/**
 * @brief Analytical solution for series RLC discharge.
 *
 * L5 Algorithm: Closed-form solution when arc resistance is
 * treated as constant (or zero).
 *
 * Cases:
 *   Underdamped:
 *     i(t) = V₀/(ω_d·L) · exp(-αt) · sin(ω_d·t)
 *     α = R/(2L), ω_d = √(ω₀² - α²)
 *
 *   Critically damped:
 *     i(t) = V₀/L · t · exp(-αt)
 *
 *   Overdamped:
 *     i(t) = V₀/(2L·β) · [exp(-(α-β)t) - exp(-(α+β)t)]
 *     β = √(α² - ω₀²)
 *
 * The analytical solution serves as a validation reference
 * for the RK4 numerical solution. Comparing both gives an
 * estimate of the numerical error.
 */
int esd_gun_simulate_rlc_analytical(const esd_gun_rlc_params_t *params,
                                     double t_end_ns,
                                     size_t num_points,
                                     esd_gun_trajectory_t *traj)
{
    if (!params || !traj || num_points < 2) {
        return -1;
    }

    traj->time_ns = (double *)malloc(num_points * sizeof(double));
    traj->voltage_kv = (double *)malloc(num_points * sizeof(double));
    traj->current_a = (double *)malloc(num_points * sizeof(double));
    traj->arc_resistance_ohm = (double *)malloc(num_points * sizeof(double));

    if (!traj->time_ns || !traj->voltage_kv ||
        !traj->current_a || !traj->arc_resistance_ohm) {
        esd_gun_trajectory_free(traj);
        return -1;
    }

    traj->num_points = num_points;

    double c_farad = params->cs_pf * 1e-12;
    double l_henry = params->l_par_nh * 1e-9;
    double r_total = params->rd_ohm + params->r_load_ohm;
    double v0      = params->v_charge_kv * 1000.0;

    double alpha = r_total / (2.0 * l_henry);
    double w0_sq = 1.0 / (l_henry * c_farad);
    double w0 = sqrt(w0_sq);
    double regime = esd_gun_rlc_damping_regime(params);

    double dt = t_end_ns / (double)(num_points - 1);

    for (size_t k = 0; k < num_points; k++) {
        double t_sec = (double)k * dt * 1e-9;  /* ns → s */
        double i_a = 0.0;
        double vc_v = v0;

        if (regime == 0) {
            /* Underdamped */
            double wd = w0 * sqrt(1.0 - (alpha*alpha)/(w0_sq));
            i_a = v0 / (wd * l_henry) * exp(-alpha * t_sec) * sin(wd * t_sec);
            vc_v = v0 * exp(-alpha * t_sec) *
                   (cos(wd * t_sec) + alpha/wd * sin(wd * t_sec));
        } else if (regime == 1) {
            /* Critically damped */
            i_a = v0 / l_henry * t_sec * exp(-alpha * t_sec);
            vc_v = v0 * (1.0 + alpha * t_sec) * exp(-alpha * t_sec);
        } else {
            /* Overdamped */
            double beta = sqrt(alpha*alpha - w0_sq);
            i_a = v0 / (2.0 * l_henry * beta) *
                  (exp(-(alpha - beta) * t_sec) - exp(-(alpha + beta) * t_sec));
            vc_v = v0 * exp(-alpha * t_sec) *
                   (cosh(beta * t_sec) + alpha/beta * sinh(beta * t_sec));
        }

        traj->time_ns[k] = (double)k * dt;
        traj->voltage_kv[k] = vc_v * 0.001;
        traj->current_a[k] = i_a;
        traj->arc_resistance_ohm[k] = 0.0;
    }

    return 0;
}

/* ─── Arc Resistance Models ────────────────────────────────────── */

/**
 * @brief RLC derivatives with time-varying arc resistance.
 *
 * Modified state equations including arc resistance R_arc(t):
 *
 *   dVc/dt = -I / C
 *   dI/dt  = (Vc - (R_total + R_arc(t)) · I) / L
 *
 * The arc resistance is computed external to the derivative
 * function because it depends on the accumulated energy/charge
 * integral, not just the instantaneous state.
 */
static void rlc_arc_derivatives(const double state[2],
                                 double derivs[2],
                                 const esd_gun_rlc_params_t *params,
                                 double r_arc_ohm)
{
    double vc = state[0];
    double i  = state[1];

    double c_farad = params->cs_pf * 1e-12;
    double l_henry = params->l_par_nh * 1e-9;
    double r_total = params->rd_ohm + params->r_load_ohm + r_arc_ohm;

    if (c_farad <= 0.0) c_farad = 1e-15;
    if (l_henry <= 0.0) l_henry = 1e-15;

    derivs[0] = -i / c_farad;
    derivs[1] = (vc - r_total * i) / l_henry;
}

/**
 * @brief RK4 step with arc resistance.
 */
static void rk4_arc_step(const double state[2],
                          const esd_gun_rlc_params_t *params,
                          double r_arc_ohm,
                          double dt,
                          double next[2])
{
    double k1[2], k2[2], k3[2], k4[2];
    double temp[2];

    rlc_arc_derivatives(state, k1, params, r_arc_ohm);

    temp[0] = state[0] + 0.5 * dt * k1[0];
    temp[1] = state[1] + 0.5 * dt * k1[1];
    rlc_arc_derivatives(temp, k2, params, r_arc_ohm);

    temp[0] = state[0] + 0.5 * dt * k2[0];
    temp[1] = state[1] + 0.5 * dt * k2[1];
    rlc_arc_derivatives(temp, k3, params, r_arc_ohm);

    temp[0] = state[0] + dt * k3[0];
    temp[1] = state[1] + dt * k3[1];
    rlc_arc_derivatives(temp, k4, params, r_arc_ohm);

    next[0] = state[0] + (dt / 6.0) * (k1[0] + 2.0*k2[0] + 2.0*k3[0] + k4[0]);
    next[1] = state[1] + (dt / 6.0) * (k1[1] + 2.0*k2[1] + 2.0*k3[1] + k4[1]);
}

/**
 * @brief Simulate ESD discharge with Rompe-Weizel arc model.
 *
 * L6 Canonical Problem: Air discharge simulation.
 *
 * Rompe-Weizel spark resistance law:
 *
 *   R_arc(t) = d / √( 2α · ∫₀ᵗ i²(τ) dτ / p )
 *
 * where:
 *   d = gap length [m]
 *   α = spark constant ≈ 0.5-1.0 × 10⁻⁴ m²/(V²·s)  (for air)
 *   p = pressure [Pa = N/m²]
 *
 * The integral ∫ i² dτ represents the energy deposited in the
 * spark channel. As more energy is deposited:
 *   - The channel heats up → more ionization → lower resistance
 *   - R_arc decreases from near-infinite to ~1-10 Ω during the arc
 *
 * This nonlinearity couples the electrical and thermal dynamics,
 * making the Rompe-Weizel model the most physically accurate
 * for spark-gap air discharge simulation.
 */
int esd_gun_simulate_with_arc_rw(const esd_gun_rlc_params_t *params,
                                  const arc_rompe_weizel_t *arc,
                                  double dt_ns,
                                  double t_end_ns,
                                  esd_gun_trajectory_t *traj)
{
    if (!params || !arc || !traj || dt_ns <= 0.0 || t_end_ns <= 0.0) {
        return -1;
    }

    size_t n_steps = (size_t)(t_end_ns / dt_ns) + 1;
    if (n_steps < 2) n_steps = 2;

    traj->time_ns = (double *)malloc(n_steps * sizeof(double));
    traj->voltage_kv = (double *)malloc(n_steps * sizeof(double));
    traj->current_a = (double *)malloc(n_steps * sizeof(double));
    traj->arc_resistance_ohm = (double *)malloc(n_steps * sizeof(double));

    if (!traj->time_ns || !traj->voltage_kv ||
        !traj->current_a || !traj->arc_resistance_ohm) {
        esd_gun_trajectory_free(traj);
        return -1;
    }

    traj->num_points = n_steps;

    /* Convert units */
    double d_m    = arc->gap_length_mm * 1e-3;
    double alpha  = arc->alpha_coeff;
    double p_atm  = arc->pressure_bar * 101325.0;  /* bar → Pa */

    /* Initial state */
    double state[2];
    state[0] = params->v_charge_kv * 1000.0;
    state[1] = 0.0;

    /* Accumulated ∫ i² dt */
    double energy_integral = 0.0;

    /* Default arc resistance before breakdown (very high) */
    double r_arc = 1e9;  /* 1 GΩ → effectively open circuit */

    for (size_t k = 0; k < n_steps; k++) {
        double t = (double)k * dt_ns;
        traj->time_ns[k] = t;
        traj->voltage_kv[k] = state[0] * 0.001;
        traj->current_a[k] = state[1];
        traj->arc_resistance_ohm[k] = r_arc;

        if (k < n_steps - 1) {
            /* Update energy integral: ∫ i² dt (trapezoidal) */
            double dt_s = dt_ns * 1e-9;
            double next_tmp[2];
            rk4_arc_step(state, params, r_arc, dt_s, next_tmp);

            /* Trapezoidal update of energy integral */
            double i_avg = 0.5 * (state[1] + next_tmp[1]);
            energy_integral += i_avg * i_avg * dt_s;

            /* Update arc resistance using Rompe-Weizel law */
            if (energy_integral > 1e-20) {
                double denominator = sqrt(2.0 * alpha * energy_integral / p_atm);
                if (denominator > 1e-15) {
                    r_arc = d_m / denominator;
                    /* Clamp arc resistance to physically reasonable range */
                    if (r_arc < 0.1) r_arc = 0.1;    /* min ~0.1 Ω */
                    if (r_arc > 1e9) r_arc = 1e9;    /* max before breakdown */
                }
            }

            state[0] = next_tmp[0];
            state[1] = next_tmp[1];
        }
    }

    return 0;
}

/**
 * @brief Simulate with Toepler arc model.
 *
 * L3: Toepler's spark law:
 *
 *   R_arc(t) = k_T · d / ∫₀ᵗ i(τ) dτ
 *
 * where k_T ≈ 4 × 10⁻⁵ V·s/m for air at atmospheric pressure.
 *
 * The Toepler model relates arc resistance to the accumulated
 * charge (∫ i dt) rather than energy. It has the advantage of
 * being simpler to compute but is less physically accurate
 * for high-energy arcs.
 */
int esd_gun_simulate_with_arc_toepler(const esd_gun_rlc_params_t *params,
                                       const arc_toepler_t *arc,
                                       double dt_ns,
                                       double t_end_ns,
                                       esd_gun_trajectory_t *traj)
{
    if (!params || !arc || !traj || dt_ns <= 0.0 || t_end_ns <= 0.0) {
        return -1;
    }

    size_t n_steps = (size_t)(t_end_ns / dt_ns) + 1;
    if (n_steps < 2) n_steps = 2;

    traj->time_ns = (double *)malloc(n_steps * sizeof(double));
    traj->voltage_kv = (double *)malloc(n_steps * sizeof(double));
    traj->current_a = (double *)malloc(n_steps * sizeof(double));
    traj->arc_resistance_ohm = (double *)malloc(n_steps * sizeof(double));

    if (!traj->time_ns || !traj->voltage_kv ||
        !traj->current_a || !traj->arc_resistance_ohm) {
        esd_gun_trajectory_free(traj);
        return -1;
    }

    traj->num_points = n_steps;

    double d_m = arc->gap_length_mm * 1e-3;
    double k_t = arc->k_toepler;

    double state[2];
    state[0] = params->v_charge_kv * 1000.0;
    state[1] = 0.0;

    double charge_integral = 0.0;  /* ∫ i dt */
    double r_arc = 1e9;

    for (size_t k = 0; k < n_steps; k++) {
        double t = (double)k * dt_ns;
        traj->time_ns[k] = t;
        traj->voltage_kv[k] = state[0] * 0.001;
        traj->current_a[k] = state[1];
        traj->arc_resistance_ohm[k] = r_arc;

        if (k < n_steps - 1) {
            double dt_s = dt_ns * 1e-9;
            double next_tmp[2];
            rk4_arc_step(state, params, r_arc, dt_s, next_tmp);

            /* Trapezoidal update of charge integral */
            double i_avg = 0.5 * (state[1] + next_tmp[1]);
            charge_integral += i_avg * dt_s;

            if (fabs(charge_integral) > 1e-15) {
                r_arc = k_t * d_m / fabs(charge_integral);
                if (r_arc < 0.1) r_arc = 0.1;
                if (r_arc > 1e9) r_arc = 1e9;
            }

            state[0] = next_tmp[0];
            state[1] = next_tmp[1];
        }
    }

    return 0;
}

/* ─── Paschen's Law ──────────────────────────────────────────── */

/**
 * @brief Breakdown voltage from Paschen's law.
 *
 * L4 Theorem: Paschen's law (1889) gives the breakdown voltage
 * of a gas between two parallel plate electrodes as a function
 * of the product of pressure p and gap distance d:
 *
 *   V_bd = B · p · d / ( ln(A · p · d) - ln(ln(1 + 1/γ)) )
 *
 * where:
 *   A = Townsend's first ionization coefficient factor
 *   B = Townsend's second coefficient
 *   γ = secondary electron emission coefficient
 *
 * Simplified for air (at room temperature, 1 atm):
 *
 *   V_bd [kV] ≈ 2.44 · d [mm] + 2.12   (for d > 0.1 mm)
 *
 * More commonly used form:
 *   V_bd [V] ≈ 3000 + 1000 · d [mm]   (for 1-10 mm at 1 atm)
 *
 * This simplified form is used because the full Townsend
 * coefficients vary with electrode geometry, surface condition,
 * humidity, and gas composition.
 */
double esd_paschen_breakdown_voltage(double gap_length_mm,
                                      double pressure_bar)
{
    if (gap_length_mm <= 0.0 || pressure_bar <= 0.0) {
        return 0.0;
    }

    /* Use simplified air breakdown formula, scaled by pressure */
    /* V_bd ≈ 3000 + 1000·d [mm]  at 1 bar */
    double v_at_1bar = 3000.0 + 1000.0 * gap_length_mm;

    /* Scale linearly with pressure (valid for moderate pressure range) */
    return v_at_1bar * pressure_bar;
}

/**
 * @brief Estimate gap length from breakdown voltage.
 *
 * L5 Algorithm: Inverse of Paschen's law.
 *
 * Uses the simplified approximation:
 *   d [mm] ≈ (V_bd - 3000) / 1000
 *
 * This allows estimating the effective spark gap during an
 * ESD event from the measured discharge voltage. For example,
 * an 8 kV air discharge corresponds to approximately 5 mm gap.
 */
double esd_voltage_to_gap_length(double voltage_kv, double pressure_bar)
{
    if (voltage_kv <= 0.0 || pressure_bar <= 0.0) {
        return 0.0;
    }

    double voltage_v = voltage_kv * 1000.0;
    double v_at_1bar = voltage_v / pressure_bar;
    double gap_mm = (v_at_1bar - 3000.0) / 1000.0;

    return (gap_mm > 0.0) ? gap_mm : 0.0;
}

/* ─── Stored Energy ──────────────────────────────────────────── */

/**
 * @brief Energy stored in ESD gun capacitor.
 *
 * L4 Theorem: The electrostatic potential energy stored is:
 *
 *   E = ½ · C · V²
 *
 * For the IEC gun:
 *   Level 1 (2 kV):  E = ½ × 150e-12 × (2000)² = 0.3 mJ
 *   Level 4 (8 kV):  E = ½ × 150e-12 × (8000)² = 4.8 mJ
 *
 * This is the total energy available for discharge. Most of it
 * is dissipated in the internal 330 Ω resistor; only a fraction
 * reaches the DUT.
 */
double esd_gun_stored_energy(double cs_pf, double voltage_kv)
{
    double c_farad = cs_pf * 1e-12;
    double v_volts = voltage_kv * 1000.0;
    double energy_joules = 0.5 * c_farad * v_volts * v_volts;
    return energy_joules * 1e6;  /* J → µJ */
}

/**
 * @brief Impedance of series RLC at a given frequency.
 *
 * L3: Z(jω) = R + j(ωL - 1/(ωC))
 *
 *   |Z| = √[R² + (ωL - 1/(ωC))²]
 *
 * At resonance (ω = ω₀):
 *   ωL = 1/(ωC) → |Z| = R (purely resistive, minimum impedance)
 *
 * For the IEC gun (R=332 Ω, L=80 nH, C=150 pF):
 *   Resonance freq = 1/(2π√(LC)) ≈ 45.9 MHz
 *   At DC: |Z| → ∞ (capacitor open)
 *   At 1 GHz: |Z| ≈ √(332² + (503-1.06)²) ≈ √(110224 + 251500) ≈ 601 Ω
 */
double esd_gun_impedance(const esd_gun_rlc_params_t *params, double freq_hz)
{
    double c_farad = params->cs_pf * 1e-12;
    double l_henry = params->l_par_nh * 1e-9;
    double r_total = params->rd_ohm + params->r_load_ohm;

    if (freq_hz <= 0.0) {
        return 1e12;  /* DC: capacitor blocks */
    }

    double omega = 2.0 * M_PI * freq_hz;

    /* Reactance: X = ωL - 1/(ωC) */
    double x_l = omega * l_henry;
    double x_c = 1.0 / (omega * c_farad);
    double x   = x_l - x_c;

    return sqrt(r_total * r_total + x * x);
}

/**
 * @brief Transfer function magnitude of ESD gun circuit.
 *
 * L3: For the series RLC with load R_L:
 *
 *   H(s) = V_out(s) / V_in(s) = R_L / (R_d + R_L + sL + 1/(sC))
 *
 *   |H(jω)| = R_L / √[(R_total)² + (ωL - 1/(ωC))²]
 *
 * This is essentially a band-pass filter whose frequency response
 * determines how fast the ESD pulse can change.
 */
double esd_gun_transfer_magnitude(const esd_gun_rlc_params_t *params,
                                   double freq_hz)
{
    double c_farad = params->cs_pf * 1e-12;
    double l_henry = params->l_par_nh * 1e-9;
    double r_total = params->rd_ohm + params->r_load_ohm;
    double r_load  = params->r_load_ohm;

    if (freq_hz <= 0.0) {
        return 0.0;  /* DC blocked by capacitor */
    }

    double omega = 2.0 * M_PI * freq_hz;
    double x_l = omega * l_henry;
    double x_c = 1.0 / (omega * c_farad);
    double x   = x_l - x_c;

    double denominator = sqrt(r_total * r_total + x * x);

    if (denominator <= 0.0) return 0.0;

    return r_load / denominator;
}

/* ─── Memory Management ──────────────────────────────────────── */

void esd_gun_trajectory_free(esd_gun_trajectory_t *traj)
{
    if (traj) {
        free(traj->time_ns);
        free(traj->voltage_kv);
        free(traj->current_a);
        free(traj->arc_resistance_ohm);
        memset(traj, 0, sizeof(*traj));
    }
}

int esd_gun_rlc_print(const esd_gun_rlc_params_t *params,
                       char *buffer, size_t buf_size)
{
    if (!params || !buffer || buf_size == 0) return -1;

    snprintf(buffer, buf_size,
             "ESD Gun RLC Model:\n"
             "  Cs       = %.1f pF\n"
             "  Rd       = %.1f ohm\n"
             "  L_par    = %.1f nH\n"
             "  R_load   = %.1f ohm\n"
             "  V_charge = %.1f kV\n",
             params->cs_pf,
             params->rd_ohm,
             params->l_par_nh,
             params->r_load_ohm,
             params->v_charge_kv);

    return 0;
}
