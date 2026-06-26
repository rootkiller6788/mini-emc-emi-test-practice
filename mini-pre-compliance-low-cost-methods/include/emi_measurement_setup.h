/**
 * emi_measurement_setup.h - Measurement Setup Models for Low-Cost Pre-Compliance
 *
 * Models for LISN (Line Impedance Stabilization Network), TEM/GTEM cells,
 * antenna setups, ground plane effects, and measurement distance configurations.
 *
 * References:
 *   - CISPR 16-1-2: Specification for radio disturbance measuring apparatus - LISN
 *   - CISPR 16-2-3: Measurement of radiated disturbances
 *   - Crawford, M.L., "Generation of Standard EM Fields Using TEM Cells", IEEE Trans. EMC, 1974.
 *   - Paul, C.R., "Introduction to EMC", Wiley, 2006. Chapters 3, 8.
 *
 * Knowledge Coverage:
 *   L1: LISN, TEM cell, OATS, SAC, FAR definitions
 *   L2: Ground plane requirements, measurement distance selection
 *   L3: Impedance networks, voltage division, site attenuation
 *   L4: Transmission line theory for TEM cells, Friis formula for site attenuation
 *   L5: Site attenuation measurement and normalization
 */

#ifndef EMI_MEASUREMENT_SETUP_H
#define EMI_MEASUREMENT_SETUP_H

#include "emi_precompliance_core.h"

/* ─── LISN Model (L1, L2) ────────────────────────────────────────────── */

/** LISN (AMN - Artificial Mains Network) specification.
 *  Standard LISN: 50uH || 50 Ohm per CISPR 16-1-2.
 *  Provides defined RF impedance (50 Ohm) on each line to ground
 *  while passing mains power (AC or DC) to the EUT. */
typedef struct {
    double inductance_h;             /** Series inductance per line, H (typ. 50e-6) */
    double cap_line_to_ground_f;     /** Line-to-ground capacitance, F (typ. 0.1e-6) */
    double cap_coupling_f;           /** Coupling capacitor to measurement port, F */
    double series_resistance_ohm;    /** Series resistance, Ohm */
    double measurement_port_z_ohm;   /** Measurement port impedance, Ohm (typ. 50) */
    double insertion_loss_db;        /** Insertion loss in measurement path, dB */
    double isolation_db;             /** Isolation between lines, dB */
    double max_current_a;            /** Maximum rated current, A */
    double max_voltage_v;            /** Maximum rated voltage, V */
    const char *type;                /** "V-LISN", "delta-LISN", "CISPR 16-1-2" */
} emi_lisn_spec_t;

/** LISN impedance vs frequency.
 *  Z(f) at EUT port (looking into LISN):
 *    Z(f) = R || (jωL + 1/(jωC_line_to_gnd))
 *
 *  At high frequencies: Z -> R = 50 Ohm (measurement port impedance)
 *  At low frequencies: Z -> power line impedance (typ. < 5 Ohm)
 *
 *  Transition frequency: f_t = R/(2*pi*L) ≈ 159 kHz for 50uH/50Ohm. */
typedef struct {
    int     num_freqs;
    double *freq_hz;
    double *impedance_mag_ohm;
    double *impedance_phase_rad;
} emi_lisn_impedance_t;

/**
 * Compute LISN impedance magnitude at a given frequency.
 * Z(f) = |R + jωL + 1/(jωC)| for the measurement path.
 *
 * For standard V-LISN: L = 50 uH, R_meas = 50 Ohm, C_coup = 0.1 uF.
 * For a 2-line LISN the impedance between either line and ground is:
 *   Z = (R || Z_L+C) where Z_L+C = jωL + 1/(jωCline) + R_series
 */
double emi_lisn_impedance_mag(double freq_hz, const emi_lisn_spec_t *lisn);

/** Compute the LISN voltage division factor.
 *  V_meas / V_noise = Z_meas / (Z_meas + Z_source)
 *  where Z_meas = 50 Ohm and Z_source depends on LISN + EUT impedance.
 *  Returns loss in dB (negative value). */
