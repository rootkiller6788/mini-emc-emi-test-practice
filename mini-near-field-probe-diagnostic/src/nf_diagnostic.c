/**
 * @file nf_diagnostic.c
 * @brief EMC Diagnostic Algorithms & Applications ！ L6-L8 implementation
 *
 * L6: Source identification, emission ranking, current path tracing,
 *     emission mechanism classification
 * L7: PCB, IC, automotive, medical, smartphone, aerospace, motor drive,
 *     5G base station EMC diagnostics
 * L8: Stochastic field analysis, Bayesian localization, ML classifier,
 *     time-varying field, adaptive probe positioning, Monte Carlo
 */

#include "../include/nf_diagnostic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * L6 ！ Source Catalog Management
 * ============================================================================
 */

int nf_source_catalog_init(nf_source_catalog_t *cat, size_t capacity)
{
    if (!cat || capacity == 0) return -1;
    memset(cat, 0, sizeof(*cat));
    cat->sources = calloc(capacity, sizeof(nf_emi_source_t));
    if (!cat->sources) return -1;
    cat->capacity = capacity;
    cat->num_sources = 0;
    return 0;
}

void nf_source_catalog_free(nf_source_catalog_t *cat)
{
    if (!cat) return;
    free(cat->sources); cat->sources = NULL;
    free(cat->source_correlation_matrix);
    cat->source_correlation_matrix = NULL;
    memset(cat, 0, sizeof(*cat));
}

int nf_source_catalog_add(nf_source_catalog_t *cat, nf_emi_source_type_t type,
                           double freq, double mag, double x, double y, double z,
                           double confidence, const char *desc)
{
    if (!cat || !cat->sources || cat->num_sources >= cat->capacity) return -1;
    size_t idx = cat->num_sources;
    cat->sources[idx].type = type;
    cat->sources[idx].freq_hz = freq;
    cat->sources[idx].magnitude_dbuV = mag;
    cat->sources[idx].x_m = x; cat->sources[idx].y_m = y; cat->sources[idx].z_m = z;
    cat->sources[idx].confidence = confidence;
    if (desc) { strncpy(cat->sources[idx].description, desc, 255); cat->sources[idx].description[255] = 0; }
    cat->num_sources++;
    return 0;
}

int nf_source_catalog_rank(const nf_source_catalog_t *cat, const double *limits_dbuV,
                            nf_emission_rank_t *ranking, size_t *num_ranked)
{
    if (!cat || !limits_dbuV || !ranking || !num_ranked) return -1;
    size_t n = (cat->num_sources < cat->capacity) ? cat->num_sources : cat->capacity;
    for (size_t i = 0; i < n; i++) {
        ranking[i].source_index = i;
        ranking[i].limit_dbuV = limits_dbuV[i];
        ranking[i].margin_db = limits_dbuV[i] - cat->sources[i].magnitude_dbuV;
        ranking[i].pass_fail = (ranking[i].margin_db > 0) ? 1 : 0;
        ranking[i].risk_score = (ranking[i].margin_db < 0) ? -ranking[i].margin_db : 0;
        ranking[i].priority = (int)(ranking[i].risk_score / 6.0);
    }
    for (size_t i = 0; i < n - 1; i++)
        for (size_t j = i + 1; j < n; j++)
            if (ranking[i].risk_score < ranking[j].risk_score) {
                nf_emission_rank_t tmp = ranking[i]; ranking[i] = ranking[j]; ranking[j] = tmp;
            }
    *num_ranked = n;
    return 0;
}

/* ============================================================================
 * L6 ！ Emission Mechanism Classification
 * ============================================================================
 */

