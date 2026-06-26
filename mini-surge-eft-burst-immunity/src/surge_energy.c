/**
 * surge_energy.c - Surge Energy Analysis and Thermal Management
 *
 * L4 Fundamental Laws: Energy conservation in surge events,
 * Joule heating, thermal runaway prevention.
 * L5 Algorithms: Energy absorption calculation, temperature
 * rise prediction, device lifetime estimation.
 *
 * Reference:
 *   MIL-STD-1275: Surge Protection for Military Vehicles
 *   ISO 7637-2: Road Vehicles - Electrical Disturbances
 *   Andoh et al., "MOV Degradation Under Surge Stress," IEEE TPD, 2001
 */

#include "surge_defs.h"
#include "surge_waveform.h"
#include "surge_protection.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ==================================================================
 * L4: Surge Energy into Various Load Types
 *
 * The energy dissipated depends on the load type:
 *   Resistive: E = integral(v^2/R) dt
 *   Capacitive: E = 1/2*C*(V_final^2 - V_initial^2)
 *   Inductive: E = 1/2*L*I^2 (worst-case inductive kickback)
 *
 * For MOV protection: energy is absorbed as heat in the ZnO grains.
 * For TVS: energy is dissipated in the avalanche region of the p-n junction.
 * For GDT: energy is dissipated in the arc plasma.
 * ================================================================== */

/**
 * Compute surge energy absorbed by a capacitive load.
 *
 * A capacitor charged by a surge absorbs energy:
 *   E = 0.5 * C * V_final^2
 *
 * This is critical for EFT injection via capacitive coupling clamp:
 *   C_coupling = 33 nF, V_peak = 4 kV
 *   E = 0.5 * 33e-9 * (4000)^2 = 0.264 J
 *
 * Complexity: O(1)
 */
double surge_energy_capacitive(double capacitance_uf, double v_peak_kv)
{
    /* E = 0.5 * C * V^2
     *
     * capacitance_uf: in microfarads
     * v_peak_kv: in kilovolts
     *
     * E = 0.5 * C_uf*1e-6 * (V_kv*1000)^2
     *   = 0.5 * C_uf * 1e-6 * V_kv^2 * 1e6
     *   = 0.5 * C_uf * V_kv^2
     *
     * So for C in uF and V in kV: E = 0.5 * C * V^2 in Joules
     */
    if (capacitance_uf <= 0.0 || v_peak_kv <= 0.0) return 0.0;

    return SURGE_ENERGY_CONSTANT * capacitance_uf * v_peak_kv * v_peak_kv;
}

/**
 * Compute surge energy absorbed by an inductive load (kickback).
 *
 * When current through an inductor is interrupted, the stored
 * magnetic energy is released:
 *   E = 0.5 * L * I^2
 *
 * This is the energy available for a flyback transient.
 * Vehicle load dump (ISO 7637-2) is a classic example:
 *   Alternator inductance ~ 0.1-10 mH, load current ~ 10-100A
 *   E = 0.5 * 5e-3 * 100^2 = 25 J  (significant!)
 *
 * Complexity: O(1)
 */
double surge_energy_inductive(double inductance_uh, double current_a)
{
    /* E = 0.5 * L * I^2
     *
     * inductance_uh: in microhenries
     * current_a: in amperes
     *
     * E = 0.5 * L_uh * 1e-6 * I_a^2  (Joules)
     */
    if (inductance_uh <= 0.0 || current_a <= 0.0) return 0.0;

    return SURGE_ENERGY_CONSTANT * inductance_uh * 1e-6 * current_a * current_a;
}

/**
 * Compute the energy deposited in protection device during clamping.
 *
 * While the device is clamping, it dissipates:
 *   E_prot = integral_0^T v_clamp(t) * i_surge(t) dt
 *
 * Simplified approximation using average values:
 *   E_prot ~ V_clamp * I_peak * tau_effective
 *
 * where tau_effective ~ t_duration / 3 for double-exponential.
 *
 * Complexity: O(1)
 */
