#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <complex.h>
#include <string.h>
#include "../include/nf_probe_core.h"
#include "../include/nf_field_theory.h"
#include "../include/nf_probe_calibration.h"
#include "../include/nf_scanning.h"
#include "../include/nf_signal_processing.h"
#include "../include/nf_diagnostic.h"

static int passed = 0, failed = 0;
#define T(n) printf("  TEST: %-50s ", n)
#define OK() do { printf("PASS\n"); passed++; } while(0)
#define BAD(m) do { printf("FAIL: %s\n", m); failed++; } while(0)
#define C(c) do { if(c) OK(); else BAD(#c); } while(0)
#define CN(a,b,t) do { if(fabs((a)-(b))<(t)) OK(); else {printf("FAIL: %.4e vs %.4e\n",(a),(b));failed++;} } while(0)

void t1(void) { T("L1: Reactive near-field"); C(nf_region_classify(0.001,1.0,0.5)==NF_REGION_REACTIVE_NEAR); }
void t2(void) { T("L1: Fraunhofer"); C(nf_region_classify(10,0.3,0.5)==NF_REGION_FRAUNHOFER); }
void t3(void) { T("L1: Probe init"); nf_probe_spec_t s; nf_probe_spec_init(&s,NF_PROBE_H_FIELD_LOOP); C(s.type==NF_PROBE_H_FIELD_LOOP&&nf_probe_spec_validate(&s)==1); }
void t4(void) { T("L1: Freq->wavelength"); CN(nf_freq_to_wavelength(1e9),0.299792458,1e-6); }
void t5(void) { T("L1: Faraday sensitivity"); double V=nf_h_loop_sensitivity(0.002,3,100e6,1.0); C(V>1e-6); }
void t6(void) { T("L2: Antenna factor"); CN(nf_antenna_factor_farfield(100e6,2.0),20*log10(100)-29.77-2.0,0.5); }
void t7(void) { T("L2: Near-field Zw electric"); double Z=nf_wave_impedance_nearfield(0.01,3.0,1); C(Z>ETA_0); }
void t8(void) { T("L2: Near-field Zw magnetic"); double Z=nf_wave_impedance_nearfield(0.01,3.0,0); C(Z<ETA_0); }
void t9(void) { T("L2: Reciprocity"); nf_reciprocity_t r={50,50,0,0,1}; C(nf_reciprocity_check(&r)==1); }
void t10(void) { T("L3: Vector add"); nf_complex_vector_t a={1,2,3},b={4,5,6},c=nf_cvec_add(a,b); CN(creal(c.x),5.0,1e-10); }
void t11(void) { T("L3: Dot product"); nf_complex_vector_t a={1,2,3}; CN(creal(nf_cvec_dot(a,a)),14.0,1e-10); }
void t12(void) { T("L3: Green function"); nf_greens_dyadic_t gf; nf_real_vector_t s={0,0,0},o={0.1,0,0}; C(nf_greens_dyadic_compute(&gf,&s,&o,1e9)==0&&cabs(gf.scalar_green)>1e-10); }
void t13(void) { T("L4: PEC BC"); nf_boundary_condition_t bc={NF_BC_PEC,{0,0,1}}; nf_complex_vector_t E={0,0,0},H={1,0,0}; C(nf_boundary_condition_check(&bc,&E,&H,1e-6)==1); }
void t14(void) { T("L4: Image theory"); nf_image_theory_t img; nf_real_vector_t s={0,0,0.5},n={0,0,1}; nf_image_theory_compute(&img,&s,&n,0,0); CN(img.image_position.z,-0.5,1e-10); }
void t15(void) { T("L5: TEM cell"); double E,H; nf_tem_cell_field(1e-3,0.05,0.1,50,&E,&H); C(E>0&&H>0); }
void t16(void) { T("L5: Microstrip"); double ee,Z0; nf_microstrip_effective_permittivity(1e-3,0.5e-3,4.4,35e-6,&ee,&Z0); C(ee>1&&Z0>20); }
void t17(void) { T("L5: 3-Antenna cal"); nf_three_antenna_cal_t r; nf_three_antenna_calibrate(-30,-30,-30,1,1,1,1e9,&r); C(r.converged==1); }
void t18(void) { T("L5: Nyquist"); nf_scan_grid_t g={0,0.1,0,0.1,0.001,11,11,0.01,0.01}; nf_nyquist_check_t nc; nf_nyquist_check(&g,1e9,&nc); C(nc.nyquist_spacing_m>0); }
void t19(void) { T("L5: FFT"); nf_fft_plan_t p; nf_fft_plan_create(&p,8,0); double complex d[8]={1,0,0,0,0,0,0,0}; nf_fft_execute(&p,d); double s=0; for(int i=0;i<8;i++)s+=creal(d[i]); CN(creal(d[0]),1.0,1e-10); nf_fft_plan_destroy(&p); }
void t20(void) { T("L5: Hann window"); double w[64]; nf_window_generate(w,64,NF_WINDOW_HANN,0); CN(w[0],0.0,1e-10); CN(w[32],1.0,0.1); }
void t21(void) { T("L5: IIR filter"); nf_iir_filter_t f; nf_iir_butterworth_design(&f,NF_FILTER_LOWPASS,0,1e3,10e3,2); double in[]={1,0,0,0,0},out[5]; nf_iir_filter_apply(&f,in,out,5); C(out[0]>0); nf_iir_filter_free(&f); }
void t22(void) { T("L5: Phase unwrap"); double w[]={0.5,2.5,-2.0,0.3}; nf_phase_unwrap_t r; nf_phase_unwrap(w,4,&r); C(r.num_points==4); nf_phase_unwrap_free(&r); }
void t23(void) { T("L5: EMI receiver"); nf_emi_receiver_model_t rx={100e6,120e3,300e3}; double sig[1000]; for(int i=0;i<1000;i++)sig[i]=sin(2*M_PI*i/100); nf_emi_receiver_process(&rx,sig,1000,1e6); C(rx.peak_output>0); }
void t24(void) { T("L5: Noise analysis"); double sig[1000]; for(int i=0;i<1000;i++)sig[i]=sin(2*M_PI*i/100); nf_noise_analysis_t na; nf_noise_analyze(sig,1000,1e3,30,5,&na); C(na.noise_floor_dbm<-100&&na.snr_db>0); }
void t25(void) { T("L6: Diff pair"); nf_diff_pair_radiation_t dp={0.5e-3,0.2e-3,0.05,1.6e-3,4.4,100,0.1,1.0,100e6}; nf_diff_pair_radiation_estimate(&dp); C(dp.common_mode_radiation_dbuVpm!=0); }
void t26(void) { T("L6: Slot coupling"); nf_slot_coupling_t sc={0.05,0.001,0.035e-3,0.01,1e9}; nf_slot_coupling_analyze(&sc); C(sc.resonance_freq_hz>0); }
void t27(void) { T("L6: Decoupling cap"); nf_decoupling_cap_t dc={100e-9,0.5e-9,0.01,0.3e-9,0,0,100e6,0,5}; nf_decoupling_impedance(&dc); C(dc.self_resonant_freq_hz>0&&dc.improvement_db>0); }
void t28(void) { T("L6: Emission mech"); nf_field_sample_t s; memset(&s,0,sizeof(s)); s.Ez=1; s.Hy=0.001; nf_emission_mechanism_t m; nf_emission_mechanism_classify(&s,100e6,&m); C(strlen(m.classification)>0); }
void t29(void) { T("L6: Field stats"); double d[]={1,2,3,4,5,6,7,8,9,10}; nf_field_statistics_t st; nf_field_statistics_compute(d,10,&st); CN(st.mean_value,5.5,1e-6); CN(st.median_value,6.0,1e-6); }
void t30(void) { T("L7: PCB diag"); nf_pcb_diagnostic_t p; nf_pcb_diagnostic_init(&p,"T","r1",0.1,0.1); C(strcmp(p.pcb_name,"T")==0); nf_pcb_diagnostic_free(&p); }
void t31(void) { T("L7: IC diag"); nf_ic_diagnostic_t ic; nf_ic_diagnostic_init(&ic,"S32",168e6,3.3,0.1); C(strcmp(ic.ic_part_number,"S32")==0); nf_ic_diagnostic_free(&ic); }
void t32(void) { T("L8: Stochastic"); nf_stochastic_field_t sf; C(nf_stochastic_field_init(&sf,100,10)==0); nf_stochastic_field_free(&sf); }
void t33(void) { T("L8: Bayesian"); nf_bayesian_localizer_t bl; nf_scan_grid_t g={0,0.1,0,0.1,0.001,5,5,0.025,0.025}; nf_field_sample_t m; memset(&m,0,sizeof(m)); C(nf_bayesian_localize_init(&bl,&g,&m,10)==0); nf_bayesian_localize_free(&bl); }
static double mc_m(const double *p){return p[0]+p[1]*2;}
void t34(void) { T("L8: Monte Carlo"); double nom[]={1,2},sd[]={0.1,0.1},mn,st,ci; C(nf_monte_carlo_uncertainty(nom,sd,2,mc_m,1000,&mn,&st,&ci)==0); CN(mn,5.0,0.3); }

int main(void) {
    printf("=== NF Probe Diagnostic Test Suite ===\n");
    t1();t2();t3();t4();t5();t6();t7();t8();t9();t10();
    t11();t12();t13();t14();t15();t16();t17();t18();t19();t20();
    t21();t22();t23();t24();t25();t26();t27();t28();t29();t30();
    t31();t32();t33();t34();
    printf("=== %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