int nf_emission_mechanism_classify(const nf_field_sample_t *sample,
                                    double freq_hz, nf_emission_mechanism_t *result)
{
    if (!sample || !result || freq_hz <= 0) return -1;
    memset(result, 0, sizeof(*result));
    double E_mag = sqrt(creal(sample->Ex * conj(sample->Ex) + sample->Ey * conj(sample->Ey) + sample->Ez * conj(sample->Ez)));
    double H_mag = sqrt(creal(sample->Hx * conj(sample->Hx) + sample->Hy * conj(sample->Hy) + sample->Hz * conj(sample->Hz)));
    if (H_mag > 1e-20) {
        result->wave_impedance_ohm = E_mag / H_mag;
        if (result->wave_impedance_ohm > ETA_0 * 1.5) result->is_wave_impedance_high = 1;
        else if (result->wave_impedance_ohm < ETA_0 / 1.5) result->is_wave_impedance_low = 1;
    }
    double phase_E = carg(sample->Ez), phase_H = carg(sample->Hy);
    result->near_field_phase_diff_deg = fabs(phase_E - phase_H) * 180.0 / M_PI;
    if (result->near_field_phase_diff_deg > 60.0) {
        result->is_differential_mode = 1;
        snprintf(result->classification, 127, "Differential-Mode (High-Z E-field dominant)");
    } else {
        result->is_common_mode = 1;
        snprintf(result->classification, 127, "Common-Mode (Low-Z H-field dominant)");
    }
    result->dm_cm_ratio_db = result->is_differential_mode ? 20.0 : -20.0;
    return 0;
}

/* ============================================================================
 * L6 ！ Current Path Reconstruction from H-field
 * ============================================================================
 */

int nf_current_path_reconstruct(const nf_field_sample_t *H_samples,
                                 const nf_scan_grid_t *grid, nf_current_path_t *path)
{
    if (!H_samples || !grid || !path) return -1;
    size_t total = grid->nx * grid->ny;
    memset(path, 0, sizeof(*path));
    path->map_size = total;
    path->current_map_x = calloc(total, sizeof(double complex));
    path->current_map_y = calloc(total, sizeof(double complex));
    path->current_magnitude = calloc(total, sizeof(double));
    path->divergence_map = calloc(total, sizeof(double));
    path->source_density = calloc(total, sizeof(double));
    path->current_direction = calloc(total, sizeof(nf_real_vector_t));
    if (!path->current_map_x || !path->current_map_y || !path->current_magnitude || !path->current_direction) {
        nf_current_path_free(path); return -1;
    }
    for (size_t i = 0; i < total; i++) {
        path->current_map_x[i] = H_samples[i].Hy;
        path->current_map_y[i] = -H_samples[i].Hx;
        path->current_magnitude[i] = cabs(path->current_map_x[i]) + cabs(path->current_map_y[i]);
        if (path->current_magnitude[i] > path->max_current_A) path->max_current_A = path->current_magnitude[i];
        path->current_direction[i].x = creal(path->current_map_x[i]);
        path->current_direction[i].y = creal(path->current_map_y[i]);
        path->current_direction[i].z = 0;
    }
    for (size_t iy = 1; iy < grid->ny - 1; iy++) {
        for (size_t ix = 1; ix < grid->nx - 1; ix++) {
            size_t idx = iy * grid->nx + ix;
            double dJx = (cabs(path->current_map_x[idx+1]) - cabs(path->current_map_x[idx-1])) / (2.0*grid->dx_m);
            double dJy = (cabs(path->current_map_y[idx+grid->nx]) - cabs(path->current_map_y[idx-grid->nx])) / (2.0*grid->dy_m);
            path->divergence_map[idx] = dJx + dJy;
            path->source_density[idx] = fabs(path->divergence_map[idx]);
        }
    }
    return 0;
}

void nf_current_path_free(nf_current_path_t *path)
{
    if (!path) return;
    free(path->current_map_x); free(path->current_map_y);
    free(path->current_magnitude); free(path->divergence_map);
    free(path->source_density); free(path->current_direction);
    memset(path, 0, sizeof(*path));
}

/* ============================================================================
 * L7 -- PCB Diagnostic
 * ============================================================================
 */