double emi_lisn_voltage_division_factor_db(double freq_hz,
                                            const emi_lisn_spec_t *lisn,
                                            double eut_source_impedance_ohm);

/** Create a standard CISPR 16-1-2 50uH/50Ohm V-LISN specification. */
void emi_lisn_init_cispr16(emi_lisn_spec_t *lisn,
                            double max_current_a, double max_voltage_v);

/** Create a low-cost DIY LISN specification (simplified).
 *  Uses a 50uH inductor and 50 Ohm termination with 0.1uF coupling cap.
 *  Limited to ~3A for low-cost designs using readily available components. */
void emi_lisn_init_lowcost(emi_lisn_spec_t *lisn);

/* ─── TEM / GTEM Cell (L1, L2) ──────────────────────────────────────── */

/** TEM (Transverse Electromagnetic) cell specification.
 *  A TEM cell is a 50-Ohm rectangular coaxial transmission line
 *  tapered at both ends. The EUT is placed between the septum
 *  (inner conductor) and the outer wall.
 *
 *  GTEM (Gigahertz TEM) uses a single-port tapered design with
 *  distributed load for operation up to 20 GHz.
 *
 *  Ref: Crawford, IEEE Trans. EMC-16, 1974; IEC 61000-4-20. */
typedef struct {
    double cell_length_m;            /** Total cell length, m */
    double cell_width_m;             /** Cell width (outer), m */
    double cell_height_m;            /** Cell height (outer), m */
    double septum_height_m;          /** Septum height above floor, m */
    double test_volume_width_m;      /** Usable test volume width, m */
    double test_volume_height_m;     /** Usable test volume height, m */
    double test_volume_depth_m;      /** Usable test volume depth, m */
    double characteristic_impedance_ohm; /** Nominal Z0 = 50 Ohm */
    double max_freq_hz;              /** Maximum usable frequency, Hz */
    double vswr_max;                 /** Maximum VSWR over bandwidth */
    double field_factor_db;          /** Field factor: E(dBV/m) - P_in(dBW) */
    double shield_effectiveness_db;  /** Shielding effectiveness, dB */
} emi_tem_cell_spec_t;

/**
 * Compute TEM cell field strength for a given input power.
 * E = sqrt(P_in * Z0) / d   where d = septum height
 * E_dBVm = P_in_dBW + 10*log10(Z0) - 20*log10(d)
 *
 * The uniform field region exists between septum and floor
 * and extends approximately 1/3 of septum width on each side
 * of the centerline, and 1/3 of the septum-floor spacing in height.
 *
 * @param p_in_dbm  Input power, dBm
 * @param tem       TEM cell specification
 * @return Field strength, V/m
 */
double emi_tem_cell_field_strength(double p_in_dbm, const emi_tem_cell_spec_t *tem);

/**
 * Compute required input power for a desired field strength.
 * P_in_dBm = 20*log10(E * d) - 10*log10(Z0) + 30
 */
double emi_tem_cell_input_power(double desired_e_vm, const emi_tem_cell_spec_t *tem);

/**
 * Compute TEM cell maximum frequency.
 * f_max = c / (2 * a) where a = larger transverse dimension.
 * Above f_max, higher-order modes (TE10) can propagate.
 */
static inline double emi_tem_cell_max_frequency(const emi_tem_cell_spec_t *tem) {
    double a = (tem->cell_width_m > tem->cell_height_m)
                ? tem->cell_width_m : tem->cell_height_m;
    if (a <= 0.0) return 1e9;
    return EMI_C0 / (2.0 * a);
}

/* ─── Antenna Setup Models (L1, L2) ──────────────────────────────────── */

/** Measurement antenna specification for radiated emissions.
 *  Includes antenna factor, gain, and polarization info. */
