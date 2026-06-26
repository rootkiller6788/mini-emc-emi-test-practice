/* shielding_application.c -- Application-Specific Shielding Designs
 * L7: Applications - Automotive, medical, 5G base station enclosures
 * L8: Advanced Topics - Thermal-shielding co-design, composite materials
 *
 * Reference: Ford EMC-CS-2009 Automotive EMC Spec
 *            CISPR 25 Vehicles, boats - Radio disturbance characteristics
 *            IEC 60601-1-2 Medical electrical equipment EMC
 *            3GPP TS 38.104 5G NR Base Station EMC
 *            NASA-HDBK-4001 Electrical Grounding for Spacecraft
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shielding_gasket.h"
#include "shielding_material.h"

/*
 * L7: Automotive ECU Enclosure Design
 *
 * Typical requirements:
 *   - SE >= 40 dB at 1 GHz (CISPR 25 Class 5)
 *   - SE >= 20 dB at 10 GHz (automotive radar coexistence)
 *   - Operating temperature: -40 to +125 deg C
 *   - Vibration: 10-2000 Hz, 10g rms
 *   - IP6x sealing
 *   - Maximum dimension for under-hood mounting
 *
 * Design approach:
 *   1. Aluminum enclosure for weight/cost
 *   2. Conductive elastomer gasket for environmental sealing
 *   3. Overlap seams with screw spacing < lambda/20
 */
int shielding_automotive_ecu_enclosure(double target_se_db,
    double max_dimension_m, enclosure_spec_t *result) {
    if (!result || target_se_db <= 0.0 || max_dimension_m <= 0.0) return -1;

    memset(result, 0, sizeof(*result));
    result->geometry = ENCLOSURE_RECTANGULAR;

    /* Size: compact form factor */
    result->dimension_x_m = max_dimension_m;
    result->dimension_y_m = max_dimension_m * 0.7;
    result->dimension_z_m = max_dimension_m * 0.3;

    /* Shield: 1.5mm aluminum */
    shielding_material_t al_mat;
    if (shielding_material_get(MATERIAL_ALUMINUM, &al_mat) != 0) return -1;
    result->shield.num_layers = 1;
    result->shield.layers[0].material = al_mat;
    result->shield.layers[0].thickness_m = 0.0015; /* 1.5mm */
    result->shield.layers[0].distance_from_source_m = 0.1;
    result->shield.air_gap_m[0] = 0.0;

    /* Seam: four edges, overlap design with gasket */
    double perimeter = 2.0 * (result->dimension_x_m + result->dimension_y_m);
    result->seam_total_length_m = perimeter;
    result->seam_treatment = SEAM_TREATMENT_GASKET;

    /* Ground: 4 corner mounting points */
    result->num_ground_points = 4;
    result->pcb_height_m = 0.005; /* 5mm standoff */

    return 0;
}

/*
 * L7: Medical Device Enclosure
 *
 * Requirements per IEC 60601-1-2:
 *   - SE >= 60 dB for life-supporting equipment
 *   - SE >= 40 dB for non-life-supporting
 *   - Must maintain SE under sterilization conditions
 *   - Biocompatible materials (no beryllium, no cadmium)
 *
 * Design approach:
 *   1. Stainless steel 304 (biocompatible, autoclavable)
 *   2. Conductive silicone gasket (medical grade)
 *   3. Welded or continuous seam construction
 *   4. Rounded corners (no sharp edges per medical safety)
 */
int shielding_medical_device_enclosure(double target_se_db,
    enclosure_spec_t *result) {
    if (!result || target_se_db <= 0.0) return -1;

    memset(result, 0, sizeof(*result));
    result->geometry = ENCLOSURE_RECTANGULAR;

    /* Typical medical device: 200x150x80 mm */
    result->dimension_x_m = 0.200;
    result->dimension_y_m = 0.150;
    result->dimension_z_m = 0.080;

    /* Shield: 1.0mm stainless steel 304 */
    shielding_material_t ss_mat;
    if (shielding_material_get(MATERIAL_STAINLESS_304, &ss_mat) != 0) return -1;
    result->shield.num_layers = 1;
    result->shield.layers[0].material = ss_mat;
    result->shield.layers[0].thickness_m = 0.001;
    result->shield.layers[0].distance_from_source_m = 0.1;
    result->shield.air_gap_m[0] = 0.0;

    /* Seam: welded construction preferred for medical */
    result->seam_total_length_m = 2.0 * (0.200 + 0.150);
    result->seam_treatment = SEAM_TREATMENT_WELD_BRAZE;

    /* Display aperture (patient monitor screen) */
    result->num_apertures = 1;
    result->apertures = NULL; /* caller must allocate or this is just a template */

    result->num_ground_points = 1; /* single-point ground per medical safety */
    result->pcb_height_m = 0.008;

    return 0;
}

