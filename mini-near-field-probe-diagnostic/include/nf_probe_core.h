#ifndef NF_PROBE_CORE_H
#define NF_PROBE_CORE_H
/**
 * @file nf_probe_core.h
 * @brief Near-Field Probe Diagnostic — Core Definitions (L1-L2)
 *
 * IEEE/ANSI C63.2 / CISPR 16-1-4 / IEC 61967-3 standard foundation.
 *
 * Knowledge Layers:
 *   L1: Near-field region definition, probe types, impedance/attenuation
 *   L2: Radiating vs reactive near-field distinction, reciprocity principle
 *
 * Ref: Clayton R. Paul, "Introduction to Electromagnetic Compatibility", 2nd ed, 2006
 *      David M. Pozar, "Microwave Engineering", 4th ed, 2012
 *      CISPR 16-1-4, "Specification for radio disturbance and immunity measuring apparatus"
 */

#include <stddef.h>
#include <stdint.h>
#include <complex.h>
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


/* ============================================================================
 * L1 — Core Definitions
 * ============================================================================
 */

#define NF_PROBE_FREQ_MIN_HZ   1.0e5
#define NF_PROBE_FREQ_MAX_HZ   6.0e9

#define ETA_0  376.730313668
#define MU_0   1.25663706212e-6
#define EPS_0  8.8541878128e-12
#define C_0    299792458.0

/* ---------- Near-Field / Far-Field Region Boundaries (L1) ---------- */

typedef enum {
    NF_REGION_REACTIVE_NEAR  = 0,
    NF_REGION_RADIATING_NEAR = 1,
    NF_REGION_FRAUNHOFER    = 2
} nf_region_t;

/* ---------- Probe Types (L1) ---------- */

typedef enum {
    NF_PROBE_H_FIELD_LOOP     = 0,
    NF_PROBE_E_FIELD_MONOPOLE = 1,
    NF_PROBE_E_FIELD_DIPOLE   = 2,
    NF_PROBE_MICROSTRIP       = 3,
    NF_PROBE_TEM_CELL         = 4,
    NF_PROBE_OPTICAL_EO       = 5,
    NF_PROBE_THERMAL          = 6
} nf_probe_type_t;

typedef enum {
    NF_ORIENT_ISOTROPIC       = 0,
    NF_ORIENT_UNIAXIAL_X      = 1,
    NF_ORIENT_UNIAXIAL_Y      = 2,
    NF_ORIENT_UNIAXIAL_Z      = 3,
    NF_ORIENT_TANGENTIAL_H    = 4,
    NF_ORIENT_NORMAL_E        = 5
} nf_orient_t;

/* ---------- Probe Physical Parameter Structs (L1) ---------- */

typedef struct {
    double radius_m;
    double wire_radius_m;
    int    turns;
    double self_inductance_H;
    double self_capacitance_F;
    double resonance_freq_Hz;
    double shielding_effectiveness_db;
} nf_h_loop_params_t;

typedef struct {
    double length_m;
    double effective_height_m;
    double tip_capacitance_F;
    double input_impedance_ohm;
    double bandwidth_hz_lo;
    double bandwidth_hz_hi;
} nf_e_monopole_params_t;

typedef struct {
    nf_probe_type_t type;
    nf_orient_t     orientation;
    char            model[64];
    char            manufacturer[64];
    double          freq_min_hz;
    double          freq_max_hz;
    double          spatial_res_m;
    double          sensitivity_v_per_am;
    double          sensitivity_v_per_vm;
    union {
        nf_h_loop_params_t      h_loop;
        nf_e_monopole_params_t  e_monopole;
    } specific;
} nf_probe_spec_t;

/* ---------- Measurement Coordinate System (L1) ---------- */

typedef struct {
    double x_m;
    double y_m;
    double z_m;
    double freq_hz;
} nf_scan_point_t;

