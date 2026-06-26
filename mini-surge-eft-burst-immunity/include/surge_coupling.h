/**
 * surge_coupling.h - Surge Coupling/Decoupling Network Design
 *
 * L6 Canonical Problems: Coupling network design per IEC 61000-4-5,
 * decoupling network for auxiliary equipment, multi-stage
 * protection coordination.
 *
 * The coupling network injects the surge into the EUT while the
 * decoupling network protects the auxiliary equipment and power
 * source from the surge.
 *
 * Reference:
 *   IEC 61000-4-5 Annex A: Coupling/Decoupling Network Design
 *   IEC 61000-4-4 Annex A: EFT Coupling Clamp Design
 *   Montrose & Nakauchi, EMC and the Printed Circuit Board, 2004
 */

#ifndef SURGE_COUPLING_H
#define SURGE_COUPLING_H

#include "surge_defs.h"
#include "surge_protection.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================================================================
 * L6: Coupling Network Design
 * ================================================================== */

/**
 * Design a surge coupling network for IEC 61000-4-5 compliance.
 *
 * Line-to-line coupling (differential mode):
 *   - 18 uF capacitor in series with each line
 *   - Surge applied across the capacitors
 *   - 10 ohm series resistor for current limiting
 *
 * Line-to-ground coupling (common mode):
 *   - 9 uF capacitor + 10 ohm in series from line to ground
 *   - Provides 12 ohm effective impedance
 *
 * The coupling capacitor must:
 * 1. Block DC/low-frequency from back-feeding into surge generator
 * 2. Pass the surge waveform without significant distortion
 * 3. Withstand the full surge voltage
 *
 * Complexity: O(1)
 * Reference: IEC 61000-4-5, Section 6.2
 */
typedef struct {
    double coupling_cap_line_uf;       /* Line coupling capacitance (uF) */
    double coupling_cap_ground_uf;     /* Ground coupling capacitance (uF) */
    double series_resistance_ohm;      /* Series resistance (ohm) */
    double decoupling_inductance_uh;   /* Decoupling inductance (uH) */
    double residual_coupling_impedance; /* Residual impedance @ surge spectrum */
    surge_coupling_mode_t mode;        /* Coupling mode */
} coupling_network_params_t;

/**
 * Initialize coupling network for standard IEC 61000-4-5 configuration.
 *
 * Default values per standard:
 * - Line-to-line: C_c = 18 uF, R_s = 10 ohm
 * - Line-to-ground: C_c = 9 uF + 10 ohm per line
 *
 * Complexity: O(1)
 */
int coupling_network_init(coupling_network_params_t *cn,
                          surge_coupling_mode_t mode);

/**
 * Compute the coupling network transfer function magnitude at a
 * given frequency.
 *
 *   |H_c(f)| = |Z_load / (Z_load + Z_coupling + Z_source)|
 *
 * This determines how much of the surge voltage reaches the EUT.
 * Ideally |H_c| ~ 1 across the surge spectrum.
 *
 * Complexity: O(1)
 */
double coupling_transfer_magnitude(double f,
                                    const coupling_network_params_t *cn,
                                    double z_load);

/**
 * Compute the -3dB bandwidth of the coupling network.
 *
 * The coupling network acts as a high-pass filter (series capacitor
 * blocks DC). The corner frequency should be well below the surge
 * spectrum to avoid waveform distortion.
 *
 * For standard IEC coupling: fc = 1/(2*pi*R*C) ~ 0.6 Hz
 * This is << surge spectrum (~kHz-MHz), ensuring faithful coupling.
 *
 * Complexity: O(1)
 */
double coupling_bandwidth_hz(const coupling_network_params_t *cn,
                              double z_load);

/* ==================================================================
 * L6: Decoupling Network Design
 * ================================================================== */

/**
 * Design decoupling inductor for auxiliary equipment protection.
 *
 * The decoupling network prevents surge energy from propagating
 * to auxiliary equipment and the mains supply.
 *
 *   L_decouple = Z_target / (2 * pi * f_surge)
 *
 * where Z_target is the desired impedance at the surge frequency,
 * typically 10-100x the source impedance.
 *
 * For 1.2/50us surge (f_corner ~ 10 kHz):
 *   L_decouple = 100 / (2*pi*10e3) ~ 1.6 mH
 *
 * Standard IEC values: 1.5 mH per line
 *
 * Complexity: O(1)
 */