/*
 * L7: 5G NR Base Station Enclosure
 *
 * Requirements per 3GPP TS 38.104:
 *   - SE >= 80 dB at sub-6 GHz (FR1)
 *   - SE >= 60 dB at mmWave 28 GHz (FR2)
 *   - Thermal management: internal heat > 500W
 *   - Outdoor environment, IP67
 *   - Lightning protection (IEC 62305)
 *
 * Design approach:
 *   1. Multi-layer shield: aluminum + mu-metal for low-freq magnetic
 *   2. Conductive elastomer + waveguide vents for cooling
 *   3. Honeycomb air vents for high-freq attenuation
 *   4. Multiple grounding points for lightning
 *
 * This is a full base station remote radio unit (RRU) enclosure.
 * Thermal management is as critical as EMI shielding.
 */
int shielding_5g_base_station_enclosure(double target_se_db,
    double internal_heat_w, enclosure_spec_t *result) {
    if (!result || target_se_db <= 0.0 || internal_heat_w <= 0.0) return -1;

    memset(result, 0, sizeof(*result));
    result->geometry = ENCLOSURE_RECTANGULAR;

    /* Typical RRU: 400x300x150 mm */
    result->dimension_x_m = 0.400;
    result->dimension_y_m = 0.300;
    result->dimension_z_m = 0.150;

    /* Layer 1: Aluminum 2mm for high-freq electric field shielding */
    shielding_material_t al_mat;
    shielding_material_get(MATERIAL_ALUMINUM, &al_mat);

    /* Layer 2: Mu-metal 0.5mm for low-freq magnetic (50/60 Hz power) */
    shielding_material_t mu_mat;
    shielding_material_get(MATERIAL_MU_METAL, &mu_mat);

    result->shield.num_layers = 2;
    result->shield.layers[0] = (shield_layer_t){al_mat, 0.002, 0.5};
    result->shield.layers[1] = (shield_layer_t){mu_mat, 0.0005, 0.5};
    result->shield.air_gap_m[0] = 0.003; /* 3mm air gap */

    /* Seam: continuous gasket + IP67 environmental seal */
    double perimeter = 2.0 * (0.400 + 0.300);
    result->seam_total_length_m = perimeter;
    result->seam_treatment = SEAM_TREATMENT_GASKET;

    /* Ventilation: honeycomb waveguide vents
     * For 500W with 40 deg C rise: need ~0.01 m^2 vent area */
    result->num_apertures = 2;
    result->apertures = NULL; /* template */

    /* Ground: 8 points for lightning protection */
    result->num_ground_points = 8;
    result->pcb_height_m = 0.010;

    return 0;
}

#include "shielding_application.h"

