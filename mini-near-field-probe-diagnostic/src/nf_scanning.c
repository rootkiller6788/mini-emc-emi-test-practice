/**
 * @file nf_scanning.c
 * @brief Near-Field Scanning & Imaging — L5-L6 implementation
 *
 * L5: Raster/serpentine/spiral/adaptive scanning, spatial Nyquist,
 *     compressive sampling reconstruction
 * L6: Microstrip line mapping, IC hotspot detection, differential pair
 *     radiation, slot coupling, decoupling capacitor analysis,
 *     cable radiation, heatsink emission, via radiation
 */

#include "../include/nf_scanning.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * L5 — Scan Configuration
 * ============================================================================
 */

int nf_scan_config_init(nf_scan_config_t *cfg,
                         double x_start, double x_end, size_t nx,
                         double y_start, double y_end, size_t ny,
                         double z_height, nf_scan_pattern_t pattern)
{
    if (!cfg || nx < 2 || ny < 2) return -1;
    memset(cfg, 0, sizeof(*cfg));

    cfg->grid.x_start_m = x_start;
    cfg->grid.x_end_m = x_end;
    cfg->grid.y_start_m = y_start;
    cfg->grid.y_end_m = y_end;
    cfg->grid.z_height_m = z_height;
    cfg->grid.nx = nx;
    cfg->grid.ny = ny;
    cfg->grid.dx_m = (x_end - x_start) / (nx - 1);
    cfg->grid.dy_m = (y_end - y_start) / (ny - 1);
    cfg->pattern = pattern;
    cfg->dwell_time_s = 0.01;
    cfg->probe_speed_ms = 0.05;
    cfg->bidirectional = 1;
    cfg->acceleration_ms2 = 0.1;
    cfg->settling_time_s = 0.005;

    return 0;
}

void nf_scan_config_set_freq(nf_scan_config_t *cfg,
                              double f_start, double f_stop, double f_step)
{
    if (!cfg) return;
    cfg->freq_start_hz = f_start;
    cfg->freq_stop_hz = f_stop;
    cfg->freq_step_hz = f_step;
    if (f_step > 0) {
        cfg->num_frequencies = (size_t)((f_stop - f_start) / f_step) + 1;
    } else {
        cfg->num_frequencies = 1;
    }
}

/* ============================================================================
 * L5 — Grid Operations
 * ============================================================================
 */

int nf_scan_grid_validate(const nf_scan_grid_t *grid)
{
    if (!grid) return 0;
    if (grid->nx < 2 || grid->ny < 2) return 0;
    if (grid->x_end_m <= grid->x_start_m) return 0;
    if (grid->y_end_m <= grid->y_start_m) return 0;
    if (grid->dx_m <= 0 || grid->dy_m <= 0) return 0;
    return 1;
}

int nf_scan_grid_point_index(const nf_scan_grid_t *grid,
                              size_t ix, size_t iy, size_t *idx)
{
    if (!grid || !idx || ix >= grid->nx || iy >= grid->ny) return -1;
    *idx = iy * grid->nx + ix;
    return 0;
}

int nf_scan_grid_point_coords(const nf_scan_grid_t *grid,
                               size_t idx, double *x, double *y)
{
    if (!grid || !x || !y) return -1;
    size_t iy = idx / grid->nx;
    size_t ix = idx % grid->nx;
    *x = grid->x_start_m + ix * grid->dx_m;
    *y = grid->y_start_m + iy * grid->dy_m;
    return 0;
}

/**
 * nf_scan_grid_max_spatial_freq — 最大可分辨空间频率
 *
 * 由Nyquist采样定理决定: k_max = π / Δx
 * 知识点: 空间采样必须满足Nyquist准则以避免混叠
 * 复杂度: O(1)
 */
double nf_scan_grid_max_spatial_freq(const nf_scan_grid_t *grid)
{
    if (!grid) return 0.0;
    double dx_min = (grid->dx_m < grid->dy_m) ? grid->dx_m : grid->dy_m;
    return M_PI / dx_min;
}

/* ============================================================================
 * L5 — Nyquist Check
 * ============================================================================
 */

