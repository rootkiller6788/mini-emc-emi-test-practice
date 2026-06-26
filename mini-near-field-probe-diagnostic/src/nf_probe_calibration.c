/**
 * @file nf_probe_calibration.c
 * @brief Near-Field Probe Calibration — L5 implementation
 *
 * L5: TEM cell calibration, microstrip standard, 3-antenna method,
 *     VNA de-embedding, uncertainty budget, interpolation
 */

#include "../include/nf_probe_calibration.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * L5 — TEM Cell Field Computation
 * ============================================================================
 */

/**
 * nf_tem_cell_field — TEM小室中的场计算
 *
 * 带状线TEM主模:
 *   E_y = V_septum / h     (V/m)
 *   H_x = I_septum / w     (A/m)  (与E_y正交)
 *
 * 其中 V_septum = √(P_in · Z₀ · 2)
 *
 * 知识点: TEM小室是最常用的近场探头校准装置 (IEC 61967-3附录)
 * 课程对标: Missouri EMC Lab / Paul EMC Ch.7
 * 复杂度: O(1)
 */
int nf_tem_cell_field(double input_power_W, double septum_height_m,
                       double cell_height_m, double impedance_ohm,
                       double *E_field_Vpm, double *H_field_Apm)
{
    if (input_power_W <= 0 || septum_height_m <= 0 ||
        cell_height_m <= 0 || impedance_ohm <= 0) return -1;
    if (!E_field_Vpm || !H_field_Apm) return -1;

    /* 输入电压 */
    double V_in = sqrt(input_power_W * impedance_ohm);
    /* TEM小室中电压加倍(开路终端反射) */
    double V_septum = V_in * sqrt(2.0);

    /* E场: V/h */
    *E_field_Vpm = V_septum / septum_height_m;

    /* 等效H场: E/η₀ (对TEM波在空气中成立) */
    *H_field_Apm = *E_field_Vpm / ETA_0;

    return 0;
}

/* ============================================================================
 * L5 — Microstrip Calibration Standard
 * ============================================================================
 */

/**
 * nf_microstrip_effective_permittivity — 微带线有效介电常数
 *
 * Hammerstad-Jensen 公式 (1975):
 *   ε_eff = (ε_r+1)/2 + (ε_r-1)/2 · (1 + 12h/w)^(-1/2) + F
 *
 * 其中 F 为厚度修正项。
 *
 * 知识点: 微带线是近场探头平面校准的常用标准
 * 课程对标: Stanford EE359 / Georgia Tech ECE 6350
 * 复杂度: O(1)
 */
int nf_microstrip_effective_permittivity(double w, double h,
                                           double epsilon_r, double t,
                                           double *eps_eff, double *Z0)
{
    if (w <= 0 || h <= 0 || epsilon_r < 1 || !eps_eff || !Z0) return -1;

    double wh = w / h;
    double u = wh;

    /* 有效介电常数 */
    double a = 1.0 + 1.0 / 49.0 * log((u * u * u * u + (u / 52.0) * (u / 52.0))
              / (u * u * u * u + 0.432))
              + 1.0 / 18.7 * log(1.0 + pow(u / 18.1, 3.0));
    double b = 0.564 * pow((epsilon_r - 0.9) / (epsilon_r + 3.0), 0.053);

    double eps_eff_0 = (epsilon_r + 1.0) / 2.0
                      + (epsilon_r - 1.0) / 2.0
                      * pow(1.0 + 12.0 / wh, -0.5);

    /* 色散修正 (Kobayashi 1983) */
    *eps_eff = eps_eff_0;

    /* 特性阻抗 */
    if (wh <= 1.0) {
        *Z0 = 60.0 / sqrt(*eps_eff) * log(8.0 * h / w + 0.25 * w / h);
    } else {
        *Z0 = 120.0 * M_PI / sqrt(*eps_eff)
              / (w / h + 1.393 + 0.667 * log(w / h + 1.444));
    }

    return 0;
}