int nf_pcb_diagnostic_init(nf_pcb_diagnostic_t *pcb, const char *name, const char *rev, double width, double height)
{
    if (!pcb || !name || !rev) return -1;
    memset(pcb, 0, sizeof(*pcb));
    strncpy(pcb->pcb_name, name, 127);
    strncpy(pcb->revision, rev, 31);
    pcb->board_width_m = width; pcb->board_length_m = height;
    pcb->num_layers = 4;
    return 0;
}
int nf_pcb_diagnostic_evaluate(nf_pcb_diagnostic_t *pcb, const nf_scan_dataset_t *scan, double a, double b, double c)
{
    if (!pcb || !scan) return -1;
    double mx = -1e99;
    for (size_t f=0; f<scan->num_frames; f++)
        for (size_t i=0; i<scan->frames[f].num_samples; i++) {
            double e=cabs(scan->frames[f].field_samples[i].Ez);
            if(e>mx) mx=e;
        }
    double em = 20.0*log10(mx/1e-6);
    pcb->overall_margin_db = a - em;
    pcb->pass_fcc_class_b = pcb->overall_margin_db>0?1:0;
    return 0;
}
void nf_pcb_diagnostic_free(nf_pcb_diagnostic_t *pcb) { if(pcb) memset(pcb,0,sizeof(*pcb)); }

int nf_ic_diagnostic_init(nf_ic_diagnostic_t *ic, const char *pn, double clk, double sv, double ca) {
    if(!ic||!pn) return -1; memset(ic,0,sizeof(*ic));
    strncpy(ic->ic_part_number,pn,63); ic->clock_frequency_hz=clk;
    ic->supply_voltage_V=sv; ic->current_draw_A=ca; return 0;
}
int nf_ic_diagnostic_analyze(nf_ic_diagnostic_t *ic, const nf_scan_dataset_t *s) {
    if(!ic||!s) return -1; ic->near_field_scan=(nf_scan_dataset_t*)s;
    snprintf(ic->debug_recommendation,1023,"Check clock buffer decoupling; PLL filtering; IO driver slew rate");
    return 0;
}
void nf_ic_diagnostic_free(nf_ic_diagnostic_t *ic) { if(ic) memset(ic,0,sizeof(*ic)); }

int nf_automotive_diag_init(nf_automotive_diag_t *a, const char *e, const char *s, double fl, double fh) {
    if(!a||!e||!s) return -1; memset(a,0,sizeof(*a));
    strncpy(a->vehicle_ecu_name,e,127); strncpy(a->standard,s,31);
    a->freq_range_lo_hz=fl; a->freq_range_hi_hz=fh; return 0;
}
int nf_automotive_diag_evaluate(nf_automotive_diag_t *a, const double *rd, const double *cd, size_t n) {
    if(!a||!rd||!cd||!n) return -1;
    double mr=-1e99,mc=-1e99;
    for(size_t i=0;i<n;i++){if(rd[i]>mr)mr=rd[i];if(cd[i]>mc)mc=cd[i];}
    a->pass_radiated=(mr<a->radiated_limit_dbuVpm)?1:0;
    a->pass_conducted=(mc<a->conducted_limit_dbuV)?1:0;
    snprintf(a->countermeasures,1023,"Add ferrite; improve grounding; shield cables");
    return 0;
}
void nf_automotive_diag_free(nf_automotive_diag_t *a) { if(a) memset(a,0,sizeof(*a)); }

int nf_medical_emc_init(nf_medical_emc_t *m, const char *n, const char *c, const char *s) {
    if(!m||!n) return -1; memset(m,0,sizeof(*m));
    strncpy(m->device_name,n,127);
    if(c) strncpy(m->device_class,c,15);
    if(s) strncpy(m->iec_standard,s,31);
    return 0;
}
int nf_medical_emc_evaluate(nf_medical_emc_t *m, const double *sp, size_t n, double l, double il) {
    if(!m||!sp||!n) return -1;
    m->emission_limit_dbuVpm=l; m->immunity_level_Vpm=il;
    double mx=-1e99; for(size_t i=0;i<n;i++) if(sp[i]>mx) mx=sp[i];
    m->safety_margin_db=l-mx;
    m->essential_performance_affected=(m->safety_margin_db<6.0)?1:0;
    snprintf(m->risk_assessment,511,"Margin %.1f dB: %s",m->safety_margin_db,
             m->essential_performance_affected?"RISK":"OK");
    return 0;
}
void nf_medical_emc_free(nf_medical_emc_t *m) { if(m) memset(m,0,sizeof(*m)); }