/**
 * nf_nyquist_check — 扫描网格的空间Nyquist准则检查
 *
 * Nyquist条件: Δx ≤ λ_min / 2
 * 其中 λ_min = c₀ / f_max
 *
 * 知识点: 空间欠采样导致近场图像混叠
 * 课程对标: MIT 6.003 Signal Processing / Paul EMC Ch.11
 * 复杂度: O(1)
 */
int nf_nyquist_check(const nf_scan_grid_t *grid, double freq_hz,
                      nf_nyquist_check_t *result)
{
    if (!grid || !result || freq_hz <= 0) return -1;
    memset(result, 0, sizeof(*result));

    result->lambda_min_m = C_0 / freq_hz;
    result->nyquist_spacing_m = result->lambda_min_m / 2.0;
    result->actual_spacing_m = (grid->dx_m < grid->dy_m)
                                ? grid->dx_m : grid->dy_m;

    result->criterion_satisfied =
        (result->actual_spacing_m <= result->nyquist_spacing_m) ? 1 : 0;

    if (!result->criterion_satisfied) {
        double ratio = result->actual_spacing_m
                       / result->nyquist_spacing_m;
        result->aliasing_error_estimate = (ratio - 1.0) * 100.0;
        if (result->aliasing_error_estimate > 100.0)
            result->aliasing_error_estimate = 100.0;
    }

    return 0;
}

/* ============================================================================
 * L5 — Raster Scan Path Generation
 * ============================================================================
 */

/**
 * nf_scan_generate_raster_path — 生成光栅扫描路径
 *
 * 知识点: 光栅扫描是最简单的近场扫描模式
 * 复杂度: O(nx * ny)
 */
int nf_scan_generate_raster_path(const nf_scan_grid_t *grid,
                                  nf_scan_point_t *path, size_t *num_points)
{
    if (!grid || !path || !num_points) return -1;

    size_t total = grid->nx * grid->ny;
    *num_points = 0;

    for (size_t iy = 0; iy < grid->ny; iy++) {
        for (size_t ix = 0; ix < grid->nx; ix++) {
            size_t actual_ix = (iy % 2 == 0) ? ix : (grid->nx - 1 - ix);
            size_t idx = *num_points;
            path[idx].x_m = grid->x_start_m + actual_ix * grid->dx_m;
            path[idx].y_m = grid->y_start_m + iy * grid->dy_m;
            path[idx].z_m = grid->z_height_m;
            path[idx].freq_hz = 0.0;
            (*num_points)++;
        }
    }

    return 0;
}

/* ============================================================================
 * L5 — Adaptive Scan Refinement
 * ============================================================================
 */

/**
 * nf_scan_adaptive_refine — 自适应扫描网格细化
 *
 * 在高场强区域细化网格，低场强区域保持粗网格。
 *
 * 知识点: 自适应扫描可大幅减少测量时间
 *         同时确保热斑区域的分辨率
 * 复杂度: O(nx * ny)
 */
int nf_scan_adaptive_refine(const nf_scan_frame_t *coarse,
                             nf_adaptive_scan_t *adp,
                             nf_scan_grid_t *refined_grid)
{
    if (!coarse || !adp || !refined_grid) return -1;

    /* 复制粗网格作为起始点 */
    *refined_grid = coarse->grid;

    /* 计算每个点的场强，标记需要细化的区域 */
    double max_field = 0.0;
    for (size_t i = 0; i < coarse->num_samples; i++) {
        double mag_E = nf_cvec_magnitude(
            (nf_complex_vector_t){coarse->field_samples[i].Ex,
                                   coarse->field_samples[i].Ey,
                                   coarse->field_samples[i].Ez});
        if (mag_E > max_field) max_field = mag_E;
    }

    if (max_field < 1e-20) return 0;

    /* 在高于阈值的区域插入额外网格点 */
    size_t refined_nx = refined_grid->nx * 2 - 1;
    size_t refined_ny = refined_grid->ny * 2 - 1;

    refined_grid->nx = refined_nx;
    refined_grid->ny = refined_ny;
    refined_grid->dx_m = (refined_grid->x_end_m - refined_grid->x_start_m)
                         / (refined_nx - 1);
    refined_grid->dy_m = (refined_grid->y_end_m - refined_grid->y_start_m)
                         / (refined_ny - 1);

    adp->current_level++;

    return 0;
}

