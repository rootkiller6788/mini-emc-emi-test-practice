/* shielding_enclosure.c -- Enclosure Cavity Resonance and Aperture Analysis
 *
 * L4: Fundamental Laws - Waveguide cutoff, Bethe aperture theory
 * L5: Algorithms - Cavity mode computation, aperture SE degradation
 * L6: Canonical Problems - Rectangular enclosure with apertures
 *
 * Reference: Bethe (1944) "Theory of Diffraction by Small Holes"
 *            Mendez (1974) "Shielding Theory of Enclosures with Apertures"
 *            Ott (2009) "EMC Engineering" Ch.10
 *            Balanis "Advanced Engineering EM" Ch.8
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shielding_gasket.h"

/* ===================================================================
 * L5: Cavity Resonance Mode Computation
 *
 * For a rectangular cavity of dimensions a x b x c [m]:
 * Resonant frequency: f_mnp = (c0/2) * sqrt((m/a)^2 + (n/b)^2 + (p/c)^2)
 *
 * where m, n, p are non-negative integers (mode indices),
 * at most one of them can be zero for non-trivial modes.
 *
 * The lowest resonance occurs at the TE_110 or TM_110 mode:
 * f_110 = (c0/2) * sqrt(1/a^2 + 1/b^2)
 *
 * Quality factor Q depends on:
 *   - Wall conductivity (losses in walls)
 *   - Dielectric loss in cavity
 *   - Aperture radiation loss
 *
 * Simplified Q for rectangular cavity:
 *   Q = (2*pi*f*epsilon*V) / (Rs*S)
 *   where Rs = sqrt(pi*f*mu/sigma) is surface resistivity
 *         V = a*b*c is cavity volume
 *         S = 2(ab+bc+ac) is interior surface area
 *
 * Reference: Ramo, Whinnery, Van Duzer "Fields and Waves" Ch.10
 * =================================================================== */
int enclosure_cavity_modes(const enclosure_spec_t *enclosure,
    double max_freq_hz, cavity_analysis_t *analysis) {
    if (!enclosure || !analysis || max_freq_hz <= 0.0) return -1;

    memset(analysis, 0, sizeof(*analysis));
    analysis->enclosure = *enclosure;

    double a = enclosure->dimension_x_m;
    double b = enclosure->dimension_y_m;
    double c = enclosure->dimension_z_m;
    if (a <= 0.0 || b <= 0.0 || c <= 0.0) return -1;

    int count = 0;
    /* Scan mode indices up to reasonable limits */
    for (int m = 0; m < 10 && count < MAX_CAVITY_MODES; m++) {
        for (int n = 0; n < 10 && count < MAX_CAVITY_MODES; n++) {
            for (int p = 0; p < 10 && count < MAX_CAVITY_MODES; p++) {
                /* At most one index can be zero for non-trivial TE/TM mode */
                int zeros = (m == 0) + (n == 0) + (p == 0);
                if (zeros > 1) continue;
                if (m + n + p == 0) continue;

                double f_res = (C0 / 2.0) * sqrt(
                    (m * m) / (a * a) +
                    (n * n) / (b * b) +
                    (p * p) / (c * c));

                if (f_res > max_freq_hz) continue;

                cavity_mode_t *mode = &analysis->modes[count];
                mode->mode_index_m = m;
                mode->mode_index_n = n;
                mode->mode_index_p = p;
                mode->resonant_frequency_hz = f_res;
                mode->is_propagating = 1;

                /* Q estimation using wall loss */
                double V = a * b * c;
                double S = 2.0 * (a*b + b*c + a*c);
                /* Use first layer conductivity for wall loss */
                double sigma_wall = SIGMA_COPPER;
                if (enclosure->shield.num_layers > 0)
                    sigma_wall = enclosure->shield.layers[0].material.conductivity;
                double Rs = sqrt(M_PI * f_res * MU0 / sigma_wall);
                double Q = (2.0 * M_PI * f_res * EPSILON0 * V) / (Rs * S);
                /* Upper bound: Q is also limited by aperture radiation */
                if (enclosure->num_apertures > 0) {
                    double aperture_Q = 100.0; /* typical aperture-limited Q */
                    if (Q > aperture_Q) Q = aperture_Q;
                }
                mode->quality_factor = Q;

                /* Approximate field distribution peak: center of cavity for fundamental */
                if (m <= 1 && n <= 1 && p <= 1) {
                    mode->field_distribution[0] = a / 2.0;
                    mode->field_distribution[1] = b / 2.0;
                    mode->field_distribution[2] = c / 2.0;
                }

                count++;
            }
        }
    }
    analysis->num_modes_found = count;

    /* Sort by frequency (simple bubble sort - small N) */
    for (int i = 0; i < count; i++) {
        for (int j = i + 1; j < count; j++) {
            if (analysis->modes[j].resonant_frequency_hz <
                analysis->modes[i].resonant_frequency_hz) {
                cavity_mode_t tmp = analysis->modes[i];
                analysis->modes[i] = analysis->modes[j];
                analysis->modes[j] = tmp;
            }
        }
    }

    if (count > 0)
        analysis->first_resonance_hz = analysis->modes[0].resonant_frequency_hz;

    return 0;
}
/* ===================================================================
 * L4: Bethe's Small Aperture Theory (1944)
 *
 * For a circular aperture of diameter d << lambda:
 *   SE_slot = 20*log10(lambda / (2*d))  [dB]
 *
 * For a rectangular slot of length L and width w (L >> w):
 *   SE_slot = 20*log10(lambda / (2*L))  [dB]
 *
 * The slot behaves as a magnetic dipole antenna, re-radiating
 * the intercepted field into the enclosure.
 *
 * For N identical apertures: SE degrades by 10*log10(N) [dB]
 *
 * Waveguide-below-cutoff (honeycomb vents):
 *   SE_waveguide = 27.3 * (t/d) * sqrt(1 - (f/f_c)^2)  [dB]
 *   where f_c = c0/(1.706*d) for circular waveguide [Hz]
 *         t = depth of honeycomb [m]
 *         d = cell diameter [m]
 *
 * Reference: Bethe (1944); Ott Ch.10.6; MIL-STD-285
 * =================================================================== */