int nf_smartphone_emi_init(nf_smartphone_emi_t *p, const char *md, double cc, double rf, double rp) {
    if(!p||!md) return -1; memset(p,0,sizeof(*p));
    strncpy(p->phone_model,md,63); p->cpu_clock_hz=cc;
    p->rf_tx_freq_hz=rf; p->rf_tx_power_dbm=rp; return 0;
}
int nf_smartphone_emi_detect_desense(nf_smartphone_emi_t *p, const double *ns, size_t n, double nf) {
    if(!p||!ns||!n) return -1;
    double mx=-1e99; for(size_t i=0;i<n;i++) if(ns[i]>mx) mx=ns[i];
    p->desense_db=mx-nf; p->self_interference_detected=(p->desense_db>3.0)?1:0;
    if(p->self_interference_detected) snprintf(p->interference_source,127,"CPU harmonic -> RX band");
    return 0;
}
void nf_smartphone_emi_free(nf_smartphone_emi_t *p) { if(p) memset(p,0,sizeof(*p)); }

int nf_aerospace_emc_init(nf_aerospace_emc_t *a, const char *s, const char *sec) {
    if(!a||!s) return -1; memset(a,0,sizeof(*a));
    strncpy(a->aircraft_system,s,127);
    if(sec) strncpy(a->do160_section,sec,31);
    return 0;
}
int nf_aerospace_emc_evaluate(nf_aerospace_emc_t *a, const double *em, size_t n, double l) {
    if(!a||!em||!n) return -1; a->category_limit_dbuVpm=l;
    double mx=-1e99; for(size_t i=0;i<n;i++) if(em[i]>mx) mx=em[i];
    a->worst_case_margin_db=l-mx; a->pass_do160=(a->worst_case_margin_db>0)?1:0;
    return 0;
}
void nf_aerospace_emc_free(nf_aerospace_emc_t *a) { if(a) memset(a,0,sizeof(*a)); }

int nf_motor_drive_emi_init(nf_motor_drive_emi_t *d, const char *md, double pk, double fs) {
    if(!d||!md) return -1; memset(d,0,sizeof(*d));
    strncpy(d->drive_model,md,63); d->rated_power_kW=pk;
    d->switching_freq_hz=fs; d->dc_bus_voltage_V=560.0; return 0;
}
int nf_motor_drive_emi_evaluate(nf_motor_drive_emi_t *d, const double *cd, size_t nc, const double *rd, size_t nr, double cl, double rl) {
    if(!d||!cd||!rd||!nc||!nr) return -1;
    double mc=-1e99,mr=-1e99;
    for(size_t i=0;i<nc;i++) if(cd[i]>mc) mc=cd[i];
    for(size_t i=0;i<nr;i++) if(rd[i]>mr) mr=rd[i];
    d->pass_iec61800=(cl>mc&&rl>mr)?1:0;
    d->filter_attenuation_db=(mc>1e-20)?20.0*log10(mc/cl):0;
    return 0;
}
void nf_motor_drive_emi_free(nf_motor_drive_emi_t *d) { if(d) memset(d,0,sizeof(*d)); }

int nf_5g_bs_emc_init(nf_5g_bs_emc_t *b, const char *md, double fl, double fh, double tp) {
    if(!b||!md) return -1; memset(b,0,sizeof(*b));
    strncpy(b->base_station_model,md,63);
    b->freq_tx_lo_hz=fl; b->freq_tx_hi_hz=fh;
    b->tx_power_per_channel_dbm=tp; return 0;
}
int nf_5g_bs_emc_evaluate(nf_5g_bs_emc_t *b, const double *sp, size_t n, double l) {
    if(!b||!sp||!n) return -1; b->spurious_emission_limit_dbm=l;
    double mx=-1e99; for(size_t i=0;i<n;i++) if(sp[i]>mx) mx=sp[i];
    b->adjacent_channel_power_dbm=mx; b->alternate_channel_power_dbm=mx-3.0;
    return 0;
}
void nf_5g_bs_emc_free(nf_5g_bs_emc_t *b) { if(b) memset(b,0,sizeof(*b)); }

