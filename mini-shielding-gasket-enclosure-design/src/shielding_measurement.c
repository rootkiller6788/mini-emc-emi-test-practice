/* shielding_measurement.c -- Shielding Effectiveness Measurement Methods
 * L5: Algorithms - MIL-STD-285, IEEE-299 test calculations
 * L7: Applications - Dynamic range estimation, SE test setup design
 *
 * Reference: MIL-STD-285 (1956, withdrawn 1997)
 *            IEEE Std 299-2006 "Measuring the Effectiveness of EM Shielding Enclosures"
 *            IEC 61000-4-21 Reverberation chamber methods
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shielding_gasket.h"

/*
 * L7: Transfer Impedance per MIL-STD-285
 *
 * MIL-STD-285 used a reference antenna setup to measure SE.
 * The transfer impedance method relates the measured SE to
 * shield material properties:
 *
 *   SE_measured = 20*log10(V_open / V_shielded)
 *   where V_open  = receiver voltage without shield
 *         V_shielded = receiver voltage with shield in place
 *
 * For a sample of size L x W [m], the transfer impedance is:
 *   Z_T = Z0 * (L * W) * 10^(-SE/20) / d^2  [ohm*m]
 *   where d = antenna separation distance [m]
 */
double shielding_transfer_impedance_mil_std_285(double freq_hz,
    double sample_size_m, double measured_se_db) {
    if (freq_hz <= 0.0 || sample_size_m <= 0.0 || measured_se_db < 0.0)
        return -1.0;

    /* Reference antenna separation per MIL-STD-285: 1m */
    double d_ref = 1.0;

    /* Compute Z_T from measured SE */
    double attenuation_linear = pow(10.0, -measured_se_db / 20.0);
    double Z_T = Z0_FREE_SPACE * (sample_size_m * sample_size_m)
               * attenuation_linear / (d_ref * d_ref);
    return Z_T;
}

/*
 * L7: Dynamic Range Estimation for SE Testing
 *
 * SE testing requires adequate dynamic range:
 *   DR = P_tx + G_tx + G_rx - P_noise - L_cable [dB]
 *
 * where P_tx = transmit power [dBm]
 *       G_tx = transmit antenna gain [dBi]
 *       G_rx = receive antenna gain [dBi]
 *       P_noise = receiver noise floor [dBm]
 *       L_cable = cable losses [dB]
 *
 * Minimum measurable SE is limited by dynamic range.
 * Maximum measurable SE is limited by system leakage.
 *
 * Reference: IEEE-299 Sec.5.2
 */
double shielding_dynamic_range_estimate(double noise_floor_dbm,
    double max_power_dbm, double antenna_gain_db) {
    /* Cable loss estimate: ~2 dB at 1 GHz for typical setup */
    double cable_loss_db = 2.0;

    /* System gain = 2 antennas + cable losses */
    double system_gain_db = 2.0 * antenna_gain_db - cable_loss_db;

    /* Dynamic range */
    double DR = max_power_dbm + system_gain_db - noise_floor_dbm;

    /* Practical limit: 80-100 dB for open-site, 120 dB for shielded room */
    if (DR > 140.0) DR = 140.0; /* absolute maximum with current technology */
    if (DR < 20.0) DR = 20.0;   /* minimum usable */

    return DR;
}

#include "shielding_measurement.h"

double shielding_measurement_uncertainty(const measurement_setup_t *setup, double freq_hz) {
    if (!setup || freq_hz <= 0.0) return -1.0;
    double u_ant = 0.5;
    double u_cable = 0.3;
    double u_site = 1.0;
    double u_rx = 0.2;
    double u_align = 0.3;
    double u_combined = sqrt(u_ant*u_ant + u_cable*u_cable + u_site*u_site
                           + u_rx*u_rx + u_align*u_align);
    return u_combined;
}

double shielding_minimum_measurable_se(const measurement_setup_t *setup) {
    if (!setup) return -1.0;
    double system_gain = setup->tx_antenna_gain_dbi + setup->rx_antenna_gain_dbi
                       - setup->cable_loss_db + setup->preamp_gain_db;
    double noise_floor = setup->rx_noise_floor_dbm;
    double max_signal = setup->tx_power_dbm;
    double margin = 6.0;
    return max_signal + system_gain - noise_floor + margin;
}

double shielding_maximum_measurable_se(const measurement_setup_t *setup, double freq_hz) {
    if (!setup || freq_hz <= 0.0) return -1.0;
    double base_dr = setup->dynamic_range_db;
    if (base_dr <= 0.0) base_dr = 120.0;
    double leakage_degradation = 0.0;
    if (freq_hz > 1.0e9) leakage_degradation = 20.0 * log10(freq_hz / 1.0e9);
    double max_se = base_dr - leakage_degradation;
    if (max_se < 40.0) max_se = 40.0;
    if (max_se > 140.0) max_se = 140.0;
    return max_se;
}

int shielding_is_se_measurable(const measurement_setup_t *setup,
    double target_se_db, double freq_hz, double confidence_level) {
    if (!setup) return 0;
    double se_min = shielding_minimum_measurable_se(setup);
    double se_max = shielding_maximum_measurable_se(setup, freq_hz);
    double uncertainty = shielding_measurement_uncertainty(setup, freq_hz);
    double expanded_uncertainty = uncertainty * (confidence_level / 0.68);
    double se_with_guard = target_se_db + expanded_uncertainty;
    return (se_with_guard >= se_min && se_with_guard <= se_max) ? 1 : 0;
}

void measurement_setup_ieee_299_default(measurement_setup_t *setup) {
    if (!setup) return;
    memset(setup, 0, sizeof(*setup));
    setup->standard = STANDARD_IEEE_299;
    setup->method = TEST_METHOD_SHIELDED_ROOM;
    setup->tx_antenna_gain_dbi = 3.0;
    setup->rx_antenna_gain_dbi = 3.0;
    setup->antenna_separation_m = 1.0;
    setup->tx_power_dbm = 0.0;
    setup->rx_noise_floor_dbm = -100.0;
    setup->cable_loss_db = 2.0;
    setup->preamp_gain_db = 20.0;
    setup->dynamic_range_db = 100.0;
    setup->calibration_offset_db = 0.0;
}