double enclosure_aperture_leakage(const aperture_spec_t *aperture,
    double freq_hz, se_field_type_t field_type) {
    (void)field_type; /* reserved for future near-field aperture correction */
    if (!aperture || freq_hz <= 0.0) return -1.0;

    double wavelength = C0 / freq_hz;

    /* Maximum dimension determines leakage */
    double L = aperture->dimension1_m; /* primary dimension */
    if (L <= 0.0) return 0.0;

    double SE_ap;

    switch (aperture->type) {
        case APERTURE_SLOT:
        case APERTURE_SEAM:
            /* Slot: SE = 20*log10(lambda / (2*L)) */
            SE_ap = 20.0 * log10(wavelength / (2.0 * L));
            break;

        case APERTURE_CIRCULAR:
            /* Circular hole: SE = 20*log10(lambda / (2*d)) */
            SE_ap = 20.0 * log10(wavelength / (2.0 * L));
            break;

        case APERTURE_RECTANGULAR:
        case APERTURE_VENTILATION:
        case APERTURE_DISPLAY: {
            /* Rectangular: use largest dimension as slot length */
            double w = aperture->dimension2_m;
            double L_eff = (L > w) ? L : w;
            SE_ap = 20.0 * log10(wavelength / (2.0 * L_eff));
            break;
        }

        case APERTURE_HONEYCOMB:
        case APERTURE_PERFORATED: {
            /* Waveguide-below-cutoff model
             * f_c = cutoff frequency for circular waveguide TE11 mode
             * f_c = c0 / (1.706 * d) for circular waveguide */
            double t = aperture->dimension2_m; /* depth/thickness */
            double d = aperture->dimension1_m; /* cell diameter */
            if (t <= 0.0) t = 0.01; /* default 10mm depth */
            if (d <= 0.0) d = 0.005; /* default 5mm cell */
            double f_c = C0 / (1.706 * d);
            if (freq_hz >= f_c) {
                /* Above cutoff: minimal attenuation */
                SE_ap = 5.0;
            } else {
                double factor = sqrt(1.0 - (freq_hz / f_c) * (freq_hz / f_c));
                SE_ap = 27.3 * (t / d) * factor;
            }
            break;
        }

        default:
            SE_ap = 20.0 * log10(wavelength / (2.0 * L));
            break;
    }

    if (SE_ap < 0.0) SE_ap = 0.0;

    /* Multiple identical apertures: SE degrades by 10*log10(N) */
    if (aperture->num_apertures > 1) {
        double degradation = 10.0 * log10((double)aperture->num_apertures);
        SE_ap -= degradation;
        if (SE_ap < 0.0) SE_ap = 0.0;
    }

    return SE_ap;
}

