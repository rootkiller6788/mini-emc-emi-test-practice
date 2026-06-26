/**
 * emi_probe_theory.h - Near-Field Probe Theory for Low-Cost EMC Pre-Compliance
 *
 * Models for E-field and H-field near-field probes, coupling mechanisms,
 * calibration methods, and wave impedance characterization.
 *
 * References:
 *   - Paul, C.R., "Introduction to EMC", 2nd ed., Wiley, 2006. Chapter 4.
 *   - Ott, H.W., "EMC Engineering", Wiley, 2009. Chapter 11.
 *   - Kanda, M., "Standard Probes for EM Field Measurements", IEEE Trans. AP, 1993.
 *   - Balanis, C.A., "Antenna Theory", 4th ed., Wiley, 2016.
 *
 * Knowledge Coverage:
 *   L1: E/H probe definitions, transfer impedance, calibration factor
 *   L2: Near-field coupling, wave impedance transition, probe loading effects
 *   L3: Faraday's law, Biot-Savart, Coulomb's law for probe response
 *   L4: Maxwell-based coupling models, mutual inductance/capacitance
 *   L5: Probe calibration algorithms, frequency response de-embedding
 */

#ifndef EMI_PROBE_THEORY_H
#define EMI_PROBE_THEORY_H

#include "emi_precompliance_core.h"
#include <stdint.h>

/* E-field Probe Specification (L1) */

typedef struct {
    double probe_length_m;
    double probe_diameter_m;
    double input_capacitance_f;
    double input_resistance_ohm;
    double transfer_impedance_ohm;
    char   calibration_date[16];
    double cal_factor_db;
} emi_eprobe_spec_t;

/* H-field Probe Specification (L1) */

typedef struct {
    double loop_diameter_m;
    int    num_turns;
    double wire_diameter_m;
    double shield_gap_rad;
    double self_inductance_h;
    double self_resonance_hz;
    double antenna_factor_db_sm;
    char   calibration_date[16];
    double cal_factor_db;
} emi_hprobe_spec_t;

/* Frequency-dependent probe transfer function */

typedef struct {
    int     num_freqs;
    double *freq_hz;
    double *transfer_mag_db;
    double *transfer_phase_rad;
    double  freq_min_hz;
    double  freq_max_hz;
} emi_probe_transfer_t;

/* E-Field Probe Physics (L2, L3, L4) */

/** E-field probe equivalent capacitance (monopole model).
 *  C = 2*pi*eps0 * h / (ln(2h/a) - 1)
 *  Ref: Balanis Eq. 4-56, short dipole capacitance. */
static inline double emi_eprobe_capacitance(double length_m, double diameter_m) {
    double h = length_m;
    double a = diameter_m / 2.0;
    double eps0 = 8.8541878128e-12;
    if (a <= 0.0 || h <= a) return 1e-15;
    return 2.0 * EMI_PI * eps0 * h / (log(2.0 * h / a) - 1.0);
}

double emi_eprobe_output_voltage(double efield_vm, const emi_eprobe_spec_t *probe,
                                  double load_cap_f);

double emi_eprobe_efield_from_voltage(double vout, const emi_eprobe_spec_t *probe,
                                       double load_cap_f);

double emi_eprobe_sensitivity_mag(double freq_hz, const emi_eprobe_spec_t *probe,
                                   double load_cap_f);

/** E-field probe -3dB bandwidth: f_3dB = 1/(2*pi*R_in*(C_ant + C_load)) */
static inline double emi_eprobe_bandwidth_hz(const emi_eprobe_spec_t *probe,
                                              double load_cap_f) {
    double c_total = emi_eprobe_capacitance(probe->probe_length_m,
                                             probe->probe_diameter_m) + load_cap_f;
    if (c_total <= 0.0 || probe->input_resistance_ohm <= 0.0) return 1e9;
    return 1.0 / (2.0 * EMI_PI * probe->input_resistance_ohm * c_total);
}

/* H-Field Probe Physics (L2, L3, L4) */

double emi_hprobe_self_inductance(double loop_radius_m, double wire_radius_m,
                                   int num_turns);