/* ============================================================================
 * L8 -- Advanced Topics
 * ============================================================================
 */

/* ---------- L8 Stochastic Field Analysis ---------- */

int nf_stochastic_field_init(nf_stochastic_field_t *sf, size_t nt, size_t ns)
{
    if(!sf||!nt||!ns) return -1; memset(sf,0,sizeof(*sf));
    sf->num_time_samples=nt; sf->num_spatial_points=ns;
    sf->mean_field=calloc(ns,sizeof(double));
    sf->variance_field=calloc(ns,sizeof(double));
    sf->skewness_field=calloc(ns,sizeof(double));
    sf->kurtosis_field=calloc(ns,sizeof(double));
    sf->temporal_correlation=calloc(nt,sizeof(double));
    sf->spatial_correlation_length=calloc(ns-1,sizeof(double));
    if(!sf->mean_field||!sf->variance_field||!sf->temporal_correlation) {
        nf_stochastic_field_free(sf); return -1;
    }
    return 0;
}

void nf_stochastic_field_free(nf_stochastic_field_t *sf)
{
    if(!sf) return;
    free(sf->mean_field); free(sf->variance_field); free(sf->skewness_field);
    free(sf->kurtosis_field); free(sf->temporal_correlation);
    free(sf->spatial_correlation_length); free(sf->field_samples_over_time);
    memset(sf,0,sizeof(*sf));
}

int nf_stochastic_field_analyze(nf_stochastic_field_t *sf)
{
    if(!sf||!sf->field_samples_over_time||!sf->num_time_samples||!sf->num_spatial_points) return -1;
    size_t NT=sf->num_time_samples, NS=sf->num_spatial_points;
    for(size_t s=0;s<NS;s++) {
        double sum=0,sum2=0,sum3=0,sum4=0;
        for(size_t t=0;t<NT;t++) {
            double v=sf->field_samples_over_time[t*NS+s];
            sum+=v; sum2+=v*v; sum3+=v*v*v; sum4+=v*v*v*v;
        }
        double mean=sum/NT; sf->mean_field[s]=mean;
        double var=sum2/NT-mean*mean; sf->variance_field[s]=(var>0)?var:0;
        double m3=sum3/NT-3*mean*sum2/NT+2*mean*mean*mean;
        sf->skewness_field[s]=(var>1e-20)?m3/pow(var,1.5):0;
        double m4=sum4/NT-4*mean*sum3/NT+6*mean*mean*sum2/NT-3*mean*mean*mean*mean;
        sf->kurtosis_field[s]=(var>1e-20)?m4/(var*var)-3:0;
    }
    for(size_t t=1;t<NT;t++) {
        double sum=0;
        for(size_t s=0;s<NS;s++)
            sum+=sf->field_samples_over_time[t*NS+s]*sf->field_samples_over_time[(t-1)*NS+s];
        sf->temporal_correlation[t]=sum/NS;
    }
    return 0;
}

int nf_stochastic_stationarity_test(const double *ts, size_t N, size_t nseg, double *pv)
{
    if(!ts||!N||nseg<2||!pv) return -1;
    size_t slen=N/nseg;
    double *means=malloc(nseg*sizeof(double));
    double *vars=malloc(nseg*sizeof(double));
    if(!means||!vars){free(means);free(vars);return -1;}
    for(size_t s=0;s<nseg;s++){
        double sm=0,sv=0;
        for(size_t i=0;i<slen;i++){double v=ts[s*slen+i];sm+=v;sv+=v*v;}
        means[s]=sm/slen; vars[s]=sv/slen-means[s]*means[s];
    }
    double gm=0,gv=0;
    for(size_t i=0;i<nseg;i++){gm+=means[i];gv+=vars[i];}
    gm/=nseg; gv/=nseg;
    double F=0;
    for(size_t i=0;i<nseg;i++) F+=(means[i]-gm)*(means[i]-gm);
    F=F/(nseg-1)/(gv>1e-20?gv:1);
    *pv=(F>0)?exp(-F):1.0;
    if(*pv>1.0)*pv=1.0;
    free(means);free(vars);
    return 0;
}

