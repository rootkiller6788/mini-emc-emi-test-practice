#ifndef SHIELDING_GASKET_H
#define SHIELDING_GASKET_H
#include <stddef.h>
#include <stdint.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SE_FIELD_ELECTRIC=0,
    SE_FIELD_MAGNETIC=1,
    SE_FIELD_PLANE_WAVE=2,
    SE_FIELD_NEAR_E=3,
    SE_FIELD_NEAR_H=4
} se_field_type_t;

typedef enum {
    SE_MECHANISM_REFLECTION=0,
    SE_MECHANISM_ABSORPTION=1,
    SE_MECHANISM_MULTIPLE=2,
    SE_MECHANISM_TOTAL=3
} se_mechanism_t;

typedef enum {
    GASKET_CONDUCTIVE_ELASTOMER=0,
    GASKET_WIRE_MESH=1,
    GASKET_SPRING_FINGER=2,
    GASKET_CONDUCTIVE_FABRIC=3,
    GASKET_FORM_IN_PLACE=4,
    GASKET_METAL_SPIRAL=5,
    GASKET_KNITTED_WIRE=6,
    GASKET_ORIENTED_WIRE=7,
    GASKET_CONDUCTIVE_FOAM=8,
    GASKET_BERYLLIUM_COPPER=9
} gasket_material_t;

typedef enum {
    GASKET_PROFILE_D_SHAPE=0,
    GASKET_PROFILE_RECTANGLE=1,
    GASKET_PROFILE_O_SHAPE=2,
    GASKET_PROFILE_C_FOLD=3,
    GASKET_PROFILE_KNIFE_EDGE=4,
    GASKET_PROFILE_FLAT_WASHER=5,
    GASKET_PROFILE_P_SHAPE=6
} gasket_profile_t;

typedef enum {
    COMPRESSION_MODE_FIXED_DEFLECTION=0,
    COMPRESSION_MODE_FIXED_FORCE=1,
    COMPRESSION_MODE_STOP_LIMITED=2
} compression_mode_t;

typedef enum {
    MATERIAL_COPPER=0,
    MATERIAL_ALUMINUM=1,
    MATERIAL_STEEL_1008=2,
    MATERIAL_MU_METAL=3,
    MATERIAL_SILVER_EPOXY=4,
    MATERIAL_NICKEL=5,
    MATERIAL_TIN_PLATED=6,
    MATERIAL_STAINLESS_304=7,
    MATERIAL_BRASS=8,
    MATERIAL_ZINC=9,
    MATERIAL_CONDUCTIVE_PAINT=10,
    MATERIAL_CUSTOM=99
} shielding_material_id_t;

typedef enum {
    ENCLOSURE_RECTANGULAR=0,
    ENCLOSURE_CYLINDRICAL=1,
    ENCLOSURE_SPHERICAL=2,
    ENCLOSURE_IRREGULAR=3,
    ENCLOSURE_WAVEGUIDE=4,
    ENCLOSURE_PCB_SHIELD_CAN=5,
    ENCLOSURE_GASKETED_TWO_PIECE=6
} enclosure_geometry_t;

typedef enum {
    APERTURE_SLOT=0,
    APERTURE_CIRCULAR=1,
    APERTURE_RECTANGULAR=2,
    APERTURE_HONEYCOMB=3,
    APERTURE_PERFORATED=4,
    APERTURE_SEAM=5,
    APERTURE_VENTILATION=6,
    APERTURE_DISPLAY=7
} aperture_type_t;

typedef enum {
    SEAM_TREATMENT_NONE=0,
    SEAM_TREATMENT_GASKET=1,
    SEAM_TREATMENT_OVERLAP=2,
    SEAM_TREATMENT_CONDUCTIVE_TAPE=3,
    SEAM_TREATMENT_WELD_BRAZE=4,
    SEAM_TREATMENT_SPRING_CONTACT=5
} seam_treatment_t;

/* Core Data Structures */

typedef struct {
    shielding_material_id_t id;
    const char *name;
    double conductivity;
    double relative_permeability;
    double relative_permittivity;
    double density;
    double thickness_min_mm;
    double thickness_max_mm;
    double cost_factor;
} shielding_material_t;

typedef struct {
    shielding_material_t material;
    double thickness_m;
    double distance_from_source_m;
} shield_layer_t;

#define MAX_SHIELD_LAYERS 5
typedef struct {
    shield_layer_t layers[MAX_SHIELD_LAYERS];
    int num_layers;
    double air_gap_m[MAX_SHIELD_LAYERS - 1];
} shield_stack_t;

