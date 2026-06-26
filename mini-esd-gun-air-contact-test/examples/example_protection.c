/**
 * @file example_protection.c
 * @brief Example: ESD protection device selection for a USB 3.0 port
 *
 * L6 Canonical Problem: Demonstrates systematic TVS selection for
 * protecting a high-speed digital interface (USB 3.0, 5 Gbps)
 * against IEC 61000-4-2 Level 4 ESD strikes.
 *
 * L7 Application: Real-world protection design for consumer electronics.
 */

#include "esd_protection.h"
#include "esd_waveform.h"
#include "esd_compliance.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

int main(void)
{
    printf("╔═══════════════════════════════════════════════╗\n");
    printf("║  ESD Protection Design Example               ║\n");
    printf("║  USB 3.0 Port Protection (5 Gbps)            ║\n");
    printf("╚═══════════════════════════════════════════════╝\n\n");

    /* ─── Define the protection window ────────────────────── */
    printf("═══ Protection Window Definition ═══\n");

    esd_protection_window_t window;
    window.v_signal_max_v = 1.0;       /* USB 3.0 differential signal ~1V */
    window.v_ic_max_v = 4.6;           /* Typical USB PHY abs max */
    window.v_supply_v = 5.0;           /* VBUS */
    window.esd_peak_current_a = 30.0;  /* IEC Level 4: 30 A peak */
    window.signal_bandwidth_mhz = 2500.0; /* USB 3.0: 5 Gbps, BW ~2.5 GHz */

    printf("  Signal max voltage:      %.1f V\n", window.v_signal_max_v);
    printf("  IC abs max voltage:      %.1f V\n", window.v_ic_max_v);
    printf("  ESD peak current:        %.1f A (IEC Level 4)\n",
           window.esd_peak_current_a);
    printf("  Signal bandwidth:        %.0f MHz (5 Gbps)\n",
           window.signal_bandwidth_mhz);
    printf("\n");

    /* ─── Create candidate TVS devices ────────────────────── */
    printf("═══ TVS Candidate Evaluation ═══\n");

    /* Candidate A: Low-capacitance 3.3V TVS */
    esd_tvs_params_t tvs_a;
    memset(&tvs_a, 0, sizeof(tvs_a));
    tvs_a.vrwm_v = 3.3;
    tvs_a.vbr_min_v = 4.5;
    tvs_a.vbr_max_v = 5.5;
    tvs_a.vc_v = 8.0;
    tvs_a.ipp_a = 4.0;
    tvs_a.cj_pf = 0.3;
    tvs_a.dynamic_r_ohm = 0.8;
    tvs_a.subtype = ESD_PROT_TVS_BIDIR;

    /* Candidate B: Ultra-low capacitance 5V TVS */
    esd_tvs_params_t tvs_b;
    memset(&tvs_b, 0, sizeof(tvs_b));
    tvs_b.vrwm_v = 5.0;
    tvs_b.vbr_min_v = 6.0;
    tvs_b.vbr_max_v = 7.5;
    tvs_b.vc_v = 12.0;
    tvs_b.ipp_a = 5.0;
    tvs_b.cj_pf = 0.15;
    tvs_b.dynamic_r_ohm = 0.5;
    tvs_b.subtype = ESD_PROT_TVS_UNIDIR;

    /* Candidate C: Higher-power 5V TVS */
    esd_tvs_params_t tvs_c;
    memset(&tvs_c, 0, sizeof(tvs_c));
    tvs_c.vrwm_v = 5.0;
    tvs_c.vbr_min_v = 6.2;
    tvs_c.vbr_max_v = 7.8;
    tvs_c.vc_v = 9.5;
    tvs_c.ipp_a = 8.0;
    tvs_c.cj_pf = 1.2;
    tvs_c.dynamic_r_ohm = 0.3;
    tvs_c.subtype = ESD_PROT_TVS_UNIDIR;

    esd_tvs_selection_t sel_a, sel_b, sel_c;
    esd_tvs_select(&window, &tvs_a, &sel_a);
    esd_tvs_select(&window, &tvs_b, &sel_b);
    esd_tvs_select(&window, &tvs_c, &sel_c);

    printf("  TVS-A (3.3V, 0.3pF):\n");
    printf("    VRWM margin:    %+.1f V  %s\n", sel_a.vrwm_margin_v,
           sel_a.suitable ? "OK" : "FAIL");
    printf("    Clamp margin:   %+.1f V\n", sel_a.vc_margin_v);
    printf("    BW margin:      %+.0f MHz\n", sel_a.bandwidth_margin_mhz);
    printf("    Suitable:       %s\n", sel_a.suitable ? "YES ✓" : "NO ✗");

    printf("  TVS-B (5V, 0.15pF):\n");
    printf("    VRWM margin:    %+.1f V  %s\n", sel_b.vrwm_margin_v,
           sel_b.suitable ? "OK" : "FAIL");
    printf("    Clamp margin:   %+.1f V\n", sel_b.vc_margin_v);
    printf("    BW margin:      %+.0f MHz\n", sel_b.bandwidth_margin_mhz);
    printf("    Suitable:       %s\n", sel_b.suitable ? "YES ✓" : "NO ✗");

    printf("  TVS-C (5V, 1.2pF):\n");
    printf("    VRWM margin:    %+.1f V  %s\n", sel_c.vrwm_margin_v,
           sel_c.suitable ? "OK" : "FAIL");
    printf("    Clamp margin:   %+.1f V\n", sel_c.vc_margin_v);
    printf("    BW margin:      %+.0f MHz\n", sel_c.bandwidth_margin_mhz);
    printf("    Suitable:       %s\n", sel_c.suitable ? "YES ✓" : "NO ✗");

    /* Compare options */
    int best_ab = esd_tvs_compare(&window, &tvs_a, &tvs_b);
    printf("\n  TVS-A vs TVS-B: %s is better\n",
           best_ab > 0 ? "A" : (best_ab < 0 ? "B" : "Equal"));

    printf("\n");

    /* ─── Bandwidth analysis ──────────────────────────────── */
    printf("═══ Bandwidth Impact Analysis ═══\n");

    double bw_limit_a = esd_tvs_bandwidth_limit(&tvs_a, 90.0, 0);
    double bw_limit_b = esd_tvs_bandwidth_limit(&tvs_b, 90.0, 0);
    double bw_limit_c = esd_tvs_bandwidth_limit(&tvs_c, 90.0, 0);

    printf("  USB 3.0 differential impedance: 90 Ω\n");
    printf("  TVS-A (Cj=0.3 pF): -3dB @ %.0f MHz\n", bw_limit_a);
    printf("  TVS-B (Cj=0.15pF): -3dB @ %.0f MHz\n", bw_limit_b);
    printf("  TVS-C (Cj=1.2 pF): -3dB @ %.0f MHz\n", bw_limit_c);

    double bw_dig_a = esd_tvs_bandwidth_limit(&tvs_a, 90.0, 1);
    double bw_dig_b = esd_tvs_bandwidth_limit(&tvs_b, 90.0, 1);
    printf("  Digital bandwidth (0.35/τ):\n");
    printf("    TVS-A: %.0f Mbps, TVS-B: %.0f Mbps\n",
           bw_dig_a * 1000.0, bw_dig_b * 1000.0);
    printf("    USB 3.0 requires ~2500 MHz analog BW\n");

    printf("\n");

    /* ─── Multi-stage protection network ──────────────────── */
    printf("═══ Multi-Stage Protection Network ═══\n");

    esd_protection_network_t net;
    memset(&net, 0, sizeof(net));
    net.primary_type = ESD_PROT_TVS_UNIDIR;
    net.tvs_primary = tvs_b;        /* Off-chip TVS at connector */
    net.series_resistance_ohm = 5.0; /* Series ferrite/resistor */
    net.series_inductance_nh = 2.0;  /* PCB trace inductance */
    net.secondary_type = ESD_PROT_TVS_UNIDIR;
    net.tvs_secondary = tvs_a;      /* On-chip / near-chip TVS */

    double v_at_ic, e_primary, e_secondary;
    esd_network_simulate(&net, 30.0, &v_at_ic, &e_primary, &e_secondary);

    printf("  Primary TVS:   5V / 0.15 pF / Ipp=5 A\n");
    printf("  Series Z:      5 Ω + 2 nH\n");
    printf("  Secondary TVS: 3.3V / 0.3 pF\n");
    printf("  At IEC Level 4 (30 A peak):\n");
    printf("    Voltage at IC:     %.1f V\n", v_at_ic);
    printf("    Energy primary:    %.1f µJ\n", e_primary * 1e6);
    printf("    Energy secondary:  %.3f µJ\n", e_secondary * 1e6);

    int safe_at_ic = (v_at_ic < window.v_ic_max_v) ? 1 : 0;
    printf("    IC safe?            %s\n", safe_at_ic ? "YES ✓" : "NO ✗");

    printf("\n");

    /* ─── PCB layout recommendation ──────────────────────── */
    printf("═══ PCB Layout Recommendation ═══\n");

    double trace_width = esd_pcb_trace_width(30.0, 1.0, 0.5, 20.0);
    printf("  For 30A ESD current, 1 oz Cu, 20mm trace:\n");
    printf("    Minimum trace width: %.1f mm\n", trace_width);
    printf("  Recommendation:\n");
    printf("    - Place TVS within 3mm of connector\n");
    printf("    - Use ≥ %.0f mil wide traces\n", trace_width * 39.37);
    printf("    - Multiple vias to ground plane\n");
    printf("    - Keep unprotected traces away from ESD path\n");

    printf("\n═══ Example Complete ═══\n");
    return 0;
}