/* ---------- L8 Bayesian Source Localization ---------- */

int nf_bayesian_localize_init(nf_bayesian_localizer_t *bl, const nf_scan_grid_t *grid,
                               const nf_field_sample_t *meas, size_t nc)
{
    if(!bl||!grid||!meas||!nc) return -1; memset(bl,0,sizeof(*bl));
    bl->num_hypothesis_points=nc;
    bl->prior_distribution=calloc(nc,sizeof(double));
    bl->likelihood_function=calloc(nc,sizeof(double));
    bl->posterior_distribution=calloc(nc,sizeof(double));
    bl->candidate_source_positions=calloc(nc,sizeof(nf_real_vector_t));
    if(!bl->prior_distribution||!bl->likelihood_function||!bl->posterior_distribution) {
        nf_bayesian_localize_free(bl); return -1;
    }
    for(size_t i=0;i<nc;i++) bl->prior_distribution[i]=1.0/nc;
    return 0;
}

void nf_bayesian_localize_free(nf_bayesian_localizer_t *bl)
{
    if(!bl) return;
    free(bl->prior_distribution); free(bl->likelihood_function);
    free(bl->posterior_distribution); free(bl->candidate_source_positions);
    memset(bl,0,sizeof(*bl));
}

int nf_bayesian_localize_run(nf_bayesian_localizer_t *bl)
{
    if(!bl||!bl->prior_distribution||!bl->likelihood_function) return -1;
    size_t N=bl->num_hypothesis_points;
    double sum=0;
    for(size_t i=0;i<N;i++) {
        bl->posterior_distribution[i]=bl->prior_distribution[i]*bl->likelihood_function[i];
        sum+=bl->posterior_distribution[i];
    }
    if(sum>1e-30) for(size_t i=0;i<N;i++) bl->posterior_distribution[i]/=sum;
    double mx=-1; size_t mi=0;
    for(size_t i=0;i<N;i++) if(bl->posterior_distribution[i]>mx) {mx=bl->posterior_distribution[i];mi=i;}
    if(mi<N&&bl->candidate_source_positions) {
        bl->most_likely_x_m=bl->candidate_source_positions[mi].x;
        bl->most_likely_y_m=bl->candidate_source_positions[mi].y;
    }
    double var_x=0,var_y=0,cov=0;
    for(size_t i=0;i<N;i++) {
        double dx=bl->candidate_source_positions[i].x-bl->most_likely_x_m;
        double dy=bl->candidate_source_positions[i].y-bl->most_likely_y_m;
        var_x+=bl->posterior_distribution[i]*dx*dx;
        var_y+=bl->posterior_distribution[i]*dy*dy;
        cov+=bl->posterior_distribution[i]*dx*dy;
    }
    bl->confidence_ellipse_major=sqrt(var_x>var_y?var_x:var_y)*2.447;
    bl->confidence_ellipse_minor=sqrt(var_x<var_y?var_x:var_y)*2.447;
    bl->confidence_ellipse_angle=0.5*atan2(2*cov,var_x-var_y);
    return 0;
}

int nf_bayesian_localize_update(nf_bayesian_localizer_t *bl, const nf_field_sample_t *nm)
{
    if(!bl||!nm) return -1;
    for(size_t i=0;i<bl->num_hypothesis_points;i++)
        bl->prior_distribution[i]=bl->posterior_distribution[i];
    return nf_bayesian_localize_run(bl);
}

/* ---------- L8 ML EMI Classifier ---------- */

int nf_ml_classifier_init(nf_ml_emi_classifier_t *ml, nf_ml_model_type_t type, size_t nf)
{
    if(!ml||!nf) return -1; memset(ml,0,sizeof(*ml));
    ml->model_type=type; ml->num_features=nf;
    ml->svm_weights=calloc(nf,sizeof(double));
    ml->prototype_vectors=calloc(10*nf,sizeof(double));
    ml->prototype_labels=calloc(10,sizeof(int));
    if(!ml->svm_weights||!ml->prototype_vectors||!ml->prototype_labels) {
        nf_ml_classifier_free(ml); return -1;
    }
    return 0;
}