int shielding_industry_requirements(industry_sector_t sector, industry_requirement_t *req) {
    if (!req) return -1;
    memset(req, 0, sizeof(*req));
    switch (sector) {
        case INDUSTRY_AUTOMOTIVE:
            req->industry = "Automotive (CISPR 25)";
            req->standard = "CISPR 25 / Ford EMC-CS-2009";
            req->se_30mhz_db = 20.0;
            req->se_100mhz_db = 30.0;
            req->se_1ghz_db = 40.0;
            req->se_10ghz_db = 20.0;
            req->environmental_class = 2;
            req->ip_rating_min = 65;
            break;
        case INDUSTRY_MEDICAL:
            req->industry = "Medical (IEC 60601-1-2)";
            req->standard = "IEC 60601-1-2 Ed.4";
            req->se_30mhz_db = 30.0;
            req->se_100mhz_db = 40.0;
            req->se_1ghz_db = 60.0;
            req->se_10ghz_db = 40.0;
            req->environmental_class = 1;
            req->ip_rating_min = 54;
            break;
        case INDUSTRY_AEROSPACE:
            req->industry = "Aerospace (DO-160)";
            req->standard = "RTCA DO-160G Sec.21";
            req->se_30mhz_db = 40.0;
            req->se_100mhz_db = 50.0;
            req->se_1ghz_db = 60.0;
            req->se_10ghz_db = 50.0;
            req->environmental_class = 4;
            req->ip_rating_min = 67;
            break;
        case INDUSTRY_TELECOM_5G:
            req->industry = "5G Telecom (3GPP TS 38.104)";
            req->standard = "3GPP TS 38.104";
            req->se_30mhz_db = 40.0;
            req->se_100mhz_db = 50.0;
            req->se_1ghz_db = 80.0;
            req->se_10ghz_db = 60.0;
            req->environmental_class = 3;
            req->ip_rating_min = 67;
            break;
        case INDUSTRY_CONSUMER:
            req->industry = "Consumer Electronics (FCC Part 15)";
            req->standard = "FCC Part 15 / EN 55032";
            req->se_30mhz_db = 10.0;
            req->se_100mhz_db = 15.0;
            req->se_1ghz_db = 20.0;
            req->se_10ghz_db = 10.0;
            req->environmental_class = 1;
            req->ip_rating_min = 20;
            break;
        case INDUSTRY_INDUSTRIAL:
            req->industry = "Industrial (IEC 61800-3)";
            req->standard = "IEC 61800-3 / EN 55011";
            req->se_30mhz_db = 30.0;
            req->se_100mhz_db = 40.0;
            req->se_1ghz_db = 50.0;
            req->se_10ghz_db = 30.0;
            req->environmental_class = 2;
            req->ip_rating_min = 54;
            break;
        case INDUSTRY_MILITARY:
            req->industry = "Military (MIL-STD-461)";
            req->standard = "MIL-STD-461G";
            req->se_30mhz_db = 60.0;
            req->se_100mhz_db = 70.0;
            req->se_1ghz_db = 80.0;
            req->se_10ghz_db = 60.0;
            req->environmental_class = 4;
            req->ip_rating_min = 68;
            break;
        case INDUSTRY_RAILWAY:
            req->industry = "Railway (EN 50121)";
            req->standard = "EN 50121-3-2";
            req->se_30mhz_db = 30.0;
            req->se_100mhz_db = 40.0;
            req->se_1ghz_db = 50.0;
            req->se_10ghz_db = 30.0;
            req->environmental_class = 2;
            req->ip_rating_min = 65;
            break;
        case INDUSTRY_MARINE:
            req->industry = "Marine (DNV/IEC 60945)";
            req->standard = "IEC 60945";
            req->se_30mhz_db = 30.0;
            req->se_100mhz_db = 40.0;
            req->se_1ghz_db = 50.0;
            req->se_10ghz_db = 30.0;
            req->environmental_class = 3;
            req->ip_rating_min = 67;
            break;
        case INDUSTRY_IOT_EDGE:
            req->industry = "IoT Edge Computing";
            req->standard = "ETSI EN 301 489";
            req->se_30mhz_db = 20.0;
            req->se_100mhz_db = 25.0;
            req->se_1ghz_db = 30.0;
            req->se_10ghz_db = 15.0;
            req->environmental_class = 2;
            req->ip_rating_min = 54;
            break;
        default:
            return -1;
    }
    return 0;
}

int shielding_check_compliance(const enclosure_spec_t *enclosure,
    const industry_requirement_t *req) {
    if (!enclosure || !req) return -1;
    double freqs[] = {30e6, 100e6, 1000e6, 10000e6};
    double reqs[] = {req->se_30mhz_db, req->se_100mhz_db, req->se_1ghz_db, req->se_10ghz_db};
    int marginal_count = 0;
    for (int i = 0; i < 4; i++) {
        double SE = enclosure_se_with_apertures(enclosure, freqs[i], SE_FIELD_PLANE_WAVE);
        if (SE < reqs[i] - 6.0) return 2;
        if (SE < reqs[i]) marginal_count++;
    }
    return marginal_count > 0 ? 1 : 0;
}

double shielding_ventilation_area_needed(double internal_heat_w,
    double ambient_temp_c, double max_internal_temp_c) {
    double delta_T = max_internal_temp_c - ambient_temp_c;
    if (delta_T <= 0.0) return 0.0;
    double h = 5.0;
    return internal_heat_w / (h * delta_T);
}