double surge_energy_in_protector(double v_clamp, double i_peak,
                                  double t_duration_us)
{
    if (v_clamp <= 0.0 || i_peak <= 0.0 || t_duration_us <= 0.0) return 0.0;

    /* tau_effective ~ t_duration / (2*sqrt(2))
     * but we use a conservative factor of 1/2
     */
    double tau_eff = t_duration_us * 1e-6 * 0.5;
    return v_clamp * i_peak * tau_eff;
}

/* ==================================================================
 * L5: Thermal Runaway Analysis
 *
 * MOV degradation is a thermal process:
 * 1. Each surge pulse heats the ZnO grains
 * 2. Over many pulses, the grain boundaries degrade (Bi2O3 phase
 *    migration, oxygen vacancy diffusion)
 * 3. Degradation increases leakage current
 * 4. Increased leakage causes self-heating even at normal voltage
 * 5. Thermal runaway: heating -> more leakage -> more heating
 *    -> catastrophic failure (short circuit, sometimes fire)
 *
 * Critical parameters:
 *   - Maximum junction temperature (typically 85-125 C for MOV)
 *   - Power dissipation capability (thermal resistance to ambient)
 *   - Leakage current temperature coefficient
 * ================================================================== */

/**
 * Estimate MOV lifetime under repetitive surge stress.
 *
 * Empirical model based on Arrhenius equation for thermal degradation:
 *   Life = A * exp(Ea/(k*T))
 *
 * where Ea is the activation energy for ZnO grain boundary degradation
 * (~0.6-0.8 eV), k is Boltzmann's constant, and T is the operating
 * temperature.
 *
 * For MOV surge life estimation, we use the power-law model:
 *   N_failure = N_rated * (I_rated / I_actual)^n * (E_rated / E_actual)
 *
 * where n is typically 3-5 for MOV devices.
 *
 * Complexity: O(1)
 * Reference: Andoh et al., "MOV Degradation," IEEE TPD, 2001
 */
int surge_mov_lifetime_estimate(const protection_device_params_t *mov,
                                 const surge_waveform_params_t *wf,
                                 double ambient_temp_c,
                                 double *estimated_pulses)
{
    if (!mov || !wf || !estimated_pulses) return SURGE_ERR_NULL_PTR;
    if (mov->type != PROT_MOV) return SURGE_ERR_INVALID_WAVEFORM;

    /* Power-law degradation model */
    double i_surge = wf->i_peak > 0.0 ? wf->i_peak :
                     wf->v_peak / wf->source_impedance;

    /* Current stress ratio */
    double i_ratio = i_surge / mov->i_peak_rated;
    if (i_ratio <= 0.0) i_ratio = 0.01;

    /* Energy stress ratio */
    double e_ratio = wf->energy_per_pulse / mov->energy_rating;
    if (e_ratio <= 0.0) e_ratio = 0.01;

    /* Combined degradation: Life = rated * (I_rated/I_surge)^3 / (1 + E/E_rated) */
    double exp_factor = 3.0;  /* MOV current exponent */
    double life_from_current = pow(1.0 / i_ratio, exp_factor);
    double life_from_energy = 1.0 / (1.0 + e_ratio);

    double base_life = life_from_current * life_from_energy;

    /* Temperature derating: 50% life reduction per 10degC above 25C */
    double temp_over_25 = ambient_temp_c - 25.0;
    double temp_factor;
    if (temp_over_25 > 0.0) {
        temp_factor = pow(0.5, temp_over_25 / 10.0);
    } else {
        temp_factor = 1.0;
    }

    /* Total estimated pulses to failure */
    *estimated_pulses = base_life * (double)mov->surge_life_cycles * temp_factor;

    if (*estimated_pulses < 1.0) *estimated_pulses = 1.0;

    return SURGE_OK;
}

/**
 * Check for thermal runaway condition in MOV.
 *
 * Thermal runaway occurs when:
 *   P_self_heating > P_dissipation_capability
 *
 * P_self_heating = V_line^2 * leakage_current(V_line, T)
 * where leakage current increases exponentially with temperature.
 *
 * P_dissipation_capability = (T_junction - T_ambient) / R_thermal
 *
 * Stability criterion:
 *   d(P_self_heating)/dT < 1/R_thermal
 *
 * Complexity: O(1)
 */