typedef struct {
    const char *name;                /** Antenna model/name */
    double freq_min_hz;              /** Minimum usable frequency */
    double freq_max_hz;              /** Maximum usable frequency */
    int    num_af_points;            /** Number of antenna factor calibration points */
    double *af_freq_hz;              /** Calibration frequencies */
    double *af_db;                   /** Antenna factor at each frequency */
    double gain_dbi_nominal;         /** Nominal gain, dBi */
    double vswr_max;                 /** Maximum VSWR */
    int    is_broadband;             /** 1=broadband (biconical, LPDA), 0=narrowband */
    const char *polarization;        /** "horizontal", "vertical", "dual" */
} emi_antenna_spec_t;

/**
 * Standard antenna types with representative characteristics.
 * Creates pre-filled antenna specifications for common EMC antennas.
 */

/** Biconical antenna: 30-300 MHz, nominal gain 0-2 dBi */
void emi_antenna_init_biconical(emi_antenna_spec_t *ant);

/** Log-periodic dipole array (LPDA): 200-2000 MHz, nominal gain 5-7 dBi */
void emi_antenna_init_lpda(emi_antenna_spec_t *ant);

/** Broadband horn (DRG): 1-18 GHz, nominal gain 8-15 dBi */
void emi_antenna_init_double_ridge_horn(emi_antenna_spec_t *ant);

/** Active monopole (rod) antenna: 9 kHz - 30 MHz, capacitive probe */
void emi_antenna_init_active_monopole(emi_antenna_spec_t *ant);

/**
 * Interpolate antenna factor from calibration table.
 * Linear interpolation in log-frequency, dB domain.
 */
double emi_antenna_factor_interpolate(const emi_antenna_spec_t *ant,
                                       double freq_hz);

/* ─── Site Attenuation (L4, L5) ──────────────────────────────────────── */

/**
 * Normalized Site Attenuation (NSA) for an ideal site.
 * NSA = AF1 + AF2 - 20*log10(f_MHz) + 48.92 - 20*log10(1/d1 + 1/d2)
 *      + 20*log10((lambda/2pi) * (1/d1^2 + 1/d2^2 - ...))
 *
 * This is the theoretical attenuation between two tuned dipoles
 * over a perfectly conducting ground plane. Used to verify OATS/SAC.
 *
 * Ref: CISPR 16-1-4, Annex A; ANSI C63.4.
 *
 * @param freq_hz        Frequency, Hz
 * @param distance_m     Horizontal separation, m (typ. 3 or 10)
 * @param h_tx_m         Transmit antenna height, m
 * @param h_rx_m         Receive antenna height, m
 * @return NSA in dB
 */
double emi_normalized_site_attenuation(double freq_hz, double distance_m,
                                        double h_tx_m, double h_rx_m);

/**
 * Compute site attenuation measurement uncertainty.
 * Based on the difference between measured and theoretical NSA.
 * CISPR 16-1-4 allows +/-4 dB deviation.
 */
double emi_site_attenuation_deviation_db(double measured_nsa_db,
                                          double theoretical_nsa_db);

/* ─── Measurement Distance and Geometry (L2) ─────────────────────────── */

/** Compute corrected field strength for non-standard measurement distance.
 *  For far-field: E_std = E_meas + 20*log10(d_meas / d_std)
 *  (20 dB/decade extrapolation for far-field only)
 *
 *  For near-field measurements this formula does NOT apply;
 *  near-field extrapolation requires knowledge of the source
 *  geometry and wave impedance.
 *
 *  Ref: CISPR 16-2-3, Annex D. */
double emi_distance_correction_db(double e_meas_dbuvm, double d_meas_m,
                                   double d_std_m);

/** Compute the maximum EUT dimension for far-field assumption.
 *  D_max = min(width, height, depth_of_test_volume)
 *  Combined with emi_fraunhofer_boundary to check if measurement
 *  distance satisfies far-field conditions. */
static inline double emi_eut_max_dimension(double width_m, double height_m,
                                            double depth_m) {
    double d1 = width_m;
    double d2 = (height_m > d1) ? height_m : d1;
    return (depth_m > d2) ? depth_m : d2;
}

#endif /* EMI_MEASUREMENT_SETUP_H */