double enclosure_seam_leakage(const enclosure_spec_t *enclosure,
    double freq_hz, seam_treatment_t treatment) {
    if (!enclosure || freq_hz <= 0.0) return -1.0;
    double total_seam = enclosure->seam_total_length_m;
    if (total_seam <= 0.0) return 0.0;
    double effective_slot_m;
    switch (treatment) {
        case SEAM_TREATMENT_NONE:           effective_slot_m = total_seam; break;
        case SEAM_TREATMENT_OVERLAP:        effective_slot_m = total_seam * 0.1; break;
        case SEAM_TREATMENT_CONDUCTIVE_TAPE:effective_slot_m = 0.001; break;
        case SEAM_TREATMENT_GASKET:         effective_slot_m = 0.005 * total_seam; break;
        case SEAM_TREATMENT_WELD_BRAZE:     effective_slot_m = 0.0001; break;
        case SEAM_TREATMENT_SPRING_CONTACT: effective_slot_m = 0.002 * total_seam; break;
        default:                            effective_slot_m = total_seam; break;
    }
    double wavelength = C0 / freq_hz;
    double SE_seam = 20.0 * log10(wavelength / (2.0 * effective_slot_m));
    if (SE_seam < 0.0) SE_seam = 0.0;
    return SE_seam;
}

double enclosure_se_with_apertures(const enclosure_spec_t *enclosure,
    double freq_hz, se_field_type_t field_type) {
    if (!enclosure || freq_hz <= 0.0) return -1.0;
    double SE_wall = shielding_effectiveness_multilayer(
        &enclosure->shield, freq_hz, field_type, 1.0);
    double SE_ap = 1000.0;
    for (int i = 0; i < enclosure->num_apertures; i++) {
        double ap_se = enclosure_aperture_leakage(
            &enclosure->apertures[i], freq_hz, field_type);
        if (ap_se < SE_ap) SE_ap = ap_se;
    }
    if (enclosure->num_apertures == 0) SE_ap = 1000.0;
    double SE_seam = enclosure_seam_leakage(enclosure, freq_hz, enclosure->seam_treatment);
    double SE_net = SE_wall;
    if (SE_ap < SE_net) SE_net = SE_ap;
    if (SE_seam < SE_net) SE_net = SE_seam;
    return SE_net;
}

int enclosure_recommend_gasket(const enclosure_spec_t *enclosure,
    double target_se_db, double max_freq_hz, gasket_spec_t *recommended) {
    if (!enclosure || !recommended || target_se_db <= 0.0 || max_freq_hz <= 0.0)
        return -1;
    double SE_wall = shielding_effectiveness_multilayer(
        &enclosure->shield, max_freq_hz, SE_FIELD_PLANE_WAVE, 1.0);
    double SE_needed = target_se_db - SE_wall;
    if (SE_needed < 0.0) SE_needed = target_se_db * 0.5;

    gasket_material_t candidates[] = {
        GASKET_BERYLLIUM_COPPER, GASKET_SPRING_FINGER, GASKET_WIRE_MESH,
        GASKET_CONDUCTIVE_ELASTOMER, GASKET_KNITTED_WIRE,
        GASKET_CONDUCTIVE_FABRIC, GASKET_FORM_IN_PLACE,
        GASKET_CONDUCTIVE_FOAM, GASKET_METAL_SPIRAL, GASKET_ORIENTED_WIRE,
    };
    int nc = sizeof(candidates) / sizeof(candidates[0]);
    int best = 0; double best_score = -1e9;
    for (int i = 0; i < nc; i++) {
        gasket_spec_t ts;
        if (gasket_spec_init(candidates[i], GASKET_PROFILE_D_SHAPE, 5.0, &ts) != 0) continue;
        gasket_compression_t st;
        gasket_compute_compression(&ts, COMPRESSION_MODE_STOP_LIMITED, 0.0, &st);
        double sl = enclosure->seam_total_length_m > 0.0 ? enclosure->seam_total_length_m : 1.0;
        double SE_g = gasket_shielding_effectiveness(&st, max_freq_hz, sl);
        double ss = (SE_g >= SE_needed) ? 100.0 : SE_g / SE_needed * 100.0;
        int cmp = gasket_galvanic_compatibility(&ts, enclosure->shield.layers[0].material.id);
        double gs = (cmp == 0) ? 100.0 : (cmp == 1) ? 60.0 : 20.0;
        double ts2 = ss * 0.5 + gs * 0.5;
        if (ts2 > best_score) { best_score = ts2; best = i; }
    }
    return gasket_spec_init(candidates[best], GASKET_PROFILE_D_SHAPE, 5.0, recommended);
}