double emi_hprobe_output_voltage(double freq_hz, double hfield_am,
                                  const emi_hprobe_spec_t *probe);

double emi_hprobe_hfield_from_voltage(double freq_hz, double vout,
                                       const emi_hprobe_spec_t *probe);

double emi_hprobe_antenna_factor(double freq_hz, const emi_hprobe_spec_t *probe);

double emi_hprobe_self_resonance(const emi_hprobe_spec_t *probe);

/* Near-Field Coupling Models (L3 Math Structures) */

/** Mutual inductance between two coaxial circular loops.
 *  Uses complete elliptic integrals K(k) and E(k).
 *  Ref: Paul, "Inductance: Loop and Partial", Wiley, 2010, Eq. 5.36. */
double emi_mutual_inductance_coaxial(double r1_m, double r2_m, double distance_m);

/** Mutual capacitance between two parallel cylindrical conductors.
 *  C_m = pi*eps0*L / arccosh(d/(2a)) */
double emi_mutual_capacitance_parallel(double radius_m, double spacing_m,
                                        double length_m);

/** Near-field wave impedance |E|/|H| as function of distance.
 *  Electric dipole: Z_w = Z0 * sqrt(1+1/(kr)^2) / sqrt(1+1/(kr)^2+1/(kr)^4)
 *  Magnetic loop:   Z_w = Z0 * sqrt(1+1/(kr)^2+1/(kr)^4) / sqrt(1+1/(kr)^2)
 *  Ref: Paul Section 4.1.2.
 *  @param source_type 0=electric dipole, 1=magnetic loop */
double emi_near_field_wave_impedance(double distance_m, double freq_hz,
                                      int source_type);

/** Reactive near-field boundary: r = lambda/(2*pi) */
static inline double emi_reactive_near_field_boundary(double freq_hz) {
    double lambda = EMI_C0 / freq_hz;
    return lambda / (2.0 * EMI_PI);
}

/** Fraunhofer far-field boundary: R_ff = 2*D^2/lambda */
static inline double emi_fraunhofer_boundary(double aperture_m, double freq_hz) {
    double lambda = EMI_C0 / freq_hz;
    if (lambda <= 0.0) return 1e6;
    return 2.0 * aperture_m * aperture_m / lambda;
}

/* Probe Calibration (L5 Algorithms) */

void emi_calibrate_eprobe(double e_ref_vm,
                           const double *v_measured_arr,
                           const double *freq_hz_arr,
                           int num_points,
                           double *cal_factor_db_out);

void emi_calibrate_hprobe(double h_ref_am,
                           const double *v_measured_arr,
                           const double *freq_hz_arr,
                           int num_points,
                           double *cal_factor_db_out);

double emi_apply_eprobe_calibration(double v_measured, double cal_factor_db);

double emi_apply_hprobe_calibration(double v_measured, double cal_factor_db);

double emi_interpolate_cal_factor(double freq_hz,
                                   const double *freq_arr,
                                   const double *cal_db_arr,
                                   int num_points);

/* Impedance and VSWR Utilities (L2) */

static inline double emi_vswr_from_gamma_mag(double gamma_mag) {
    if (gamma_mag >= 1.0) return 1.0e6;
    return (1.0 + gamma_mag) / (1.0 - gamma_mag);
}

static inline double emi_gamma_mag_from_vswr(double vswr) {
    if (vswr < 1.0) return 0.0;
    return (vswr - 1.0) / (vswr + 1.0);
}

static inline double emi_return_loss_from_vswr(double vswr) {
    double gamma = emi_gamma_mag_from_vswr(vswr);
    if (gamma <= 0.0) return 100.0;
    return -20.0 * log10(gamma);
}

/** Mismatch loss: ML_dB = -10*log10(1 - |Gamma|^2) */
static inline double emi_mismatch_loss_db(double gamma_mag) {
    double g2 = gamma_mag * gamma_mag;
    if (g2 >= 1.0) return 100.0;
    return -10.0 * log10(1.0 - g2);
}

#endif /* EMI_PROBE_THEORY_H */