typedef struct {
    gasket_material_t material;
    gasket_profile_t profile;
    double width_mm;
    double height_uncompressed_mm;
    double height_compressed_mm;
    double compression_set_percent;
    double closure_force_n_per_m;
    double volume_resistivity_ohm_cm;
    double transfer_impedance_ohm_m;
    double temperature_max_c;
    double temperature_min_c;
    int ip_rating;
    int environmental_class;
} gasket_spec_t;

typedef struct {
    gasket_spec_t spec;
    compression_mode_t mode;
    double applied_force_n_per_m;
    double deflection_mm;
    double compression_percent;
    double contact_resistance_ohm;
    double effective_attenuation_db;
} gasket_compression_t;

typedef struct {
    aperture_type_t type;
    double dimension1_m;
    double dimension2_m;
    int num_apertures;
    double center_x_m;
    double center_y_m;
    int is_seam;
    double seam_length_m;
    seam_treatment_t seam_treatment;
} aperture_spec_t;

typedef struct {
    enclosure_geometry_t geometry;
    double dimension_x_m;
    double dimension_y_m;
    double dimension_z_m;
    shield_stack_t shield;
    aperture_spec_t *apertures;
    int num_apertures;
    double seam_total_length_m;
    seam_treatment_t seam_treatment;
    int num_ground_points;
    double pcb_height_m;
} enclosure_spec_t;

typedef struct {
    double frequency_hz;
    se_field_type_t field_type;
    double absorption_loss_db;
    double reflection_loss_db;
    double multiple_reflection_correction_db;
    double total_se_db;
    double near_field_correction_db;
    double aperture_leakage_db;
    double seam_leakage_db;
    double net_se_db;
} se_result_t;

typedef struct {
    int mode_index_m;
    int mode_index_n;
    int mode_index_p;
    double resonant_frequency_hz;
    double quality_factor;
    double field_distribution[3];
    int is_propagating;
} cavity_mode_t;

#define MAX_CAVITY_MODES 50

typedef struct {
    enclosure_spec_t enclosure;
    cavity_mode_t modes[MAX_CAVITY_MODES];
    int num_modes_found;
    double first_resonance_hz;
    double lowest_se_frequency_hz;
    double lowest_se_value_db;
    double average_se_0_1ghz_db;
    double average_se_1_10ghz_db;
} cavity_analysis_t;

/* Fundamental Constants (L4) */
#define Z0_FREE_SPACE  376.730313668
#define EPSILON0       8.8541878128e-12
#define MU0            1.25663706212e-6
#define C0             299792458.0
#define SIGMA_COPPER   5.80e7

/* Material Library (L2) */
int shielding_material_get(shielding_material_id_t id, shielding_material_t *mat);
double shielding_skin_depth(const shielding_material_t *material, double freq_hz);
double shielding_wave_impedance(const shielding_material_t *material, double freq_hz);

/* Schelkunoff Shielding Theory (L4: Fundamental Laws) */
double shielding_absorption_loss(const shield_layer_t *layer, double freq_hz);
double shielding_reflection_loss(const shield_layer_t *layer, double freq_hz,
    se_field_type_t field_type, double dist_from_source_m);
double shielding_multiple_reflection_correction(const shield_layer_t *layer, double freq_hz);
se_result_t shielding_effectiveness_single_layer(const shield_layer_t *layer,
    double freq_hz, se_field_type_t field_type, double dist_from_source_m);
double shielding_effectiveness_multilayer(const shield_stack_t *stack,
    double freq_hz, se_field_type_t field_type, double dist_from_source_m);

/* Gasket Modeling (L2/L5) */
int gasket_spec_init(gasket_material_t material, gasket_profile_t profile,
    double width_mm, gasket_spec_t *spec);
int gasket_compute_compression(const gasket_spec_t *spec,
    compression_mode_t mode, double value, gasket_compression_t *state);
double gasket_transfer_impedance(const gasket_compression_t *state, double freq_hz);
double gasket_shielding_effectiveness(const gasket_compression_t *state,
    double freq_hz, double seam_length_m);
double gasket_service_life_estimate(const gasket_spec_t *spec,
    double daily_cycles, double temp_c, int uv_exposure);
int gasket_galvanic_compatibility(const gasket_spec_t *gasket_spec,
    shielding_material_id_t flange_material);

/* Enclosure Analysis (L5/L6) */
int enclosure_cavity_modes(const enclosure_spec_t *enclosure,
    double max_freq_hz, cavity_analysis_t *analysis);
double enclosure_aperture_leakage(const aperture_spec_t *aperture,
    double freq_hz, se_field_type_t field_type);
double enclosure_seam_leakage(const enclosure_spec_t *enclosure,
    double freq_hz, seam_treatment_t treatment);
double enclosure_se_with_apertures(const enclosure_spec_t *enclosure,
    double freq_hz, se_field_type_t field_type);
int enclosure_recommend_gasket(const enclosure_spec_t *enclosure,
    double target_se_db, double max_freq_hz, gasket_spec_t *recommended);