double shielding_enclosure_thermal_resistance(const enclosure_spec_t *enclosure) {
    if (!enclosure) return -1.0;
    double a = enclosure->dimension_x_m;
    double b = enclosure->dimension_y_m;
    double c = enclosure->dimension_z_m;
    double surface_area = 2.0 * (a*b + b*c + a*c);
    if (surface_area <= 0.0) return -1.0;
    double k = 167.0;
    if (enclosure->shield.num_layers > 0) {
        material_extended_t ext;
        if (shielding_material_get_extended(enclosure->shield.layers[0].material.id, &ext) == 0)
            k = ext.thermal_conductivity_w_mk;
    }
    double t = 0.001;
    if (enclosure->shield.num_layers > 0)
        t = enclosure->shield.layers[0].thickness_m;
    return t / (k * surface_area);
}

int shielding_thermal_emc_co_design(double target_se_db, double max_freq_hz,
    double internal_heat_w, double max_temp_rise_c, enclosure_spec_t *result) {
    if (!result || target_se_db <= 0.0 || max_freq_hz <= 0.0) return -1;
    memset(result, 0, sizeof(*result));
    result->geometry = ENCLOSURE_RECTANGULAR;
    double vent_area = shielding_ventilation_area_needed(internal_heat_w, 25.0, 25.0 + max_temp_rise_c); (void)vent_area; /* reserved for future thermal optimization */
    result->dimension_x_m = 0.300;
    result->dimension_y_m = 0.200;
    result->dimension_z_m = 0.100;
    shielding_material_t al;
    shielding_material_get(MATERIAL_ALUMINUM, &al);
    result->shield.num_layers = 1;
    result->shield.layers[0].material = al;
    result->shield.layers[0].thickness_m = 0.002;
    result->shield.layers[0].distance_from_source_m = 1.0;
    result->seam_total_length_m = 2.0 * (0.300 + 0.200);
    result->seam_treatment = SEAM_TREATMENT_GASKET;
    result->num_ground_points = 4;
    result->pcb_height_m = 0.010;
    return 0;
}

/* ===================================================================
 * L7: Additional Industry Application Functions
 * =================================================================== */

/**
 * @brief Aerospace avionics enclosure design per DO-160G.
 *
 * DO-160G Section 21 requires:
 *   - SE >= 60 dB from 100 MHz to 18 GHz
 *   - Lightning indirect effects protection
 *   - HIRF (High Intensity Radiated Fields) protection
 *   - Temperature range: -55 to +85 deg C
 *   - Altitude: up to 55,000 ft
 *
 * Uses aluminum with mu-metal liner for low-frequency magnetic
 * shielding against 400 Hz aircraft power.
 */
int shielding_aerospace_avionics_enclosure(double target_se_db,
    double max_dimension_m, int altitude_class, enclosure_spec_t *result) {
    if (!result || target_se_db <= 0.0) return -1;

    memset(result, 0, sizeof(*result));
    result->geometry = ENCLOSURE_RECTANGULAR;

    /* Avionics LRU: typically 300x200x150 mm */
    result->dimension_x_m = max_dimension_m;
    result->dimension_y_m = max_dimension_m * 0.65;
    result->dimension_z_m = max_dimension_m * 0.5;

    /* Multi-layer: aluminum for high freq, mu-metal for 400 Hz */
    shielding_material_t al, mu;
    shielding_material_get(MATERIAL_ALUMINUM, &al);
    shielding_material_get(MATERIAL_MU_METAL, &mu);
    result->shield.num_layers = 2;
    result->shield.layers[0].material = mu;
    result->shield.layers[0].thickness_m = 0.0005;
    result->shield.layers[0].distance_from_source_m = 0.5;
    result->shield.layers[1].material = al;
    result->shield.layers[1].thickness_m = 0.002;
    result->shield.layers[1].distance_from_source_m = 0.5;
    result->shield.air_gap_m[0] = 0.002;

    /* Seam: beryllium copper fingerstock for high reliability */
    double perimeter = 2.0 * (result->dimension_x_m + result->dimension_y_m);
    result->seam_total_length_m = perimeter;
    result->seam_treatment = SEAM_TREATMENT_SPRING_CONTACT;

    /* Ground: 8 points for lightning protection per DO-160G Sec.22 */
    result->num_ground_points = 8;
    result->pcb_height_m = 0.012;

    /* Altitude class affects pressure equalization requirements */
    /* Class A=15000ft, B=25000ft, C=35000ft, D=50000ft, E=55000ft */
    if (altitude_class >= 4) {
        /* High altitude: need pressure vent with waveguide filter */
    }

    return 0;
}