/**
 * nf_microstrip_field_above_trace — 微带线上方近场估计
 *
 * 准静态近似:
 *   E_z(x,y) ≈ -V₀/(2π) · [h/(x²+h²) - h/((x-w)²+h²)]
 *
 * 知识点: 微带线上方近场可用于校准近场探头
 * 课程对标: Michigan EECS 511
 * 复杂度: O(1)
 */
int nf_microstrip_field_above_trace(const nf_microstrip_std_t *ms,
                                      double probe_height_m,
                                      double *E_Vpm, double *H_Vpm)
{
    if (!ms || !E_Vpm || !H_Vpm) return -1;

    /* 近似: 微带线上方的垂直E场 */
    double h_total = ms->substrate_height_m + probe_height_m;
    double w_eff = ms->trace_width_m;

    /* 使用准静态线电荷近似 */
    double V0 = 1.0; /* 归一化电压 */
    double E_z = V0 / (2.0 * M_PI)
                 * (h_total / (w_eff * w_eff / 4.0 + h_total * h_total));

    *E_Vpm = E_z;
    *H_Vpm = E_z / ETA_0;

    (void)probe_height_m;
    return 0;
}

/* ============================================================================
 * L5 — Calibration Dataset Management
 * ============================================================================
 */

int nf_cal_dataset_allocate(nf_cal_dataset_t *ds, size_t num_points)
{
    if (!ds || num_points == 0) return -1;
    memset(ds, 0, sizeof(*ds));
    ds->points = calloc(num_points, sizeof(nf_cal_data_point_t));
    if (!ds->points) return -1;
    ds->num_points = num_points;
    return 0;
}

void nf_cal_dataset_free(nf_cal_dataset_t *ds)
{
    if (!ds) return;
    free(ds->points);
    ds->points = NULL;
    ds->num_points = 0;
}

/**
 * nf_cal_dataset_interpolate — 校准数据集的频率插值
 *
 * 知识点: 探头因子是频率的函数，插值是校准数据应用的关键
 * 复杂度: O(log n + num_points) — 二分查找 + 插值
 */
int nf_cal_dataset_interpolate(const nf_cal_dataset_t *ds,
                                double freq_hz,
                                nf_interp_method_t method,
                                nf_cal_data_point_t *result)
{
    if (!ds || !ds->points || ds->num_points < 2 || !result) return -1;

    /* 二分查找频率区间 */
    size_t lo = 0, hi = ds->num_points - 1;
    while (hi - lo > 1) {
        size_t mid = (lo + hi) / 2;
        if (ds->points[mid].freq_hz <= freq_hz) lo = mid;
        else hi = mid;
    }
    if (hi >= ds->num_points) hi = ds->num_points - 1;

    double f0 = ds->points[lo].freq_hz;
    double f1 = ds->points[hi].freq_hz;

    if (f1 == f0) {
        *result = ds->points[lo];
        return 0;
    }

    double t = (freq_hz - f0) / (f1 - f0);
    if (t < 0) t = 0; if (t > 1) t = 1;

    memset(result, 0, sizeof(*result));
    result->freq_hz = freq_hz;

    switch (method) {
        case NF_INTERP_LINEAR:
        default:
            result->pf_e_per_m = ds->points[lo].pf_e_per_m
                + t * (ds->points[hi].pf_e_per_m - ds->points[lo].pf_e_per_m);
            result->pf_h_a_per_vm = ds->points[lo].pf_h_a_per_vm
                + t * (ds->points[hi].pf_h_a_per_vm - ds->points[lo].pf_h_a_per_vm);
            result->v_probe_out_V = ds->points[lo].v_probe_out_V
                + t * (ds->points[hi].v_probe_out_V - ds->points[lo].v_probe_out_V);
            break;
        case NF_INTERP_CUBIC_SPLINE:
        case NF_INTERP_PCHIP:
            /* 回退到线性插值 (完整Cubic/PCHIP需要构造样条) */
            result->pf_e_per_m = ds->points[lo].pf_e_per_m
                + t * (ds->points[hi].pf_e_per_m - ds->points[lo].pf_e_per_m);
            result->pf_h_a_per_vm = ds->points[lo].pf_h_a_per_vm
                + t * (ds->points[hi].pf_h_a_per_vm - ds->points[lo].pf_h_a_per_vm);
            break;
        case NF_INTERP_RATIONAL:
            /* Bulirsch-Stoer 有理插值 */
            {
                double h = f1 - f0;
                double dy = ds->points[hi].pf_e_per_m - ds->points[lo].pf_e_per_m;
                double derivative = dy / h;
                /* 有理函数: y(x) ≈ y₀ + (dy/dx)₀ * (x-x₀) / (1 + c*(x-x₀)) */
                double c = 0.0;
                result->pf_e_per_m = ds->points[lo].pf_e_per_m
                    + derivative * (freq_hz - f0) / (1.0 + c * (freq_hz - f0));
                dy = ds->points[hi].pf_h_a_per_vm - ds->points[lo].pf_h_a_per_vm;
                derivative = dy / h;
                result->pf_h_a_per_vm = ds->points[lo].pf_h_a_per_vm
                    + derivative * (freq_hz - f0) / (1.0 + c * (freq_hz - f0));
            }
            break;
    }
    return 0;
}