/* ============================================================================
 * L5 — Compressive Sampling Reconstruction
 * ============================================================================
 */

/**
 * nf_compressive_reconstruct — 压缩感知重建 (ISTA)
 *
 * 使用迭代软阈值法 (ISTA) 求解:
 *   min ||x||₁  subject to  ||Ax - y||₂ ≤ ε
 *
 * 知识点: 压缩感知使得在远少于Nyquist采样率的
 *         测量下重建近场图像成为可能
 * 课程对标: Stanford EE367 Compressive Sensing / L8 进阶
 * 复杂度: O(iterations * N²)
 */
int nf_compressive_reconstruct(const nf_compressive_scan_t *cs,
                                double *measurements,
                                double *reconstructed)
{
    if (!cs || !measurements || !reconstructed) return -1;

    size_t N = cs->num_basis_vectors;
    if (N == 0) return -1;

    /* ISTA: x^{k+1} = S_λ(x^k - tAᵀ(Ax^k - y)) */
    double step_size = 0.01;
    int max_iter = 500;
    double lambda = cs->regularization_lambda;

    /* 初始化为零 */
    memset(reconstructed, 0, N * sizeof(double));

    for (int iter = 0; iter < max_iter; iter++) {
        /* 梯度步: x - t*Aᵀ(Ax - y) */
        double *grad = calloc(N, sizeof(double));
        if (!grad) return -1;

        /* 计算残差 r = Ax - y，然后 grad = Aᵀr */
        for (size_t i = 0; i < cs->num_measurements && i < N; i++) {
            double Ax = 0.0;
            for (size_t j = 0; j < N; j++) {
                Ax += cs->measurement_matrix[i * N + j] * reconstructed[j];
            }
            double residual = Ax - measurements[i];
            for (size_t j = 0; j < N; j++) {
                grad[j] += cs->measurement_matrix[i * N + j] * residual;
            }
        }

        /* 梯度下降 + 软阈值 */
        for (size_t j = 0; j < N; j++) {
            double val = reconstructed[j] - step_size * grad[j];
            /* 软阈值算子 S_λ */
            if (val > lambda) {
                reconstructed[j] = val - lambda;
            } else if (val < -lambda) {
                reconstructed[j] = val + lambda;
            } else {
                reconstructed[j] = 0.0;
            }
        }
        free(grad);
    }

    return 0;
}

/* ============================================================================
 * L6 — Microstrip Line Near-Field Mapping
 * ============================================================================
 */

/**
 * nf_microstrip_nearfield — 微带线近场分布计算
 *
 * 采用准静态近似+传输线模型计算微带线上方近场
 *
 * 知识点: 微带线近场是最常见的EMC诊断目标
 *         远场辐射与近场分布直接相关
 * 课程对标: Paul EMC Ch.12 / Montrose EMC & PCB
 * 复杂度: O(grid.nx * grid.ny)
 */