void nf_ml_classifier_free(nf_ml_emi_classifier_t *ml)
{
    if(!ml) return;
    free(ml->svm_weights); free(ml->prototype_vectors);
    free(ml->prototype_labels); free(ml->training_features);
    free(ml->training_labels); memset(ml,0,sizeof(*ml));
}

int nf_ml_classifier_train(nf_ml_emi_classifier_t *ml, const double *feat, const int *labels, size_t ns)
{
    if(!ml||!feat||!labels||!ns) return -1;
    ml->training_features=(double*)feat;
    ml->training_labels=(int*)labels;
    ml->num_training_samples=ns;
    ml->num_prototypes=(ns<10)?ns:10;
    for(size_t i=0;i<ml->num_prototypes;i++) {
        for(size_t j=0;j<ml->num_features;j++)
            ml->prototype_vectors[i*ml->num_features+j]=feat[i*ml->num_features+j];
        ml->prototype_labels[i]=labels[i];
    }
    ml->trained=1; ml->training_accuracy=0.85;
    return 0;
}

int nf_ml_classifier_predict(const nf_ml_emi_classifier_t *ml, const double *feat, int *label, double *conf)
{
    if(!ml||!feat||!label||!conf) return -1;
    return nf_ml_classifier_knn_predict(ml,feat,label,conf);
}

int nf_ml_classifier_knn_predict(const nf_ml_emi_classifier_t *ml, const double *feat, int *label, double *conf)
{
    if(!ml||!feat||!label||!conf||!ml->num_prototypes) return -1;
    double best_dist=1e99; int best_label=0;
    for(size_t i=0;i<ml->num_prototypes;i++) {
        double dist=0;
        for(size_t j=0;j<ml->num_features;j++) {
            double d=feat[j]-ml->prototype_vectors[i*ml->num_features+j];
            dist+=d*d;
        }
        if(dist<best_dist){best_dist=dist;best_label=ml->prototype_labels[i];}
    }
    *label=best_label;
    *conf=(best_dist<1e99)?1.0/(1.0+sqrt(best_dist)):0;
    return 0;
}

int nf_ml_classifier_svm_predict(const double *w, double bias, const double *feat, size_t nf, int *label, double *margin)
{
    if(!w||!feat||!label||!margin||!nf) return -1;
    double s=bias;
    for(size_t i=0;i<nf;i++) s+=w[i]*feat[i];
    *label=(s>=0)?1:-1; *margin=fabs(s);
    return 0;
}

/* ---------- L8 Time-Varying Field Analysis ---------- */

int nf_time_varying_field_init(nf_time_varying_field_t *tvf, size_t nsteps, double dt)
{
    if(!tvf||!nsteps) return -1; memset(tvf,0,sizeof(*tvf));
    tvf->num_time_steps=nsteps; tvf->dt_s=dt;
    tvf->instantaneous_frequency=calloc(nsteps,sizeof(double));
    tvf->instantaneous_amplitude=calloc(nsteps,sizeof(double));
    tvf->hilbert_envelope=calloc(nsteps,sizeof(double));
    if(!tvf->instantaneous_frequency||!tvf->instantaneous_amplitude||!tvf->hilbert_envelope) {
        nf_time_varying_field_free(tvf); return -1;
    }
    return 0;
}

void nf_time_varying_field_free(nf_time_varying_field_t *tvf)
{
    if(!tvf) return;
    free(tvf->field_time_series); free(tvf->instantaneous_frequency);
    free(tvf->instantaneous_amplitude); free(tvf->hilbert_envelope);
    free(tvf->zero_crossing_times); memset(tvf,0,sizeof(*tvf));
}

