#ifndef FIELD_PROPAGATION_H
#define FIELD_PROPAGATION_H

#include <stddef.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * L3: Field Propagation - Near/Far Field, Wave Impedance, Poynting Vector
 *
 * Reference: Balanis (2016) Ch 2
 *           Paul (2006) EMC Ch 3
 *           Jackson (1999) Classical Electrodynamics
 * ========================================================================= */

typedef enum {
    FP_STATIC_FIELD         = 0,
    FP_REACTIVE_NEAR_FIELD  = 1,
    FP_RADIATING_NEAR_FIELD = 2,
    FP_FAR_FIELD            = 3
} fp_field_region_t;

typedef struct {
    double re;
    double im;
} fp_complex_t;

typedef struct {
    fp_complex_t e_theta;
    fp_complex_t e_phi;
    fp_complex_t e_r;
    fp_complex_t h_theta;
    fp_complex_t h_phi;
    fp_complex_t h_r;
} fp_field_vector_t;

typedef struct {
    double current_moment_am;
    double frequency_hz;
    double position_r_m;
    double theta_rad;
    double phi_rad;
} fp_hertzian_dipole_t;

typedef struct {
    fp_field_vector_t fields;
    double            e_magnitude_vm;
    double            h_magnitude_am;
    double            wave_impedance_ohm;
    double            poynting_vector_wm2;
    double            total_radiated_power_w;
    fp_field_region_t region;
} fp_field_result_t;

typedef struct {
    double loop_area_m2;
    size_t num_turns;
    double current_a;
    double frequency_hz;
} fp_loop_antenna_t;

void fp_hertzian_dipole_field(const fp_hertzian_dipole_t *dipole,
                               fp_field_result_t *result);
void fp_hertzian_dipole_far_field(const fp_hertzian_dipole_t *dipole,
                                   fp_field_result_t *result);
void fp_loop_antenna_field(const fp_loop_antenna_t *loop,
                            double r, double theta,
                            fp_field_result_t *result);
fp_field_region_t fp_classify_region(double r, double D, double frequency_hz);
double fp_wave_impedance_magnitude(double r, double frequency_hz);
double fp_extrapolate_distance(double e_field_vm, double distance_known,
                                double distance_new, double frequency_hz,
                                int source_type);
double fp_poynting_from_efield(double e_field_vm);
double fp_free_space_impedance(void);
double fp_wavenumber(double frequency_hz);
double fp_field_from_power(double power_tx_w, double gain_tx_linear,
                            double distance_m);

#ifdef __cplusplus
}
#endif

#endif /* FIELD_PROPAGATION_H */