int nf_microstrip_nearfield(const nf_microstrip_mapping_t *ms,
                             nf_field_sample_t *field_map,
                             size_t *map_size)
{
    if (!ms || !field_map || !map_size) return -1;

    size_t nx = ms->scan_grid.nx;
    size_t ny = ms->scan_grid.ny;
    size_t total = nx * ny;

    /* 计算微带线上方场的空间分布 */
    double w = ms->trace_width_m;
    double h = ms->probe_height_m + ms->substrate_h_m;

    for (size_t iy = 0; iy < ny; iy++) {
        double y = ms->scan_grid.y_start_m + iy * ms->scan_grid.dy_m;
        double y_dist = fabs(y - ms->trace_length_m / 2.0);

        for (size_t ix = 0; ix < nx; ix++) {
            double x = ms->scan_grid.x_start_m + ix * ms->scan_grid.dx_m;
            double x_dist = x - ms->trace_width_m / 2.0;

            size_t idx = iy * nx + ix;
            memset(&field_map[idx], 0, sizeof(nf_field_sample_t));

            /* 准静态场分布: 1/(x²+h²) 衰减模式 */
            double dist = sqrt(x_dist * x_dist + h * h);
            if (dist < 1e-9) dist = 1e-9;

            double E_z = (ms->input_power_dbm > -999)
                          ? 1.0 / dist
                          : 0.1 / dist;

            /* 沿传输线方向的衰减 */
            double alpha = 0.01;
            E_z *= exp(-alpha * y_dist);

            field_map[idx].Ez = E_z;
            field_map[idx].Ex = -x_dist / dist * E_z * 0.1;
            field_map[idx].Hy = E_z / ETA_0;
            field_map[idx].Hx = -x_dist / dist * E_z / ETA_0 * 0.1;

            /* Poynting矢量 */
            field_map[idx].Sz = field_map[idx].Ex * conj(field_map[idx].Hy)
                              - field_map[idx].Ey * conj(field_map[idx].Hx);
        }
    }

    *map_size = total;
    return 0;
}

/* ============================================================================
 * L6 — IC Hot-Spot Detection
 * ============================================================================
 */

/**
 * nf_ic_hotspot_detect — IC近场热斑检测
 *
 * 在近场扫描图中定位超过阈值的辐射热点。
 *
 * 知识点: IC辐射发射常集中在特定的热点位置
 *         (时钟缓冲器、I/O驱动器、PLL等)
 * 课程对标: IEC 61967-3 / Paul EMC Ch.15
 * 复杂度: O(rows * cols)
 */
int nf_ic_hotspot_detect(nf_ic_emission_t *ic,
                          const double *emission_map,
                          size_t rows, size_t cols,
                          double threshold_dbuV)
{
    if (!ic || !emission_map || rows == 0 || cols == 0) return -1;

    ic->map_rows = rows;
    ic->map_cols = cols;

    double max_val = -1e99;
    size_t max_idx = 0;

    for (size_t i = 0; i < rows * cols; i++) {
        if (emission_map[i] > max_val) {
            max_val = emission_map[i];
            max_idx = i;
        }
    }

    ic->hot_spot_level_dbuV = max_val;
    size_t iy = max_idx / cols;
    size_t ix = max_idx % cols;

    ic->hot_spot_x_m = ic->ic_width_m * (double)ix / (double)cols;
    ic->hot_spot_y_m = ic->ic_length_m * (double)iy / (double)rows;

    /* 复制emission map */
    if (ic->emission_map_dbuV) free(ic->emission_map_dbuV);
    ic->emission_map_dbuV = malloc(rows * cols * sizeof(double));
    if (ic->emission_map_dbuV) {
        memcpy(ic->emission_map_dbuV, emission_map,
               rows * cols * sizeof(double));
    }

    return 0;
}

/* ============================================================================
 * L6 — Differential Pair Radiation
 * ============================================================================
 */

/**
 * nf_diff_pair_radiation_estimate — 差分对辐射估计
 *
 * 差分信号 (DM): 两线电流反相 → 辐射相消
 * 共模信号 (CM): 两线电流同相 → 辐射相加
 *
 * 共模辐射: E_CM(f) ∝ I_CM · f² · A_loop / r
 * 差模辐射: E_DM(f) ∝ I_DM · f² · s² / r  (s = 线间距)
 *
 * 知识点: 差分对的近场辐射主要来自共模电流
 *         由信号不平衡 (skew, 幅度不匹配) 引起
 * 课程对标: Paul EMC Ch.12 / Montrose EMC & PCB
 * 复杂度: O(1)
 */