int surge_check_thermal_runaway(const protection_device_params_t *dev,
                                 double v_line, double ambient_temp_c,
                                 int *is_stable)
{
    if (!dev || !is_stable) return SURGE_ERR_NULL_PTR;
    if (v_line <= 0.0) {
        *is_stable = 1;
        return SURGE_OK;
    }

    /* Estimate leakage current at operating voltage and temperature.
     * Leakage current doubles approximately every 12-15 degC for MOV. */
    double i_leak_base = dev->leakage_current_ua * 1e-6;  /* Convert uA to A */
    double temp_over_25 = ambient_temp_c - 25.0;
    double i_leak = i_leak_base * pow(2.0, temp_over_25 / 15.0);

    /* Self-heating power */
    double p_self = v_line * i_leak;

    /* Available dissipation */
    double t_j_max = 125.0;  /* Typical MOV max junction temp */
    double p_diss_max = (t_j_max - ambient_temp_c) / dev->thermal_resistance;
    if (p_diss_max < 0.0) p_diss_max = 0.0;

    /* Stability: self-heating < dissipation */
    /* With 2x safety margin for transients */
    *is_stable = (p_self * 2.0) < p_diss_max ? 1 : 0;

    return SURGE_OK;
}

/* ==================================================================
 * L5: Protection Coordination Energy Budget
 * ================================================================== */

/**
 * Compute total energy budget for a multi-stage protection scheme.
 *
 * The total surge energy must be distributed among stages such that
 * no single stage exceeds its energy rating.
 *
 * Energy budget allocation:
 *   Stage 1 (GDT/MOV): 60-80% of total energy (bulk absorption)
 *   Stage 2 (MOV/TVS):  15-30% (residual handling)
 *   Stage 3 (TVS):       5-10% (precision clamping)
 *
 * Complexity: O(n_stages)
 */
int surge_energy_budget(const multistage_protection_t *mp,
                         double *energy_per_stage)
{
    if (!mp || !energy_per_stage) return SURGE_ERR_NULL_PTR;
    if (mp->num_stages < 1 || mp->num_stages > 5)
        return SURGE_ERR_INVALID_WAVEFORM;

    int s;
    double total = mp->total_energy_absorbed;

    for (s = 0; s < mp->num_stages; s++) {
        energy_per_stage[s] = total * mp->stage_energy_fraction[s];
    }

    return SURGE_OK;
}

/**
 * Calculate required MOV disk diameter for given energy absorption.
 *
 * Empirical sizing:
 *   5mm disk:  10 J max (single pulse)
 *   7mm disk:  30 J max
 *   10mm disk: 55 J max
 *   14mm disk: 100 J max
 *   20mm disk: 200 J max
 *
 * For repetitive pulses, derate by factor of 0.5-0.8 depending
 * on repetition rate and cooling.
 *
 * Complexity: O(1)
 */
int surge_mov_sizing(double energy_per_pulse, int num_pulses,
                      double repetition_rate_hz,
                      int *disk_diameter_mm, double *derated_energy_j)
{
    if (!disk_diameter_mm || !derated_energy_j) return SURGE_ERR_NULL_PTR;
    if (energy_per_pulse <= 0.0) return SURGE_ERR_INVALID_WAVEFORM;

    /* Multi-pulse derating factor:
     *   - Single pulse: 1.0
     *   - 2-10 pulses:  0.8
     *   - 11-100:        0.6
     *   - 101-1000:      0.4
     *   - 1000+:         0.3
     */
    double derate;
    if (num_pulses <= 1) {
        derate = 1.0;
    } else if (num_pulses <= 10) {
        derate = 0.8;
    } else if (num_pulses <= 100) {
        derate = 0.6;
    } else if (num_pulses <= 1000) {
        derate = 0.4;
    } else {
        derate = 0.3;
    }

    /* Additional derating for high repetition rate */
    if (repetition_rate_hz > 1.0) {
        derate *= 0.8;
    }
    if (repetition_rate_hz > 100.0) {
        derate *= 0.5;
    }

    double effective_energy = energy_per_pulse / derate;

    /* Select disk diameter */
    if (effective_energy <= 5.0) {
        *disk_diameter_mm = 5;
        *derated_energy_j = 10.0 * derate;
    } else if (effective_energy <= 15.0) {
        *disk_diameter_mm = 7;
        *derated_energy_j = 30.0 * derate;
    } else if (effective_energy <= 30.0) {
        *disk_diameter_mm = 10;
        *derated_energy_j = 55.0 * derate;
    } else if (effective_energy <= 60.0) {
        *disk_diameter_mm = 14;
        *derated_energy_j = 100.0 * derate;
    } else if (effective_energy <= 120.0) {
        *disk_diameter_mm = 20;
        *derated_energy_j = 200.0 * derate;
    } else {
        /* Energy exceeds single MOV capability - use parallel MOVs */
        int disks_needed = (int)ceil(effective_energy / 200.0);
        *disk_diameter_mm = 20 * disks_needed;  /* Indicate multiple disks */
        *derated_energy_j = 200.0 * (double)disks_needed * derate;
    }

    return SURGE_OK;
}