/**
 * @brief Consumer electronics shielding (FCC Part 15 Class B).
 *
 * Typical requirements:
 *   - SE >= 10-20 dB (much less stringent than industrial/military)
 *   - Focus on cost and aesthetics
 *   - Thin conductive coating often sufficient
 *   - Plastic enclosure with conductive paint/shielding
 */
int shielding_consumer_electronics_enclosure(double target_se_db,
    int is_handheld, enclosure_spec_t *result) {
    if (!result || target_se_db <= 0.0) return -1;

    memset(result, 0, sizeof(*result));
    result->geometry = is_handheld ? ENCLOSURE_IRREGULAR : ENCLOSURE_RECTANGULAR;

    /* Smartphone/tablet: ~150x75x8mm; desktop: ~300x200x50mm */
    if (is_handheld) {
        result->dimension_x_m = 0.150;
        result->dimension_y_m = 0.075;
        result->dimension_z_m = 0.008;
    } else {
        result->dimension_x_m = 0.300;
        result->dimension_y_m = 0.200;
        result->dimension_z_m = 0.050;
    }

    /* Conductive paint or thin spray coating */
    shielding_material_t paint;
    shielding_material_get(MATERIAL_CONDUCTIVE_PAINT, &paint);
    result->shield.num_layers = 1;
    result->shield.layers[0].material = paint;
    result->shield.layers[0].thickness_m = 0.00005; /* 50um typical */
    result->shield.layers[0].distance_from_source_m = 0.05;

    /* Seam: conductive fabric gasket or form-in-place */
    double perimeter = 2.0 * (result->dimension_x_m + result->dimension_y_m);
    result->seam_total_length_m = perimeter;
    result->seam_treatment = SEAM_TREATMENT_CONDUCTIVE_TAPE;

    /* Display aperture: large, dominant leakage path */
    result->num_ground_points = is_handheld ? 2 : 4;
    result->pcb_height_m = 0.003;

    return 0;
}

/**
 * @brief Industrial motor drive enclosure (IEC 61800-3).
 *
 * VFD/VSD enclosures must handle:
 *   - High conducted EMI from PWM switching (2-16 kHz)
 *   - High dV/dt coupling to enclosure
 *   - Motor cable radiation (3-30 MHz typically)
 *   - Thermal management for IGBT losses
 */
int shielding_motor_drive_enclosure(double power_kw,
    double switching_freq_khz, enclosure_spec_t *result) {
    (void)switching_freq_khz; /* reserved for PWM frequency-dependent design */
    if (!result || power_kw <= 0.0) return -1;

    memset(result, 0, sizeof(*result));
    result->geometry = ENCLOSURE_RECTANGULAR;

    /* Size scales with power: ~0.1m per kW for small drives */
    double size_factor = 0.05 * pow(power_kw, 0.33);
    if (size_factor < 0.3) size_factor = 0.3;
    result->dimension_x_m = size_factor * 2.0;
    result->dimension_y_m = size_factor * 1.5;
    result->dimension_z_m = size_factor * 1.0;

    /* Steel: good low-frequency magnetic shielding for PWM */
    shielding_material_t steel;
    shielding_material_get(MATERIAL_STEEL_1008, &steel);
    result->shield.num_layers = 1;
    result->shield.layers[0].material = steel;
    result->shield.layers[0].thickness_m = 0.0015;
    result->shield.layers[0].distance_from_source_m = 0.2;

    /* Seam: continuous weld preferred for conducted emissions */
    double perimeter = 2.0 * (result->dimension_x_m + result->dimension_y_m);
    result->seam_total_length_m = perimeter;
    result->seam_treatment = SEAM_TREATMENT_WELD_BRAZE;

    /* Ventilation: honeycomb for IGBT cooling */
    result->num_ground_points = 1; /* Single-point ground for VFD */
    result->pcb_height_m = 0.015;

    return 0;
}
