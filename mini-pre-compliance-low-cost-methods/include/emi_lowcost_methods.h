/**
 * emi_lowcost_methods.h - Low-Cost Pre-Compliance Methods
 *
 * Practical low-cost alternatives to full compliance testing:
 * near-field scanning, current probe methods, TEM cell radiated testing,
 * DIY LISN construction, SDR-based spectrum analysis, reverberation
 * techniques, and correlation to full compliance results.
 *
 * References:
 *   - Wyatt, K., "Create Your Own EMC Troubleshooting Kit", Interference Technology, 2013.
 *   - Montrose, M.I., "EMC Made Simple", Montrose Compliance Services, 2010.
 *   - Smith, D.C., "High Frequency Measurements and Noise in Electronic Circuits", 1993.
 *   - Williams, T., "EMC for Product Designers", 5th ed., Newnes, 2017. Chapters 16-17.
 *
 * Knowledge Coverage:
 *   L1: Low-cost probe types, current clamp, budget antenna definitions
 *   L2: Cost-accuracy trade-offs, pre-compliance confidence levels
 *   L3: Correlation analysis between low-cost and full compliance
 *   L4: Shielding effectiveness measurement theory
 *   L5: DIY calibration methods, open-air test site validation
 *   L6: Complete low-cost pre-compliance workflow
 *   L7: Real-world applications (Arduino EMC, SMPS, IoT devices)
 */

#ifndef EMI_LOWCOST_METHODS_H
#define EMI_LOWCOST_METHODS_H

#include "emi_precompliance_core.h"
#include "emi_probe_theory.h"
#include "emi_measurement_setup.h"
#include "emi_spectrum_analysis.h"
#include <stdint.h>

/* ─── Low-Cost Probe Types (L1, L2) ──────────────────────────────────── */

/** DIY near-field probe construction parameters.
 *  Low-cost probes can be made from semi-rigid coax (RG-402)
 *  or enameled copper wire. Costs < $5 per probe vs $200-500 commercial. */
typedef struct {
    const char *probe_type;          /** "e-field_monopole", "h-field_loop", "current_clamp" */
    double loop_diameter_mm;         /** For H-field: loop diameter in mm */
    int    num_turns;                /** For H-field: number of turns */
    double wire_diameter_mm;         /** Wire diameter, mm */
    const char *connector_type;      /** "SMA", "BNC", "N-type" */
    int    has_shielding;            /** 1 if shielded (semi-rigid coax), 0 if bare wire */
    double estimated_bandwidth_hz;   /** Estimated -3dB bandwidth */
    double estimated_sensitivity_db; /** Estimated sensitivity relative to commercial */
    double material_cost_usd;        /** Approximate material cost, USD */
} emi_diy_probe_spec_t;

/** Build specification for a DIY H-field probe from semi-rigid coax.
 *  Classic design: semi-rigid coax loop (1-3 cm diameter) with a
 *  gap in the shield at the top for magnetic field sensing.
 *
 *  Ref: Wyatt, "EMC Troubleshooting Kit", Interference Technology, 2013. */
void emi_diy_hprobe_init(emi_diy_probe_spec_t *probe, double loop_diameter_mm,
                          int num_turns);

/** Build specification for a DIY E-field probe (monopole).
 *  Simple monopole probe from rigid wire, 1-5 cm long.
 *  Best for locating high-impedance (voltage-driven) emission sources. */
void emi_diy_eprobe_init(emi_diy_probe_spec_t *probe, double probe_length_mm);

/* ─── Current Probe Methods (L2, L5) ─────────────────────────────────── */

/**
 * Current probe (RF current clamp) model.
 * Used for conducted emissions measurement on cables without
 * breaking the circuit. The probe is a split-core toroidal
 * transformer that senses common-mode current on a cable.
 *
 * Transfer impedance: Z_T = V_out / I_cable  (typically 1-10 Ohm)
 * Flat response from ~100 kHz to 100+ MHz.
 *
 * Low-cost alternative: FT-240-43 ferrite core with 8-turn winding,
 * terminated in 50 Ohm. Cost < $10 vs $500-2000 for commercial probes.
 */