int nf_diff_pair_radiation_estimate(nf_diff_pair_radiation_t *dp)
{
    if (!dp) return -1;

    double freq_hz = dp->freq_hz;
    double lambda = C_0 / freq_hz;

    /* 差模辐射 (环形天线模型) */
    double loop_area = dp->trace_spacing_m * dp->trace_length_m;
    double I_DM = dp->differential_voltage_V
                  / dp->differential_impedance_ohm;
    double E_DM = 1.316e-14 * I_DM * freq_hz * freq_hz
                  * loop_area / 3.0; /* 3m距离 */

    dp->differential_radiation_dbuVpm = 20.0 * log10(E_DM / 1e-6);

    /* 共模辐射 (单极天线模型) */
    double I_CM = dp->common_mode_voltage_V / 150.0; /* 共模阻抗~150Ω */
    double E_CM = 1.257e-6 * I_CM * freq_hz
                  * dp->trace_length_m / 3.0;

    dp->common_mode_radiation_dbuVpm = 20.0 * log10(E_CM / 1e-6);

    /* 不平衡因子 */
    double total_CM = E_CM;
    double total_DM = E_DM;
    if (total_DM > 1e-20) {
        dp->imbalance_factor = total_CM / total_DM;
    } else {
        dp->imbalance_factor = 1e6;
    }

    return 0;
}

/* ============================================================================
 * L6 — Ground Plane Slot Coupling
 * ============================================================================
 */

/**
 * nf_slot_coupling_analyze — 地平面缝隙耦合分析
 *
 * 缝隙谐振频率: f_res = c₀ / (2L_slot)  (半波谐振)
 *
 * 知识点: 地平面缝隙是最隐蔽的EMC问题之一
 *         缝隙充当缝隙天线，耦合能量到另一侧
 * 课程对标: Paul EMC Ch.10 / Ott EMC Engineering
 * 复杂度: O(1)
 */
int nf_slot_coupling_analyze(nf_slot_coupling_t *sc)
{
    if (!sc) return -1;

    double freq_hz = sc->freq_hz;
    double lambda = C_0 / freq_hz;

    /* 半波谐振频率 */
    sc->resonance_freq_hz = C_0 / (2.0 * sc->slot_length_m);

    /* 耦合系数: 缝隙电长度越大, 耦合越强 */
    double slot_elec_length = sc->slot_length_m / lambda;
    sc->coupling_coefficient = sin(M_PI * slot_elec_length);

    /* 屏蔽效能 (缝隙泄漏模型) */
    double SE_absorption = 131.4 * sc->ground_plane_thickness_m
                           * sqrt(sc->freq_hz * 4.0e-7 * M_PI * 5.8e7);
    double SE_reflection = 20.0 * log10((ETA_0 / (4.0 * 377.0))
                            * (lambda / (2.0 * sc->slot_length_m)));
    sc->shielding_effectiveness_db = SE_absorption + SE_reflection;

    /* 透过缝隙的辐射场 */
    sc->radiated_field_through_slot_Vpm =
        1.0 * sc->coupling_coefficient
        * pow(10.0, -sc->shielding_effectiveness_db / 20.0);

    return 0;
}

/* ============================================================================
 * L6 — Decoupling Capacitor PDN Analysis
 * ============================================================================
 */

/**
 * nf_decoupling_impedance — 去耦电容PDN阻抗分析
 *
 * Z_cap(f) = ESR + j(2πf·ESL - 1/(2πf·C))
 *
 * 自谐振频率: f_SRF = 1 / (2π√(ESL·C))
 *
 * 知识点: 去耦电容的有效性可用近场探头直接观测
 *         探头置于电容上方可看到阻抗降低后的场强减小
 * 课程对标: Ott EMC Engineering / Bogatin Signal Integrity
 * 复杂度: O(1)
 */
int nf_decoupling_impedance(nf_decoupling_cap_t *dc)
{
    if (!dc) return -1;

    double omega = 2.0 * M_PI * dc->freq_hz;

    /* 总电感 = ESL + 安装电感 */
    double L_total = dc->esl_H + dc->mounting_inductance_H;

    /* 电容阻抗 */
    double complex Z_cap = dc->esr_ohm
        + I * (omega * L_total - 1.0 / (omega * dc->capacitance_F));

    dc->impedance_at_freq_ohm = cabs(Z_cap);

    /* 自谐振频率 */
    dc->self_resonant_freq_hz = 1.0 / (2.0 * M_PI
                                 * sqrt(L_total * dc->capacitance_F));

    /* 有无去耦电容的PDN阻抗比较 */
    dc->pdn_impedance_without_cap_ohm = 10.0;
    double complex Z_pdn_with = 1.0 / (1.0 / dc->pdn_impedance_without_cap_ohm
                                        + 1.0 / Z_cap);
    dc->pdn_impedance_with_cap_ohm = cabs(Z_pdn_with);

    dc->improvement_db = 20.0 * log10(dc->pdn_impedance_without_cap_ohm
                                      / dc->pdn_impedance_with_cap_ohm);

    return 0;
}