typedef struct {
    double complex Ex;
    double complex Ey;
    double complex Ez;
    double complex Hx;
    double complex Hy;
    double complex Hz;
    double complex Sx;
    double complex Sy;
    double complex Sz;
} nf_field_sample_t;

typedef struct {
    double x_start_m;
    double x_end_m;
    double y_start_m;
    double y_end_m;
    double z_height_m;
    size_t nx;
    size_t ny;
    double dx_m;
    double dy_m;
} nf_scan_grid_t;

/* ---------- Field Computation Helpers (L1) ---------- */

#define NF_FIELD_MAGNITUDE(f)  cabs(f)
#define NF_FIELD_DB(f, ref)    (20.0 * log10(cabs(f) / (ref)))
#define NF_FIELD_PHASE(f)      carg(f)

/* ============================================================================
 * L2 — Core Concepts
 * ============================================================================
 */

typedef struct {
    double complex tx_impedance;
    double complex rx_impedance;
    double         tx_gain_db;
    double         rx_gain_db;
    int            reciprocity_holds;
} nf_reciprocity_t;

typedef struct {
    double perturbation_error_pct;
    double probe_electrical_size;
    double probe_dut_separation;
    double recommended_min_distance;
} nf_perturbation_t;

/* ---------- C63.2 Standard: Antenna Factor (L2) ---------- */

typedef struct {
    double af_per_m;
    double af_db_per_m;
    double gain_dbi;
    double impedance_ohm;
} nf_antenna_factor_t;

typedef struct {
    double pf_e_per_m;
    double pf_h_a_per_vm;
    double calibration_uncertainty_db;
    double cross_polarization_db;
} nf_probe_factor_t;

/* ---------- CISPR 16-1-1: Measurement Receiver (L2) ---------- */

typedef struct {
    double freq_center_hz;
    double rbw_hz;
    double vbw_hz;
    double sweep_time_s;
    double noise_floor_dbm;
    double reference_level_dbm;
    int    detector_mode;
    int    averaging_count;
} nf_receiver_params_t;

typedef struct {
    double gain_db;
    double noise_figure_db;
    double p1db_dbm;
    double ip3_dbm;
    double input_impedance_ohm;
    double freq_min_hz;
    double freq_max_hz;
} nf_preamplifier_t;

/* ============================================================================
 * API Function Declarations
 * ============================================================================
 */

nf_region_t nf_region_classify(double r, double lambda, double D_max);
const char* nf_region_name(nf_region_t region);

void nf_probe_spec_init(nf_probe_spec_t *spec, nf_probe_type_t type);
int  nf_probe_spec_validate(const nf_probe_spec_t *spec);

double nf_antenna_factor_farfield(double freq_hz, double gain_dbi);
double nf_probe_factor_e_field(double v_out_V, double e_incident_Vpm);
double nf_probe_factor_h_field(double v_out_V, double h_incident_Apm);

int  nf_reciprocity_check(const nf_reciprocity_t *rcp);

double nf_perturbation_estimate(double probe_size_m,
                                 double freq_hz,
                                 double distance_m);

double nf_h_loop_sensitivity(double radius_m, int turns,
                              double freq_hz, double H_incident_Apm);

double nf_e_monopole_sensitivity(double effective_height_m,
                                  double E_incident_Vpm);

double nf_spatial_resolution(double probe_diameter_m,
                              double distance_m);

double nf_freq_to_wavelength(double freq_hz);
double nf_wavelength_to_freq(double lambda_m);

double nf_wave_impedance_nearfield(double r, double lambda,
                                    int is_electric_source);

double nf_wave_impedance_schelkunoff(double r, double lambda,
                                      double source_impedance_ohm);

void nf_probe_spec_print(const nf_probe_spec_t *spec);
void nf_scan_grid_print(const nf_scan_grid_t *grid);
void nf_field_sample_print(const nf_field_sample_t *sample);

#endif /* NF_PROBE_CORE_H */