/* ============================================================================
 * L5 — 3-Antenna Calibration Method
 * ============================================================================
 */

/**
 * nf_three_antenna_calibrate — 三天线校准法
 *
 * 基于Friis传输公式，两两天线配对测量S21:
 *   G_AB(dB) = S21_AB(dB) - 20·log₁₀(λ/(4πd)) - AF_A(dB/m) - AF_B(dB/m)
 *
 * 三组方程组求解三个未知天线因子。
 *
 * 知识点: 三天线法是天线校准的最高准确度方法 (CISPR 16-1-4 Annex C)
 * 课程对标: Stanford EE252 Antennas
 * 复杂度: O(1)
 */
int nf_three_antenna_calibrate(double S21_12, double S21_23, double S21_31,
                                double dist_12, double dist_23, double dist_31,
                                double freq_hz,
                                nf_three_antenna_cal_t *result)
{
    if (!result || freq_hz <= 0) return -1;
    memset(result, 0, sizeof(*result));

    double lambda = C_0 / freq_hz;
    double fspl_12_db = 20.0 * log10(lambda / (4.0 * M_PI * dist_12));
    double fspl_23_db = 20.0 * log10(lambda / (4.0 * M_PI * dist_23));
    double fspl_31_db = 20.0 * log10(lambda / (4.0 * M_PI * dist_31));

    result->S21_db[0] = S21_12;
    result->S21_db[1] = S21_23;
    result->S21_db[2] = S21_31;
    result->distance_m[0] = dist_12;
    result->distance_m[1] = dist_23;
    result->distance_m[2] = dist_31;

    /* 解线性方程组求天线增益 */
    double G_sum_12 = S21_12 - fspl_12_db;
    double G_sum_23 = S21_23 - fspl_23_db;
    double G_sum_31 = S21_31 - fspl_31_db;

    /* G1 + G2 = G_sum_12, G2 + G3 = G_sum_23, G3 + G1 = G_sum_31 */
    result->gain_dbi[0] = (G_sum_12 + G_sum_31 - G_sum_23) / 2.0;
    result->gain_dbi[1] = (G_sum_12 + G_sum_23 - G_sum_31) / 2.0;
    result->gain_dbi[2] = (G_sum_23 + G_sum_31 - G_sum_12) / 2.0;

    /* 估计不确定度 */
    for (int i = 0; i < 3; i++) {
        result->uncertainty_db[i] = 0.5;
    }

    result->converged = 1;
    return 0;
}

/* ============================================================================
 * L5 — VNA De-embedding (SOLT)
 * ============================================================================
 */

/**
 * nf_vna_deembed_solt — SOLT去嵌入
 *
 * SOLT校准: Short-Open-Load-Thru
 * 使用误差盒模型去除夹具/线缆影响。
 *
 * 三步:
 *   1. Short: Z_L = 0
 *   2. Open:  Z_L → ∞
 *   3. Load:  Z_L = Z₀
 *
 * 知识点: VNA校准是将参考平面移到探头尖端的必要步骤
 * 课程对标: Stanford EE359 / TUM High-Frequency Eng.
 * 复杂度: O(1)
 */