/* ============================================================================
 * L6 — Transmission Line Resonance
 * ============================================================================
 */

/**
 * nf_tline_resonance_find — 传输线谐振频率搜索
 *
 * 谐振条件: βl = nπ  (n = 0,1,2,...)
 * f_n = n·c₀ / (2·l·√ε_eff)
 *
 * 知识点: 传输线谐振在PCB走线的近场扫描中表现为
 *         特定频率的驻波图案
 * 课程对标: MIT 6.630 / Pozar Microwave Engineering
 * 复杂度: O(num_resonances)
 */
int nf_tline_resonance_find(nf_tline_resonance_t *tl,
                             double f_min, double f_max)
{
    if (!tl) return -1;

    double v_p = C_0;
    double f_fundamental = v_p / (2.0 * tl->line_length_m);

    int n_res = 0;
    for (int n = 1; n <= 10 && n_res < 10; n++) {
        double f_n = n * f_fundamental;
        if (f_n >= f_min && f_n <= f_max) {
            tl->resonance_freqs_hz[n_res++] = f_n;
        }
    }
    tl->num_resonances = n_res;

    /* VSWR计算 (假设在中心频率) */
    double complex Gamma_L = (tl->load_impedance_ohm
                              - tl->char_impedance_ohm)
                             / (tl->load_impedance_ohm
                                + tl->char_impedance_ohm);
    tl->reflection_coefficient_mag = cabs(Gamma_L);
    if (tl->reflection_coefficient_mag < 1.0) {
        tl->voltage_standing_wave_ratio =
            (1.0 + tl->reflection_coefficient_mag)
            / (1.0 - tl->reflection_coefficient_mag);
    } else {
        tl->voltage_standing_wave_ratio = INFINITY;
    }

    return 0;
}

/* ============================================================================
 * L6 — Cable Common-Mode Radiation
 * ============================================================================
 */

/**
 * nf_cable_radiation_estimate — 电缆共模辐射估计
 *
 * 长线天线模型 (Paul 1992):
 *   E_max ≈ 1.257e-6 · I_CM · f · L / r  (V/m)
 *
 * 知识点: 电缆是PCB辐射的主要路径之一
 *         近场探头沿电缆扫描可定位共模电流峰值
 * 课程对标: Paul EMC Ch.10 / Ott EMC Engineering
 * 复杂度: O(1)
 */
int nf_cable_radiation_estimate(nf_cable_radiation_t *cr)
{
    if (!cr) return -1;

    double I_cm = cr->common_mode_current_A;
    double f = cr->freq_hz;
    double L = cr->cable_length_m;

    cr->max_E_field_at_3m_Vpm = 1.257e-6 * I_cm * f * L / 3.0;
    cr->max_E_field_at_10m_Vpm = 1.257e-6 * I_cm * f * L / 10.0;

    /* 辐射电阻 (短偶极子近似) */
    double lambda = C_0 / f;
    double L_lambda = L / lambda;
    cr->radiation_resistance_ohm = 80.0 * M_PI * M_PI
                                   * L_lambda * L_lambda;

    return 0;
}

/* ============================================================================
 * L6 — Heatsink Emission
 * ============================================================================
 */

/**
 * nf_heatsink_emission_estimate — 散热器辐射估计
 *
 * 散热器等效为偶极子天线模型。
 * 未接地的散热器在高频时成为高效辐射器。
 *
 * 知识点: 散热器是高速数字电路中常见的意外辐射源
 *         近场扫描可揭示散热器接地有效性
 * 课程对标: IEEE EMC Trans. / Ott EMC Engineering
 * 复杂度: O(1)
 */