typedef struct {
    double transfer_impedance_ohm;   /** Transfer impedance Z_T, Ohm */
    double freq_min_hz;              /** Minimum usable frequency */
    double freq_max_hz;              /** Maximum usable frequency */
    double insertion_impedance_ohm;  /** Insertion impedance (loading on cable), Ohm */
    double saturation_current_a;     /** Saturation current, A */
    double core_mu_r;                /** Core relative permeability */
    int    num_secondary_turns;      /** Secondary winding turns */
    double core_cross_section_m2;    /** Core cross-sectional area, m^2 */
    double core_effective_length_m;  /** Core effective magnetic path length, m */
} emi_current_probe_spec_t;

/**
 * Compute current probe transfer impedance for a toroidal core.
 * Z_T = (N_sec / N_pri) * (j*omega*L_m) || R_load
 * where N_pri = 1 (single pass through core for the cable)
 * L_m = mu0 * mu_r * N_sec^2 * A_core / l_eff  (magnetizing inductance)
 *
 * At frequencies well above the -3dB corner:
 * Z_T ≈ R_load / N_sec = 50 / N_sec  Ohm (flat region)
 */
double emi_current_probe_transfer_impedance(double freq_hz,
                                             const emi_current_probe_spec_t *cp);

/**
 * Compute cable current from measured probe output.
 * I_cable = V_measured / Z_T(f)
 *
 * For common-mode current measurement:
 * I_CM = V_out / Z_T(f)
 * Convert to dBuA: I_dBuA = V_dBuV - Z_T_dBOhm
 */
double emi_current_probe_measure_current(double v_measured_v, double freq_hz,
                                          const emi_current_probe_spec_t *cp);

/** Initialize a DIY current probe spec using an FT-240-43 ferrite.
 *  8 turns secondary, 50 Ohm load: Z_T ≈ 6.25 Ohm flat.
 *  Cost ~$10, usable 100 kHz - 100 MHz. */
void emi_diy_current_probe_init(emi_current_probe_spec_t *cp);

/* ─── Low-Cost Radiated Emissions Setup (L5, L6) ─────────────────────── */

/**
 * Open-Area Test Site (OATS) validation using the three-antenna method.
 * A low-cost OATS can be set up in a parking lot, rooftop, or open field.
 * The site must be validated per CISPR 16-1-4 Annex A.
 *
 * Three-antenna method: measure S21 between three identical antennas
 * in three pairwise combinations (1-2, 1-3, 2-3) at the same positions.
 * From these three measurements, each antenna factor can be derived
 * without needing a calibrated reference antenna.
 *
 * Ref: CISPR 16-1-4, Section 7.3.
 *
 * @param s21_12_db   Measured S21 between antenna 1 and 2, dB
 * @param s21_13_db   Measured S21 between antenna 1 and 3, dB
 * @param s21_23_db   Measured S21 between antenna 2 and 3, dB
 * @param freq_hz     Frequency, Hz
 * @param distance_m  Separation distance, m
 * @param af1_out     Computed AF for antenna 1, dB(1/m)
 * @param af2_out     Computed AF for antenna 2, dB(1/m)
 * @param af3_out     Computed AF for antenna 3, dB(1/m)
 */
void emi_three_antenna_method(double s21_12_db, double s21_13_db,
                               double s21_23_db, double freq_hz,
                               double distance_m,
                               double *af1_out, double *af2_out,
                               double *af3_out);

/**
 * Low-cost GTEM cell validation using the reference radiator method.
 * A known signal source (tracking generator + small antenna) is placed
 * in the test volume. The received field is compared to the theoretical
 * field calculated from the cell geometry and input power.
 */