double decoupling_inductance(double z_source, double f_corner,
                              double impedance_ratio);

/**
 * Check if a given decoupling network provides adequate isolation.
 *
 * The attenuation provided by the decoupling network:
 *   A_dB = 20 * log10(1 + Z_decouple / Z_source)
 *
 * For effective decoupling: A_dB > 20 dB (factor of 10 reduction)
 * For high-reliability: A_dB > 40 dB (factor of 100 reduction)
 *
 * Complexity: O(1)
 */
double decoupling_attenuation_db(double f, double l_decouple_uh,
                                  double z_source);

/**
 * Verify coupling/decoupling network does not distort the surge
 * waveform by more than the allowed tolerance.
 *
 * IEC tolerance for surge waveform:
 * - Front time: +/-30%
 * - Duration: +/-20%
 * - Peak: +/-10%
 *
 * This function checks the waveform distortion through the network.
 *
 * Complexity: O(n_points)
 */
int coupling_verify_waveform(const coupling_network_params_t *cn,
                             const surge_waveform_params_t *wf,
                             double *peak_error_pct,
                             double *rise_error_pct,
                             double *width_error_pct);

/* ==================================================================
 * L6: Multi-Stage Protection Coordination
 * ================================================================== */

/**
 * Design a multi-stage surge protection scheme.
 *
 * Coordination principle:
 * Each stage must handle its share of the surge energy while
 * clamping to a level that the next stage can survive.
 *
 * Stage ordering (port to load):
 *   1. GDT or SGAP (primary): Handles bulk energy, high I_peak
 *   2. MOV (secondary): Handles residual, faster response
 *   3. TVS (tertiary): Final precision clamping
 *
 * Decoupling between stages is essential:
 *   Z_decouple = (V_clamp_prev - V_breakdown_next) / I_residual
 *
 * Without decoupling, the fastest device (TVS) takes all the
 * energy and may fail, defeating the cascaded protection.
 *
 * Complexity: O(n_stages^2)
 * Reference: Standler, Protection of Electronic Circuits, 1989
 */
int multistage_protection_design(multistage_protection_t *mp,
                                  const surge_waveform_params_t *wf,
                                  double v_line_max, double v_withstand);

/**
 * Compute energy distribution across protection stages.
 *
 * Energy division depends on the V-I characteristics and
 * decoupling impedances. The first stage triggers at highest
 * voltage, absorbing the bulk energy. Residual energy passes
 * through decoupling to subsequent stages.
 *
 * Complexity: O(n_stages * n_iterations) for iterative solution
 */
int multistage_energy_distribution(multistage_protection_t *mp,
                                    const surge_waveform_params_t *wf);

/**
 * Verify stage coordination: ensure each stage triggers in order
 * and no stage is overstressed.
 *
 * Coordination check:
 *   1. V_clamp(stage_i) < V_breakdown(stage_{i+1}) (voltage handoff)
 *   2. I_peak(stage_i) < I_rated(stage_i) (no overstress)
 *   3. E_absorbed(stage_i) < E_rating(stage_i) (energy margin)
 *   4. V_residual_at_load < V_withstand (protection goal)
 *
 * Returns SURGE_OK if coordination is valid.
 * Returns SURGE_ERR_COORDINATION_FAIL if any check fails.
 *
 * Complexity: O(n_stages)
 */
int multistage_verify_coordination(const multistage_protection_t *mp,
                                    double v_withstand);

/**
 * Compute the residual (let-through) voltage at the protected load
 * after multi-stage protection.
 *
 * This is the ultimate figure of merit for the protection scheme.
 *
 *   V_residual = V_clamp(last_stage) + I_residual * Z_wiring
 *
 * Complexity: O(1)
 */
double multistage_residual_voltage(const multistage_protection_t *mp,
                                    double i_residual);

#ifdef __cplusplus
}
#endif

#endif /* SURGE_COUPLING_H */