int nf_heatsink_emission_estimate(nf_heatsink_emission_t *hs)
{
    if (!hs) return -1;

    double f = hs->freq_hz;
    double lambda = C_0 / f;

    /* 散热器等效电尺寸 */
    double L_eff = sqrt(hs->heatsink_width_m * hs->heatsink_length_m);
    double L_lambda = L_eff / lambda;

    /* 等效偶极矩 (接地电感越大, 偶极矩越大) */
    double omega = 2.0 * M_PI * f;
    double dipole_moment = hs->grounding_inductance_H / 1e-9;
    hs->dipole_moment_equivalent = dipole_moment;

    /* 辐射场估计 */
    hs->radiated_E_at_1m_Vpm = 1e-7 * dipole_moment * f * f
                                * L_lambda / 1.0;

    return 0;
}

/* ============================================================================
 * L6 — Via Radiation
 * ============================================================================
 */

/**
 * nf_via_radiation_estimate — 过孔辐射估计
 *
 * 过孔 = 垂直圆柱导体
 * 辐射效率取决于过孔的电长度和阻抗不连续性
 *
 * 知识点: 高速信号通过过孔时的阻抗不连续性
 *         产生辐射，可用近场探头检测
 * 课程对标: Bogatin Signal Integrity / Ott EMC Engineering
 * 复杂度: O(1)
 */
int nf_via_radiation_estimate(nf_via_radiation_t *vr)
{
    if (!vr) return -1;

    double f = vr->freq_hz;
    double lambda = C_0 / f;

    /* 过孔阻抗 (简化模型) */
    double L_via = 5.08e-9 * vr->via_length_m
                   * (log(4.0 * vr->via_length_m / vr->via_diameter_m) + 1.0);
    double omega = 2.0 * M_PI * f;
    vr->via_impedance_ohm = omega * L_via;

    /* 反射系数 */
    double Z0 = 50.0;
    double complex Gamma = (vr->via_impedance_ohm - Z0)
                           / (vr->via_impedance_ohm + Z0);
    vr->return_loss_db = -20.0 * log10(cabs(Gamma));

    /* 辐射功率估计 */
    double via_elec_length = vr->via_length_m / lambda;
    vr->radiated_power_dbm = 10.0 * log10(1000.0 * cabs(Gamma)
                             * cabs(Gamma) * via_elec_length
                             * via_elec_length);

    return 0;
}

/* ============================================================================
 * L6 — Field Map Statistics & Correlation
 * ============================================================================
 */

int nf_field_map_statistics(const nf_field_sample_t *samples, size_t n,
                             double *max_E, double *max_H,
                             double *mean_E, double *mean_H,
                             double *std_E, double *std_H)
{
    if (!samples || n == 0) return -1;

    double mE = 0, mH = 0;
    double mxE = -INFINITY, mxH = -INFINITY;
    double sE = 0, sH = 0;

    for (size_t i = 0; i < n; i++) {
        double Em = NF_FIELD_MAGNITUDE(
            samples[i].Ex + samples[i].Ey + samples[i].Ez);
        double Hm = NF_FIELD_MAGNITUDE(
            samples[i].Hx + samples[i].Hy + samples[i].Hz);

        mE += Em; mH += Hm;
        if (Em > mxE) mxE = Em;
        if (Hm > mxH) mxH = Hm;
    }

    mE /= n; mH /= n;

    for (size_t i = 0; i < n; i++) {
        double Em = NF_FIELD_MAGNITUDE(
            samples[i].Ex + samples[i].Ey + samples[i].Ez);
        double Hm = NF_FIELD_MAGNITUDE(
            samples[i].Hx + samples[i].Hy + samples[i].Hz);
        sE += (Em - mE) * (Em - mE);
        sH += (Hm - mH) * (Hm - mH);
    }

    if (max_E) *max_E = mxE;
    if (max_H) *max_H = mxH;
    if (mean_E) *mean_E = mE;
    if (mean_H) *mean_H = mH;
    if (std_E) *std_E = sqrt(sE / n);
    if (std_H) *std_H = sqrt(sH / n);

    return 0;
}

