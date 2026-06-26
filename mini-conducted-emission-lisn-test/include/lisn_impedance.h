#ifndef LISN_IMPEDANCE_H
#define LISN_IMPEDANCE_H
#include "lisn_core.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LISN_TOPOLOGY_V,
    LISN_TOPOLOGY_DELTA,
    LISN_TOPOLOGY_T,
    LISN_TOPOLOGY_PI,
    LISN_TOPOLOGY_L_CLC
} lisn_topology_t;

typedef struct {
    double real_ohm;
    double imag_ohm;
} complex_z_t;

typedef struct {
    lisn_topology_t topology;
    double L_series_uh;
    double R_series_ohm;
    double R_shunt_ohm;
    double C_shunt_nf;
    double C_coupling_nf;
    double R_discharge_kohm;
    double L_parasitic_nh;
    double C_parasitic_pf;
    double R_core_loss_ohm;
    double skin_effect_coeff;
} lisn_network_model_t;

complex_z_t lisn_inductor_impedance(double L_uh, double R_dc_ohm, double C_par_pf, double f_hz);
complex_z_t lisn_parallel_z(complex_z_t z1, complex_z_t z2);
complex_z_t lisn_series_z(complex_z_t z1, complex_z_t z2);
complex_z_t lisn_v_network_impedance(const lisn_network_model_t *model, double f_hz);
complex_z_t lisn_delta_network_impedance(const lisn_network_model_t *model, double f_hz);
complex_z_t lisn_common_mode_impedance(const lisn_network_model_t *model, double f_hz);
complex_z_t lisn_differential_mode_impedance(const lisn_network_model_t *model, double f_hz);
double lisn_self_resonant_frequency(double L_uh, double C_par_pf);
void lisn_impedance_sweep(const lisn_network_model_t *model, double f_start_hz, double f_stop_hz, int n_points, lisn_impedance_state_t *results);
double lisn_coupling_cap_impedance(double C_uf, double f_hz);
void lisn_network_model_init_cispr22(lisn_network_model_t *model);
void lisn_impedance_tolerance_check(const lisn_network_model_t *model, double f_start_hz, double f_stop_hz, int n_points, double *max_dev_pct, double *worst_f_hz);

#ifdef __cplusplus
}
#endif

/* L4: Network parameter types for 2-port LISN analysis */
typedef struct {
    double s11_real, s11_imag;
    double s12_real, s12_imag;
    double s21_real, s21_imag;
    double s22_real, s22_imag;
} lisn_s_parameters_t;

/* L4: ABCD (transmission) matrix parameters for cascaded LISN elements */
typedef struct {
    double A_real, A_imag;
    double B_real, B_imag;
    double C_real, C_imag;
    double D_real, D_imag;
} lisn_abcd_matrix_t;

/* Additional impedance functions */
double lisn_reflection_coefficient(double z_load, double z0);
double lisn_vswr_from_reflection(double gamma);
double lisn_return_loss_db(const complex_z_t *z_load, double z0);
void lisn_s_to_abcd(const lisn_s_parameters_t *s, lisn_abcd_matrix_t *abcd);
void lisn_abcd_to_s(const lisn_abcd_matrix_t *abcd, lisn_s_parameters_t *s);
double lisn_max_power_transfer_impedance(const lisn_network_model_t *m, double f_hz);
double lisn_quality_factor(double L_uh, double R_series, double f_hz);

#endif