double emi_gtem_validation_factor(double measured_field_vm,
                                   double input_power_w,
                                   const emi_tem_cell_spec_t *tem);

/* ─── Shielding Effectiveness (L6) ───────────────────────────────────── */

/**
 * Shielding effectiveness measurement using two antennas.
 * SE_dB = 10*log10(P_received_without_shield / P_received_with_shield)
 *
 * For pre-compliance, a simple setup with a signal generator,
 * two antennas, and a spectrum analyzer can measure SE of enclosures,
 * gaskets, and cable shields up to ~1 GHz (limited by antenna size).
 *
 * Typical procedure:
 *   1. Reference: measure S21 with no shield between antennas
 *   2. Shielded: insert the shield between antennas, measure S21
 *   3. SE = Reference - Shielded
 *
 * Ref: IEEE Std 299, "Measuring the Effectiveness of EM Shielding Enclosures".
 */
double emi_shielding_effectiveness(double ref_level_dbm, double shielded_level_dbm,
                                    const emi_antenna_spec_t *tx_ant,
                                    const emi_antenna_spec_t *rx_ant,
                                    double distance_m, double freq_hz);

/**
 * Cable shielding effectiveness measurement using current probe injection.
 * Inject common-mode current on the shield and measure the coupled
 * differential-mode voltage at the far end.
 *
 * SE_cable = 20*log10(I_injected / I_coupled)
 *
 * Ref: IEC 62153-4-3, "Metallic Communication Cable Test Methods - Transfer Impedance".
 */
double emi_cable_shielding_effectiveness(double injected_current_ma,
                                          double coupled_voltage_uv,
                                          double freq_hz,
                                          double cable_length_m);

/* ─── Correlation to Full Compliance (L3, L5) ────────────────────────── */

/**
 * Correlation factor between pre-compliance and full compliance.
 * For each frequency, compute:
 *   Corr(f) = E_full(f) - E_pre(f)
 *
 * A systematic offset (e.g., +3 dB for pre-compliance) can be used
 * as a safety margin. Random variations represent measurement uncertainty.
 *
 * A good pre-compliance setup has:
 *   - Systematic offset < 3 dB
 *   - Standard deviation < 4 dB
 *   - Correlation coefficient r > 0.85
 */
typedef struct {
    int     num_points;
    double *freq_hz;
    double *precompliance_dbuvm;
    double *fullcompliance_dbuvm;
    double *correlation_factor_db;   /** E_full - E_pre at each frequency */
    double  mean_offset_db;           /** Mean systematic offset */
    double  std_deviation_db;         /** Standard deviation of offset */
    double  pearson_r;               /** Pearson correlation coefficient */
    double  max_deviation_db;        /** Maximum absolute deviation */
    int     num_comparable_points;   /** Points with valid data in both sets */
} emi_correlation_result_t;

/**
 * Compute correlation between pre-compliance and full compliance datasets.
 *
 * Pearson correlation: r = Cov(X,Y) / (sigma_X * sigma_Y)
 * where X = pre-compliance, Y = full compliance.
 *
 * A value of r > 0.9 indicates strong linear relationship;
 * pre-compliance can reliably predict full compliance with a
 * known offset and uncertainty margin.
 */
int emi_compute_correlation(const double *pre_dbuvm, const double *full_dbuvm,
                             int num_points,
                             emi_correlation_result_t *result);

/**
 * Estimate the recommended pre-compliance limit margin.
 * Recommended_margin = |mean_offset| + k * std_deviation
 * where k = 2 for 95% confidence, k = 1 for 68% confidence.
 *
 * Example: if mean_offset = 2 dB, std_dev = 3 dB, k = 2
 *   Recommended margin = 2 + 6 = 8 dB below the full compliance limit.
 */
double emi_recommend_precompliance_margin(const emi_correlation_result_t *corr,
                                           double k_factor);

#endif /* EMI_LOWCOST_METHODS_H */