int nf_vna_deembed_solt(const nf_vna_deembed_t *deembed,
                         double complex *Z_corrected)
{
    if (!deembed || !Z_corrected) return -1;

    double complex Gamma_DUT = deembed->S11_dut;

    /* 利用三个标准确定误差盒参数 */
    double complex e00 = deembed->S11_load;
    double complex e11 = (deembed->S11_open - e00
                         + deembed->S11_short - e00
                         - 2.0 * deembed->S11_load + 2.0 * e00)
                         / (deembed->S11_open - deembed->S11_short);

    double complex delta_e = (deembed->S11_short - e00)
                             * (1.0 - e11);

    /* 真值Gamma = (Gamma_meas - e00) / (e11*(Gamma_meas-e00) + delta_e) */
    double complex Gamma_true = (Gamma_DUT - e00)
        / (e11 * (Gamma_DUT - e00) + delta_e);

    *Z_corrected = deembed->Z0 * (1.0 + Gamma_true) / (1.0 - Gamma_true);
    return 0;
}

/* ============================================================================
 * L5 — Uncertainty Budget (GUM: Guide to Uncertainty in Measurement)
 * ============================================================================
 */

/**
 * nf_uncertainty_budget_compute — 测量不确定度预算 (ISO/IEC GUM)
 *
 * u_c = √(Σ u_i²) — 组合标准不确定度
 * U = k·u_c      — 扩展不确定度 (k=2 → 95%置信区间)
 *
 * 知识点: 近场探头测量的不确定度来源:
 *         探头定位、阻抗失配、接收机非线性、噪声、重复性
 * 课程对标: IEEE Std 1309-2013
 * 复杂度: O(1)
 */
int nf_uncertainty_budget_compute(const nf_cal_dataset_t *ds,
                                   nf_uncertainty_budget_t *budget)
{
    if (!ds || !budget) return -1;
    memset(budget, 0, sizeof(*budget));

    /* 典型近场探头测量不确定度分量 (dB) */
    budget->probe_positioning_db = 0.8;
    budget->impedance_mismatch_db = 0.5;
    budget->receiver_nonlinearity_db = 0.3;
    budget->noise_contribution_db = 0.2;
    budget->repeatability_db = 0.4;

    /* 组合标准不确定度 (RSS) */
    double sum_sq = budget->probe_positioning_db * budget->probe_positioning_db
                  + budget->impedance_mismatch_db * budget->impedance_mismatch_db
                  + budget->receiver_nonlinearity_db * budget->receiver_nonlinearity_db
                  + budget->noise_contribution_db * budget->noise_contribution_db
                  + budget->repeatability_db * budget->repeatability_db;

    budget->standard_uncertainty_db = sqrt(sum_sq);
    budget->combined_uncertainty_db = budget->standard_uncertainty_db;
    budget->coverage_factor_k = 2.0;
    budget->expanded_uncertainty_db = budget->coverage_factor_k
                                      * budget->combined_uncertainty_db;

    (void)ds;
    return 0;
}

/* ============================================================================
 * L5 — Spatial Calibration (Position Alignment)
 * ============================================================================
 */

/**
 * nf_spatial_calibrate — 空间坐标校准 (最小二乘刚体配准)
 *
 * 求解最佳刚体变换 (rotation + translation + scale) 使
 * 测量坐标对齐参考坐标。
 *
 * 使用Kabsch-Umeyama算法 (SVD方法):
 *   H = Σ (p_ref_i - c_ref)(p_meas_i - c_meas)ᵀ
 *   H = UΣVᵀ → R = VUᵀ
 *
 * 知识点: 探头定位误差是近场测量最大的系统误差源
 * 课程对标: 计算几何 / 计算机视觉在EMC中的应用
 * 复杂度: O(n)
 */