#include "shielding_enclosure.h"

int enclosure_check_screw_spacing(double spacing_m, double max_freq_hz) {
    if (spacing_m <= 0.0 || max_freq_hz <= 0.0) return 0;
    double wavelength = C0 / max_freq_hz;
    double max_spacing = wavelength / 20.0;
    return spacing_m <= max_spacing ? 1 : 0;
}

int enclosure_recommend_screw_count(double perimeter_m, double max_freq_hz) {
    if (perimeter_m <= 0.0 || max_freq_hz <= 0.0) return -1;
    double wavelength = C0 / max_freq_hz;
    double max_spacing = wavelength / 20.0;
    int n = (int)ceil(perimeter_m / max_spacing);
    if (n < 2) n = 2;
    return n;
}

int enclosure_check_resonance_band(const enclosure_spec_t *enclosure,
    double f_start_hz, double f_end_hz) {
    if (!enclosure || f_start_hz <= 0.0 || f_end_hz <= f_start_hz) return 0;
    cavity_analysis_t analysis;
    if (enclosure_cavity_modes(enclosure, f_end_hz, &analysis) != 0) return 0;
    for (int i = 0; i < analysis.num_modes_found; i++) {
        if (analysis.modes[i].resonant_frequency_hz >= f_start_hz &&
            analysis.modes[i].resonant_frequency_hz <= f_end_hz)
            return 1;
    }
    return 0;
}

double enclosure_honeycomb_se(double cell_diameter_m, double thickness_m, double freq_hz) {
    if (cell_diameter_m <= 0.0 || thickness_m <= 0.0 || freq_hz <= 0.0) return -1.0;
    double f_c = C0 / (1.706 * cell_diameter_m);
    if (freq_hz >= f_c) return 5.0;
    double factor = sqrt(1.0 - (freq_hz / f_c) * (freq_hz / f_c));
    return 27.3 * (thickness_m / cell_diameter_m) * factor;
}

double enclosure_max_allowable_slot(double target_se_db, double freq_hz) {
    if (target_se_db <= 0.0 || freq_hz <= 0.0) return -1.0;
    double wavelength = C0 / freq_hz;
    double max_L = wavelength / (2.0 * pow(10.0, target_se_db / 20.0));
    return max_L;
}

/* ===================================================================
 * L6: Additional Enclosure Design Analysis Functions
 * =================================================================== */

/**
 * @brief Compute the slot antenna equivalent SE at resonance.
 *
 * A slot in a conducting plane has maximum radiation when
 * its length = lambda/2. At this frequency, the slot acts
 * as a resonant antenna with near-unity transmission.
 *
 * SE_resonant = 0 dB (perfect transmission at resonance)
 * SE_off_resonant = 20*log10(lambda/(2*L)) for L < lambda/2
 *
 * @param slot_length_m  Slot length [m]
 * @param freq_hz        Frequency [Hz]
 * @return SE at this frequency [dB]
 */
double enclosure_slot_resonance_se(double slot_length_m, double freq_hz) {
    if (slot_length_m <= 0.0 || freq_hz <= 0.0) return -1.0;

    double wavelength = C0 / freq_hz;
    double half_wavelength = wavelength / 2.0;

    /* Resonant when L ~ n*lambda/2 */
    double ratio = slot_length_m / half_wavelength;
    double frac_part = ratio - floor(ratio);

    /* Quality factor: narrower resonance for longer slots */
    double Q = 10.0; /* typical slot Q */
    double detuning = fabs(frac_part - 0.5) * 2.0; /* 0 = resonance, 1 = anti-resonance */

    /* Lorentzian-like resonance shape */
    double resonance_factor = 1.0 / (1.0 + pow(detuning * Q, 2.0));

    /* SE = base_SE * (1 - resonance_factor) */
    double base_SE = 20.0 * log10(wavelength / (2.0 * slot_length_m));
    if (base_SE < 0.0) base_SE = 0.0;

    double SE = base_SE * (1.0 - resonance_factor);
    if (SE < 0.0) SE = 0.0;
    return SE;
}