/**
 * Compute the total surge energy delivered by a surge generator
 * capacitor discharge.
 *
 * IEC surge generators use a charged capacitor:
 *   - Combination wave (1.2/50us): C_gen = 10 uF @ up to 4 kV
 *     E = 0.5 * 10e-6 * (4000)^2 = 80 J
 *   - Combination wave (1.2/50us, 2 ohm): peak current = 2 kA
 *   - Telecom (10/700us): C_gen = 20 uF @ up to 4 kV
 *     E = 0.5 * 20e-6 * (4000)^2 = 160 J
 *
 * Complexity: O(1)
 */
double surge_generator_stored_energy(surge_waveform_type_t type,
                                      double v_charge_kv)
{
    if (v_charge_kv <= 0.0) return 0.0;

    double cap_uf;

    switch (type) {
        case SURGE_WAVE_1_2_50_US:
        case SURGE_WAVE_8_20_US:
            cap_uf = 10.0;  /* 10 uF, IEC combination wave generator */
            break;
        case SURGE_WAVE_10_700_US:
        case SURGE_WAVE_10_1000_US:
            cap_uf = 20.0;  /* 20 uF, telecom surge generator */
            break;
        case SURGE_WAVE_10_350_US:
            cap_uf = 200.0; /* 200 uF, high-energy lightning generator */
            break;
        case SURGE_WAVE_5_50_NS:
            cap_uf = 0.01;  /* 10 nF, EFT generator */
            break;
        case SURGE_WAVE_RING_0_5_US:
            cap_uf = 0.5;   /* 0.5 uF, ring wave generator */
            break;
        default:
            cap_uf = 10.0;
            break;
    }

    return SURGE_ENERGY_CONSTANT * cap_uf * v_charge_kv * v_charge_kv;
}

/**
 * Estimate the temperature of a protection device after a surge.
 *
 *   T_final = T_initial + E_absorbed / C_thermal
 *
 * where C_thermal is the device's thermal capacity (J/K).
 *
 * For rapid successive surges, the temperature may not return
 * to ambient, leading to cumulative heating and possible
 * thermal runaway.
 *
 * Complexity: O(1)
 */
double surge_device_temperature_after_surge(double t_initial_c,
                                             double energy_absorbed_j,
                                             double thermal_capacity_j_per_k)
{
    if (thermal_capacity_j_per_k <= 0.0) return t_initial_c;
    return t_initial_c + energy_absorbed_j / thermal_capacity_j_per_k;
}

/**
 * Calculate thermal time constant for cooling between surges.
 *
 *   T(t) = T_ambient + (T_initial - T_ambient) * exp(-t/tau_thermal)
 *
 * For MOV: tau_thermal depends on disk size (5mm: ~1s, 20mm: ~10s)
 * For TVS (SMB): tau_thermal ~ 10-100ms
 *
 * This determines if the device cools between bursts.
 *
 * Complexity: O(1)
 */
double surge_cooling_temperature(double t_initial_c, double t_ambient_c,
                                  double time_s, double tau_thermal_s)
{
    if (tau_thermal_s <= 0.0) return t_ambient_c;

    double delta = t_initial_c - t_ambient_c;
    if (delta <= 0.0) return t_ambient_c;

    return t_ambient_c + delta * exp(-time_s / tau_thermal_s);
}