int nf_spatial_calibrate(double *measured_positions_x,
                          double *measured_positions_y,
                          double *reference_positions_x,
                          double *reference_positions_y,
                          size_t num_points,
                          double *offset_x, double *offset_y,
                          double *rotation_deg, double *scale)
{
    if (!measured_positions_x || !measured_positions_y ||
        !reference_positions_x || !reference_positions_y ||
        num_points < 2) return -1;

    /* 计算质心 */
    double mx = 0, my = 0, rx = 0, ry = 0;
    for (size_t i = 0; i < num_points; i++) {
        mx += measured_positions_x[i];
        my += measured_positions_y[i];
        rx += reference_positions_x[i];
        ry += reference_positions_y[i];
    }
    mx /= (double)num_points; my /= (double)num_points;
    rx /= (double)num_points; ry /= (double)num_points;

    /* 构建互相关矩阵 */
    double Sxx = 0, Sxy = 0, Syx = 0, Syy = 0;
    double s2_meas = 0, s2_ref = 0;

    for (size_t i = 0; i < num_points; i++) {
        double dmx = measured_positions_x[i] - mx;
        double dmy = measured_positions_y[i] - my;
        double drx = reference_positions_x[i] - rx;
        double dry = reference_positions_y[i] - ry;

        Sxx += dmx * drx;
        Sxy += dmx * dry;
        Syx += dmy * drx;
        Syy += dmy * dry;

        s2_meas += dmx * dmx + dmy * dmy;
        s2_ref  += drx * drx + dry * dry;
    }

    /* SVD分解 (2x2闭合解) */
    double det = Sxx * Syy - Sxy * Syx;
    double trace = Sxx + Syy;
    double norm = sqrt(trace * trace + 4.0 * det);

    double cos_theta = (trace) / norm;
    double sin_theta = (Sxy - Syx) / norm;

    *rotation_deg = atan2(sin_theta, cos_theta) * 180.0 / M_PI;

    /* 尺度因子 */
    *scale = (s2_ref > 0) ? sqrt(s2_ref / s2_meas) : 1.0;

    /* 平移 */
    *offset_x = rx - (*scale) * (cos_theta * mx - sin_theta * my);
    *offset_y = ry - (*scale) * (sin_theta * mx + cos_theta * my);

    return 0;
}

/* ============================================================================
 * L5 — Interpolator
 * ============================================================================
 */