/**
 * @brief Faraday cage low-frequency magnetic shielding.
 *
 * For a Faraday cage (mesh or perforated metal), the magnetic
 * field attenuation at low frequencies follows:
 *
 * SE_H ~ 20*log10(1 + (2*pi*f*tau)) where tau = L/R
 *
 * For a copper mesh:
 *   L ~ mu_0 * a (a = mesh spacing)
 *   R = rho * l / A (resistive loss)
 *
 * @param mesh_spacing_m    Wire/mesh spacing [m]
 * @param wire_radius_m     Wire radius [m]
 * @param material          Conductor material
 * @param freq_hz           Frequency [Hz]
 * @return Magnetic field SE [dB]
 */
double enclosure_faraday_cage_se_h(double mesh_spacing_m, double wire_radius_m,
    const shielding_material_t *material, double freq_hz) {
    if (mesh_spacing_m <= 0.0 || wire_radius_m <= 0.0 ||
        !material || freq_hz <= 0.0) return -1.0;

    /* Inductance per cell: L ~ mu_0 * a */
    double L = MU0 * mesh_spacing_m;

    /* Resistance per cell perimeter: R = rho * 4a / (pi * r^2) */
    double rho = 1.0 / material->conductivity;
    double A = M_PI * wire_radius_m * wire_radius_m;
    double R = rho * 4.0 * mesh_spacing_m / A;
    if (R <= 0.0) return -1.0;

    double tau = L / R;
    double SE = 20.0 * log10(1.0 + 2.0 * M_PI * freq_hz * tau);
    if (SE < 0.0) SE = 0.0;
    return SE;
}

/**
 * @brief Compute the attenuation of a perforated panel.
 *
 * For a panel with N circular holes of diameter d,
 * the optical transparency is:
 *   eta = N * pi * d^2 / (4 * A_panel)
 *
 * SE = -20*log10(eta) for eta << 1 (many small holes)
 *
 * @param panel_area_m2  Total panel area [m^2]
 * @param hole_diameter_m Hole diameter [m]
 * @param num_holes      Number of holes
 * @param freq_hz        Frequency [Hz]
 * @return Panel SE [dB]
 */
double enclosure_perforated_panel_se(double panel_area_m2,
    double hole_diameter_m, int num_holes, double freq_hz) {
    if (panel_area_m2 <= 0.0 || hole_diameter_m <= 0.0 ||
        num_holes <= 0 || freq_hz <= 0.0) return -1.0;

    double total_hole_area = num_holes * M_PI * hole_diameter_m * hole_diameter_m / 4.0;
    double transparency = total_hole_area / panel_area_m2;

    if (transparency >= 1.0) return 0.0; /* more holes than panel */
    if (transparency <= 0.0) return 200.0; /* solid panel */

    /* SE from optical transparency */
    double SE_optical = -20.0 * log10(transparency);

    /* Waveguide below cutoff for each hole adds SE */
    aperture_spec_t ap;
    memset(&ap, 0, sizeof(ap));
    ap.type = APERTURE_HONEYCOMB;
    ap.dimension1_m = hole_diameter_m;
    ap.dimension2_m = 0.001; /* assume thin panel */
    ap.num_apertures = num_holes;
    double SE_waveguide = enclosure_aperture_leakage(&ap, freq_hz, SE_FIELD_PLANE_WAVE);

    /* Combined: use the better of the two (optical vs waveguide) */
    return (SE_optical > SE_waveguide) ? SE_optical : SE_waveguide;
}

/**
 * @brief PCB shield can design check.
 *
 * PCB-level shielding cans must:
 *   1. Contact PCB ground plane with sufficient vias
 *   2. Have via spacing < lambda/20 at max freq
 *   3. Cover entire noisy/sensitive area
 *
 * @param can_perimeter_m     Can perimeter [m]
 * @param num_ground_vias     Number of ground vias
 * @param max_freq_hz         Max frequency [Hz]
 * @return 0=inadequate, 1=adequate, 2=over-designed
 */
int enclosure_pcb_shield_can_check(double can_perimeter_m,
    int num_ground_vias, double max_freq_hz) {
    if (can_perimeter_m <= 0.0 || num_ground_vias < 1 || max_freq_hz <= 0.0)
        return 0;

    double via_spacing = can_perimeter_m / (double)num_ground_vias;
    double wavelength = C0 / max_freq_hz;
    double max_spacing = wavelength / 20.0;

    if (via_spacing > max_spacing * 2.0) return 0; /* inadequate */
    if (via_spacing < max_spacing * 0.5) return 2; /* over-designed */
    return 1; /* adequate */
}