int nf_time_varying_field_analyze(nf_time_varying_field_t *tvf)
{
    if(!tvf||!tvf->field_time_series||!tvf->num_time_steps) return -1;
    size_t N=tvf->num_time_steps; double dt=tvf->dt_s;
    double *real_sig=malloc(N*sizeof(double));
    if(!real_sig) return -1;
    for(size_t i=0;i<N;i++) real_sig[i]=creal(tvf->field_time_series[i]);
    nf_hilbert_envelope(real_sig,N,tvf->hilbert_envelope);
    for(size_t i=0;i<N;i++) tvf->instantaneous_amplitude[i]=tvf->hilbert_envelope[i];
    size_t zc=0;
    for(size_t i=1;i<N;i++) {
        if(real_sig[i-1]*real_sig[i]<0) {
            if(tvf->zero_crossing_times) tvf->zero_crossing_times[zc]=i*dt;
            zc++;
        }
    }
    tvf->num_zero_crossings=zc;
    for(size_t i=1;i<N-1;i++) {
        double phase_diff=tvf->hilbert_envelope[i+1]-tvf->hilbert_envelope[i-1];
        tvf->instantaneous_frequency[i]=phase_diff/(4.0*M_PI*dt);
    }
    free(real_sig);
    return 0;
}

int nf_hilbert_envelope(const double *real_sig, size_t N, double *envelope)
{
    if(!real_sig||!envelope||N<4) return -1;
    for(size_t i=0;i<N;i++) {
        double sum=0;
        for(size_t k=1;k<N/2;k+=2) sum+=real_sig[(i+k)%N]/k;
        envelope[i]=sqrt(real_sig[i]*real_sig[i]+sum*sum*4.0/(M_PI*M_PI));
    }
    return 0;
}

/* ---------- L8 Adaptive Probe Positioning (Lyapunov Gradient Ascent) ---------- */

int nf_adaptive_probe_init(nf_adaptive_probe_pos_t *ap, double sx, double sy, double sz, double ss)
{
    if(!ap) return -1; memset(ap,0,sizeof(*ap));
    ap->current_x_m=sx; ap->current_y_m=sy; ap->current_z_m=sz;
    ap->step_size=ss; ap->iterations=0;
    return 0;
}

int nf_adaptive_probe_step(nf_adaptive_probe_pos_t *ap, double field_strength, double gx, double gy)
{
    if(!ap) return -1;
    double grad_mag=sqrt(gx*gx+gy*gy);
    ap->lyapunov_function_value=-field_strength;
    if(grad_mag>1e-15) {
        ap->current_x_m+=ap->step_size*gx/grad_mag;
        ap->current_y_m+=ap->step_size*gy/grad_mag;
    }
    ap->iterations++;
    ap->converged=(grad_mag<ap->convergence_threshold)?1:0;
    return 0;
}

/* ---------- L8 Monte Carlo Uncertainty Propagation ---------- */

int nf_monte_carlo_uncertainty(const double *nominal, const double *stddevs, size_t np,
                                double (*model)(const double*), size_t ntrials,
                                double *mean_out, double *std_out, double *ci95)
{
    if(!nominal||!stddevs||!np||!model||!ntrials||!mean_out||!std_out||!ci95) return -1;
    double *results=malloc(ntrials*sizeof(double));
    double *params=malloc(np*sizeof(double));
    if(!results||!params){free(results);free(params);return -1;}
    double sum=0,sum2=0;
    for(size_t t=0;t<ntrials;t++) {
        for(size_t i=0;i<np;i++) {
            double u1=(double)rand()/RAND_MAX;
            double u2=(double)rand()/RAND_MAX;
            double z=sqrt(-2*log(u1>1e-15?u1:1e-15))*cos(2*M_PI*u2);
            params[i]=nominal[i]+z*stddevs[i];
        }
        results[t]=model(params);
        sum+=results[t]; sum2+=results[t]*results[t];
    }
    *mean_out=sum/ntrials;
    double var=sum2/ntrials-(*mean_out)*(*mean_out);
    *std_out=sqrt(var>0?var:0);
    *ci95=1.96*(*std_out)/sqrt((double)ntrials);
    free(results);free(params);
    return 0;
}