/* Measurement (L7) */
double shielding_transfer_impedance_mil_std_285(double freq_hz,
    double sample_size_m, double measured_se_db);
double shielding_dynamic_range_estimate(double noise_floor_dbm,
    double max_power_dbm, double antenna_gain_db);

/* Application Scenarios (L7/L8) */
int shielding_automotive_ecu_enclosure(double target_se_db,
    double max_dimension_m, enclosure_spec_t *result);
int shielding_medical_device_enclosure(double target_se_db,
    enclosure_spec_t *result);
int shielding_5g_base_station_enclosure(double target_se_db,
    double internal_heat_w, enclosure_spec_t *result);

#ifdef __cplusplus
}
#endif
#endif /* SHIELDING_GASKET_H */

/* ===================================================================
 * L5: Additional Shield Optimization Functions
 * =================================================================== */

/* Frequency sweep and optimization */
int shielding_frequency_sweep(const shield_layer_t *layer,
    double f_start_hz, double f_end_hz, int points_per_decade,
    se_field_type_t field_type, double dist_m,
    se_result_t **results, int *num_points);
int shielding_find_crossover_frequency(const shield_layer_t *layer,
    double f_min_hz, double f_max_hz, double threshold_db,
    se_field_type_t field_type, double dist_m, double *result_hz);
double shielding_minimum_thickness(const shielding_material_t *material,
    double freq_hz, double target_se_db, se_field_type_t field_type,
    double dist_m);

/* Gasket multi-criteria analysis */
int gasket_rank_by_criteria(shielding_material_id_t flange_material,
    double freq_hz, double seam_length_m, int env_class,
    gasket_material_t *rankings, int *num_ranked);
int gasket_closure_force_budget(const gasket_spec_t *spec,
    double perimeter_m, int num_fasteners,
    double *total_force_n, double *force_per_fastener_n);
int gasket_check_ip_rating(const gasket_compression_t *state, int ip_target);

/* Advanced enclosure analysis */
double enclosure_slot_resonance_se(double slot_length_m, double freq_hz);
double enclosure_faraday_cage_se_h(double mesh_spacing_m, double wire_radius_m,
    const shielding_material_t *material, double freq_hz);
double enclosure_perforated_panel_se(double panel_area_m2,
    double hole_diameter_m, int num_holes, double freq_hz);
int enclosure_pcb_shield_can_check(double can_perimeter_m,
    int num_ground_vias, double max_freq_hz);

/* Industry-specific enclosure designs */
int shielding_aerospace_avionics_enclosure(double target_se_db,
    double max_dimension_m, int altitude_class, enclosure_spec_t *result);
int shielding_consumer_electronics_enclosure(double target_se_db,
    int is_handheld, enclosure_spec_t *result);
int shielding_motor_drive_enclosure(double power_kw,
    double switching_freq_khz, enclosure_spec_t *result);

/* ===================================================================
 * L2: Near-Field / Far-Field Boundary Computation
 *
 * The transition between near-field and far-field occurs at:
 *   r = lambda / (2*pi)  ~  lambda / 6.28
 *
 * For r < lambda/(2*pi): near field (reactive, complex impedance)
 * For r > lambda/(2*pi): far field (plane wave, Z = 377 ohm)
 *
 * Near-field E (high impedance): Z_w > Z_0, reflection enhanced
 * Near-field H (low impedance):  Z_w < Z_0, reflection reduced
 * =================================================================== */

/**
 * @brief Compute near-field / far-field boundary distance.
 * boundary = C0 / (2 * pi * f) = lambda / (2*pi)
 * @param freq_hz  Frequency [Hz]
 * @return Boundary distance [m]
 */
double shielding_near_field_boundary(double freq_hz);

/**
 * @brief Determine field region type based on distance and frequency.
 * @param distance_m  Distance from source [m]
 * @param freq_hz     Frequency [Hz]
 * @return SE_FIELD_PLANE_WAVE if far field,
 *         SE_FIELD_NEAR_E or SE_FIELD_NEAR_H for near field
 */
se_field_type_t shielding_determine_field_region(double distance_m, double freq_hz);

/**
 * @brief Print a human-readable SE report to stdout.
 * @param result  SE computation result
 */
void shielding_print_se_report(const se_result_t *result);

/**
 * @brief Print enclosure analysis summary to stdout.
 * @param analysis  Cavity analysis result
 */
void shielding_print_cavity_report(const cavity_analysis_t *analysis);

/**
 * @brief Validate enclosure design for common errors.
 * Checks: non-zero dimensions, valid material, reasonable thickness, etc.
 * @param enclosure  Enclosure to validate
 * @return 0 if valid, positive error code if invalid
 */
int shielding_validate_enclosure(const enclosure_spec_t *enclosure);