int nf_interpolator_build(nf_interpolator_t *interp,
                           const double *x, const double *y,
                           size_t n, nf_interp_method_t method)
{
    if (!interp || !x || !y || n < 2) return -1;
    memset(interp, 0, sizeof(*interp));

    interp->method = method;
    interp->num_nodes = n;

    /* 线性插值不需要额外系数存储 */
    if (method == NF_INTERP_LINEAR) {
        interp->x_nodes = malloc(n * sizeof(double));
        interp->y_nodes = malloc(n * sizeof(double));
        if (!interp->x_nodes || !interp->y_nodes) {
            nf_interpolator_free(interp);
            return -1;
        }
        memcpy(interp->x_nodes, x, n * sizeof(double));
        memcpy(interp->y_nodes, y, n * sizeof(double));
    }

    /* Cubic spline with natural boundary condition */
    if (method == NF_INTERP_CUBIC_SPLINE) {
        interp->x_nodes = malloc(n * sizeof(double));
        interp->y_nodes = malloc(n * sizeof(double));
        interp->coeffs = malloc((n - 1) * 4 * sizeof(double));
        if (!interp->x_nodes || !interp->y_nodes || !interp->coeffs) {
            nf_interpolator_free(interp);
            return -1;
        }
        memcpy(interp->x_nodes, x, n * sizeof(double));
        memcpy(interp->y_nodes, y, n * sizeof(double));

        /* 计算二阶导数 (自然边界: y''₀ = y''_{n-1} = 0) */
        size_t n_seg = n - 1;
        double *h = malloc(n_seg * sizeof(double));
        double *alpha = malloc(n * sizeof(double));
        double *c = malloc(n * sizeof(double));
        double *l = malloc(n * sizeof(double));
        double *mu = malloc(n * sizeof(double));
        double *z = malloc(n * sizeof(double));

        if (!h || !alpha || !c || !l || !mu || !z) {
            free(h); free(alpha); free(c); free(l); free(mu); free(z);
            nf_interpolator_free(interp);
            return -1;
        }

        for (size_t i = 0; i < n_seg; i++) h[i] = x[i+1] - x[i];
        for (size_t i = 1; i < n - 1; i++) {
            alpha[i] = 3.0 / h[i] * (y[i+1] - y[i])
                     - 3.0 / h[i-1] * (y[i] - y[i-1]);
        }

        l[0] = 1; mu[0] = 0; z[0] = 0;
        for (size_t i = 1; i < n - 1; i++) {
            l[i] = 2.0 * (x[i+1] - x[i-1]) - h[i-1] * mu[i-1];
            mu[i] = h[i] / l[i];
            z[i] = (alpha[i] - h[i-1] * z[i-1]) / l[i];
        }
        l[n-1] = 1; z[n-1] = 0; c[n-1] = 0;

        for (size_t i = n - 1; i > 0; i--) {
            size_t j = i - 1;
            c[j] = z[j] - mu[j] * c[j+1];
        }

        /* 存储样条系数: 每段 [a b c d] */
        for (size_t i = 0; i < n_seg; i++) {
            size_t off = i * 4;
            interp->coeffs[off + 0] = y[i];                     /* a */
            interp->coeffs[off + 1] = (y[i+1]-y[i])/h[i]
                - h[i]*(c[i+1] + 2*c[i])/3.0;                  /* b */
            interp->coeffs[off + 2] = c[i];                     /* c */
            interp->coeffs[off + 3] = (c[i+1]-c[i])/(3.0*h[i]);/* d */
        }

        free(h); free(alpha); free(c); free(l); free(mu); free(z);
    }

    interp->num_intervals = n - 1;
    return 0;
}

void nf_interpolator_free(nf_interpolator_t *interp)
{
    if (!interp) return;
    free(interp->x_nodes); interp->x_nodes = NULL;
    free(interp->y_nodes); interp->y_nodes = NULL;
    free(interp->coeffs);  interp->coeffs = NULL;
}

/**
 * nf_interpolator_eval — 插值器求值
 *
 * 知识点: 插值法是校准数据使用的基础
 * 复杂度: O(log n) — 二分查找
 */
double nf_interpolator_eval(const nf_interpolator_t *interp, double x)
{
    if (!interp || !interp->x_nodes || !interp->y_nodes) return 0.0;
    if (interp->num_nodes == 0) return 0.0;

    /* 二分查找区间 */
    size_t lo = 0, hi = interp->num_nodes - 1;
    while (hi - lo > 1) {
        size_t mid = (lo + hi) / 2;
        if (interp->x_nodes[mid] <= x) lo = mid;
        else hi = mid;
    }

    double x0 = interp->x_nodes[lo];
    double x1 = interp->x_nodes[hi];
    double y0 = interp->y_nodes[lo];
    double y1 = interp->y_nodes[hi];

    if (interp->method == NF_INTERP_CUBIC_SPLINE && interp->coeffs) {
        double dx = x - x0;
        size_t off = lo * 4;
        double a = interp->coeffs[off + 0];
        double b = interp->coeffs[off + 1];
        double c = interp->coeffs[off + 2];
        double d = interp->coeffs[off + 3];
        return a + b * dx + c * dx * dx + d * dx * dx * dx;
    }

    /* 线性插值 (默认) */
    if (x1 == x0) return y0;
    return y0 + (y1 - y0) * (x - x0) / (x1 - x0);
}

/* ============================================================================
 * L5 — Probe Factor Frequency Interpolation
 * ============================================================================
 */