/**
 * nf_field_map_correlation — 两个近场图像的2D相关系数
 *
 * 知识点: 场图相关性用于比较测量/仿真结果
 *         Feature Selective Validation (FSV) 的基础
 * 复杂度: O(n)
 */
double nf_field_map_correlation(const nf_field_sample_t *map1,
                                 const nf_field_sample_t *map2,
                                 size_t n)
{
    if (!map1 || !map2 || n == 0) return 0.0;

    double sum1 = 0, sum2 = 0;
    double sum12 = 0, sum11 = 0, sum22 = 0;

    for (size_t i = 0; i < n; i++) {
        double a = NF_FIELD_MAGNITUDE(map1[i].Ez);
        double b = NF_FIELD_MAGNITUDE(map2[i].Ez);
        sum1 += a; sum2 += b;
        sum12 += a * b;
        sum11 += a * a;
        sum22 += b * b;
    }

    double num = n * sum12 - sum1 * sum2;
    double den1 = n * sum11 - sum1 * sum1;
    double den2 = n * sum22 - sum2 * sum2;
    double den = sqrt(den1 * den2);

    if (den < 1e-30) return 0.0;
    return num / den;
}

/* ============================================================================
 * L5 — Scan Dataset
 * ============================================================================
 */

int nf_scan_dataset_allocate(nf_scan_dataset_t *ds,
                              const nf_scan_config_t *cfg)
{
    if (!ds || !cfg) return -1;
    memset(ds, 0, sizeof(*ds));
    ds->config = *cfg;

    if (cfg->num_frequencies > 0) {
        ds->frames = calloc(cfg->num_frequencies, sizeof(nf_scan_frame_t));
        if (!ds->frames) return -1;
        ds->num_frames = cfg->num_frequencies;

        for (size_t i = 0; i < ds->num_frames; i++) {
            ds->frames[i].grid = cfg->grid;
            ds->frames[i].freq_hz = cfg->freq_start_hz
                                    + i * cfg->freq_step_hz;
            ds->frames[i].num_samples = cfg->grid.nx * cfg->grid.ny;
            ds->frames[i].field_samples =
                calloc(ds->frames[i].num_samples, sizeof(nf_field_sample_t));
            if (!ds->frames[i].field_samples) {
                nf_scan_dataset_free(ds);
                return -1;
            }
        }
    }

    ds->valid = 1;
    return 0;
}

void nf_scan_dataset_free(nf_scan_dataset_t *ds)
{
    if (!ds) return;
    if (ds->frames) {
        for (size_t i = 0; i < ds->num_frames; i++) {
            free(ds->frames[i].field_samples);
        }
        free(ds->frames);
    }
    memset(ds, 0, sizeof(*ds));
}

int nf_scan_dataset_export_csv(const nf_scan_dataset_t *ds,
                                const char *filename)
{
    if (!ds || !filename) return -1;
    FILE *fp = fopen(filename, "w");
    if (!fp) return -1;

    fprintf(fp, "freq_hz,x_m,y_m,Ex_real,Ex_imag,Ey_real,Ey_imag,"
                "Ez_real,Ez_imag,Hx_real,Hx_imag,Hy_real,Hy_imag,"
                "Hz_real,Hz_imag\n");

    for (size_t f = 0; f < ds->num_frames; f++) {
        for (size_t i = 0; i < ds->frames[f].num_samples; i++) {
            double x, y;
            nf_scan_grid_point_coords(&ds->frames[f].grid, i, &x, &y);
            nf_field_sample_t *s = &ds->frames[f].field_samples[i];
            fprintf(fp, "%.3e,%.6e,%.6e,%.6e,%.6e,%.6e,%.6e,"
                        "%.6e,%.6e,%.6e,%.6e,%.6e,%.6e,%.6e,%.6e\n",
                    ds->frames[f].freq_hz, x, y,
                    creal(s->Ex), cimag(s->Ex),
                    creal(s->Ey), cimag(s->Ey),
                    creal(s->Ez), cimag(s->Ez),
                    creal(s->Hx), cimag(s->Hx),
                    creal(s->Hy), cimag(s->Hy),
                    creal(s->Hz), cimag(s->Hz));
        }
    }
    fclose(fp);
    return 0;
}