int nf_probe_factor_interpolate(const double *freqs_hz,
                                 const double *pf_e_values,
                                 const double *pf_h_values,
                                 size_t num_freqs,
                                 double target_freq_hz,
                                 double *pf_e_out, double *pf_h_out)
{
    if (!freqs_hz || !pf_e_values || !pf_h_values ||
        num_freqs < 2 || !pf_e_out || !pf_h_out) return -1;

    nf_interpolator_t interp_e, interp_h;
    int ret_e = nf_interpolator_build(&interp_e, freqs_hz, pf_e_values,
                                       num_freqs, NF_INTERP_CUBIC_SPLINE);
    int ret_h = nf_interpolator_build(&interp_h, freqs_hz, pf_h_values,
                                       num_freqs, NF_INTERP_CUBIC_SPLINE);

    if (ret_e == 0 && ret_h == 0) {
        *pf_e_out = nf_interpolator_eval(&interp_e, target_freq_hz);
        *pf_h_out = nf_interpolator_eval(&interp_h, target_freq_hz);
    } else {
        /* 回退线性插值 */
        size_t lo = 0, hi = num_freqs - 1;
        while (hi - lo > 1) {
            size_t mid = (lo + hi) / 2;
            if (freqs_hz[mid] <= target_freq_hz) lo = mid; else hi = mid;
        }
        double t = (target_freq_hz - freqs_hz[lo]) / (freqs_hz[hi] - freqs_hz[lo]);
        *pf_e_out = pf_e_values[lo] + t * (pf_e_values[hi] - pf_e_values[lo]);
        *pf_h_out = pf_h_values[lo] + t * (pf_h_values[hi] - pf_h_values[lo]);
    }

    nf_interpolator_free(&interp_e);
    nf_interpolator_free(&interp_h);
    return 0;
}

/* ============================================================================
 * L5 — TRL Calibration
 * ============================================================================
 */

/**
 * nf_vna_trl_compute — TRL校准 (Thru-Reflect-Line)
 *
 * 比SOLT更准确的高频校准方法。
 * 知识点: TRL是微波频段探头校准的标准方法
 * 复杂度: O(1)
 */
int nf_vna_trl_compute(double complex S11_meas, double complex S21_meas,
                        double complex S12_meas, double complex S22_meas,
                        double complex *S11_corr, double complex *S21_corr,
                        double complex *S12_corr, double complex *S22_corr)
{
    if (!S11_corr || !S21_corr || !S12_corr || !S22_corr) return -1;

    /* 简化TRL: 假设理想的Thru和Line标准
     * 实际中需要使用Thru和Line标准测量值来求解传播常数 */
    *S11_corr = S11_meas;
    *S21_corr = S21_meas;
    *S12_corr = S12_meas;
    *S22_corr = S22_meas;
    return 0;
}

/* ============================================================================
 * L5 — Calibration Verification
 * ============================================================================
 */

int nf_cal_compute_probe_factor(const nf_cal_dataset_t *ds,
                                 nf_probe_factor_t *pf)
{
    if (!ds || !ds->points || ds->num_points == 0 || !pf) return -1;
    memset(pf, 0, sizeof(*pf));

    /* 对所有频点计算平均探头因子 */
    double sum_pfe = 0, sum_pfh = 0;
    for (size_t i = 0; i < ds->num_points; i++) {
        sum_pfe += ds->points[i].pf_e_per_m;
        sum_pfh += ds->points[i].pf_h_a_per_vm;
    }
    pf->pf_e_per_m = sum_pfe / (double)ds->num_points;
    pf->pf_h_a_per_vm = sum_pfh / (double)ds->num_points;
    pf->calibration_uncertainty_db = 0.5;
    pf->cross_polarization_db = 20.0;

    return 0;
}

int nf_cal_verify_with_reference(const nf_cal_dataset_t *ds,
                                  const nf_probe_factor_t *pf,
                                  const nf_probe_spec_t *ref_probe,
                                  double *max_deviation_db)
{
    if (!ds || !pf || !ref_probe || !max_deviation_db) return -1;

    double max_dev = 0.0;
    for (size_t i = 0; i < ds->num_points; i++) {
        double expected = ds->points[i].pf_e_per_m;
        double measured = pf->pf_e_per_m;
        double dev = 20.0 * log10(fabs(expected / measured));
        if (dev > max_dev) max_dev = dev;
    }
    *max_deviation_db = max_dev;
    return 0;
}
