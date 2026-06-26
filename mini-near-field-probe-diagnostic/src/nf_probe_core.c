/**
 * @file nf_probe_core.c
 * @brief Near-Field Probe Core — L1-L2 implementation
 *
 * 所有实现基于实际物理公式，每个函数 = 一个独立知识点。
 *
 * L1: 场区划分、探头参数、空间分辨率
 * L2: 互易性、扰动分析、天线因子、波阻抗
 */

#include "../include/nf_probe_core.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ============================================================================
 * L1 — 场区划分 (Region Classification)
 * ============================================================================
 */

/**
 * nf_region_classify — IEEE Std 145-2013 场区划分
 *
 * 反应近场: r < λ/(2π)
 * 辐射近场: λ/(2π) ≤ r < 2D²/λ
 * 远场:     r ≥ 2D²/λ
 *
 * 知识点: 天线近场/远场边界定义
 * 课程对标: MIT 6.630 EM Waves / Stanford EE252 Antennas
 * 复杂度: O(1)
 */
nf_region_t nf_region_classify(double r, double lambda, double D_max)
{
    double reactive_boundary = lambda / (2.0 * M_PI);
    double fraunhofer_boundary = 2.0 * D_max * D_max / lambda;

    if (r < reactive_boundary) {
        return NF_REGION_REACTIVE_NEAR;
    } else if (r < fraunhofer_boundary) {
        return NF_REGION_RADIATING_NEAR;
    } else {
        return NF_REGION_FRAUNHOFER;
    }
}

const char* nf_region_name(nf_region_t region)
{
    switch (region) {
        case NF_REGION_REACTIVE_NEAR:  return "Reactive Near-Field";
        case NF_REGION_RADIATING_NEAR: return "Radiating Near-Field (Fresnel)";
        case NF_REGION_FRAUNHOFER:     return "Far-Field (Fraunhofer)";
        default: return "Unknown";
    }
}

/* ============================================================================
 * L1 — 探头参数初始化
 * ============================================================================
 */

/**
 * nf_probe_spec_init — 探头参数结构体初始化
 *
 * 为每种探头类型填充合理的默认参数。
 * 知识点: 不同类型近场探头的物理参数
 * 复杂度: O(1)
 */
void nf_probe_spec_init(nf_probe_spec_t *spec, nf_probe_type_t type)
{
    if (!spec) return;
    memset(spec, 0, sizeof(*spec));
    spec->type = type;

    switch (type) {
        case NF_PROBE_H_FIELD_LOOP:
            spec->orientation = NF_ORIENT_TANGENTIAL_H;
            spec->freq_min_hz = 1.0e5;
            spec->freq_max_hz = 3.0e9;
            spec->spatial_res_m = 2.0e-3;
            spec->sensitivity_v_per_am = 1.0e-3;
            spec->sensitivity_v_per_vm = 0.0;
            spec->specific.h_loop.radius_m = 2.0e-3;
            spec->specific.h_loop.wire_radius_m = 0.1e-3;
            spec->specific.h_loop.turns = 3;
            spec->specific.h_loop.self_inductance_H = 50e-9;
            spec->specific.h_loop.self_capacitance_F = 0.5e-12;
            spec->specific.h_loop.resonance_freq_Hz = 1.0e9;
            spec->specific.h_loop.shielding_effectiveness_db = 20.0;
            break;

        case NF_PROBE_E_FIELD_MONOPOLE:
            spec->orientation = NF_ORIENT_NORMAL_E;
            spec->freq_min_hz = 1.0e5;
            spec->freq_max_hz = 6.0e9;
            spec->spatial_res_m = 1.0e-3;
            spec->sensitivity_v_per_am = 0.0;
            spec->sensitivity_v_per_vm = 1.0e-3;
            spec->specific.e_monopole.length_m = 5.0e-3;
            spec->specific.e_monopole.effective_height_m = 2.5e-3;
            spec->specific.e_monopole.tip_capacitance_F = 0.1e-12;
            spec->specific.e_monopole.input_impedance_ohm = 1.0e6;
            spec->specific.e_monopole.bandwidth_hz_lo = 1.0e5;
            spec->specific.e_monopole.bandwidth_hz_hi = 6.0e9;
            break;

        case NF_PROBE_E_FIELD_DIPOLE:
            spec->orientation = NF_ORIENT_TANGENTIAL_H;
            spec->freq_min_hz = 1.0e6;
            spec->freq_max_hz = 3.0e9;
            spec->spatial_res_m = 2.0e-3;
            spec->sensitivity_v_per_am = 0.0;
            spec->sensitivity_v_per_vm = 0.5e-3;
            break;

        case NF_PROBE_MICROSTRIP:
            spec->orientation = NF_ORIENT_TANGENTIAL_H;
            spec->freq_min_hz = 1.0e6;
            spec->freq_max_hz = 6.0e9;
            spec->spatial_res_m = 5.0e-4;
            spec->sensitivity_v_per_am = 5.0e-3;
            spec->sensitivity_v_per_vm = 1.0e-3;
            break;

        case NF_PROBE_TEM_CELL:
            spec->orientation = NF_ORIENT_ISOTROPIC;
            spec->freq_min_hz = 1.0e4;
            spec->freq_max_hz = 1.0e9;
            spec->spatial_res_m = 1.0e-2;
            spec->sensitivity_v_per_am = 1.0e-4;
            spec->sensitivity_v_per_vm = 1.0e-4;
            break;

        case NF_PROBE_OPTICAL_EO:
            spec->orientation = NF_ORIENT_ISOTROPIC;
            spec->freq_min_hz = 1.0e3;
            spec->freq_max_hz = 4.0e10;
            spec->spatial_res_m = 1.0e-5;
            spec->sensitivity_v_per_am = 1.0e-5;
            spec->sensitivity_v_per_vm = 1.0e-2;
            break;

        case NF_PROBE_THERMAL:
            spec->orientation = NF_ORIENT_ISOTROPIC;
            spec->freq_min_hz = 1.0e7;
            spec->freq_max_hz = 1.0e11;
            spec->spatial_res_m = 1.0e-4;
            spec->sensitivity_v_per_am = 1.0e-6;
            spec->sensitivity_v_per_vm = 1.0e-3;
            break;

        default:
            break;
    }
}

/**
 * nf_probe_spec_validate — 验证探头参数合理性
 *
 * 检查参数是否在物理可行范围内。
 * 知识点: 探头参数物理约束
 * 复杂度: O(1)
 */
int nf_probe_spec_validate(const nf_probe_spec_t *spec)
{
    if (!spec) return 0;

    if (spec->freq_min_hz <= 0 || spec->freq_max_hz <= 0) return 0;
    if (spec->freq_min_hz > spec->freq_max_hz) return 0;
    if (spec->spatial_res_m <= 0) return 0;

    switch (spec->type) {
        case NF_PROBE_H_FIELD_LOOP:
            if (spec->specific.h_loop.radius_m <= 0) return 0;
            if (spec->specific.h_loop.turns <= 0) return 0;
            break;
        case NF_PROBE_E_FIELD_MONOPOLE:
            if (spec->specific.e_monopole.length_m <= 0) return 0;
            if (spec->specific.e_monopole.effective_height_m <= 0) return 0;
            break;
        default:
            break;
    }
    return 1;
}

/* ============================================================================
 * L1 — 频率/波长转换
 * ============================================================================
 */

/**
 * nf_freq_to_wavelength — 频率转波长
 * 知识点: λ = c₀ / f
 * 复杂度: O(1)
 */
double nf_freq_to_wavelength(double freq_hz)
{
    if (freq_hz <= 0.0) return INFINITY;
    return C_0 / freq_hz;
}

/**
 * nf_wavelength_to_freq — 波长转频率
 * 知识点: f = c₀ / λ
 * 复杂度: O(1)
 */
double nf_wavelength_to_freq(double lambda_m)
{
    if (lambda_m <= 0.0) return INFINITY;
    return C_0 / lambda_m;
}

/* ============================================================================
 * L1 — 空间分辨率 (Spatial Resolution)
 * ============================================================================
 */

/**
 * nf_spatial_resolution — 探头空间分辨率估计
 *
 * 经验公式: δ ≈ √(d_probe² + 4·h²)
 * 其中 d_probe = 探头直径, h = 探头高度
 *
 * 知识点: 近场探头空间分辨率受探头尺寸和距离共同限制
 * 课程对标: Michigan EECS 511 Microwave / Paul EMC Ch.11
 * 复杂度: O(1)
 */
double nf_spatial_resolution(double probe_diameter_m, double distance_m)
{
    return sqrt(probe_diameter_m * probe_diameter_m
                + 4.0 * distance_m * distance_m);
}

/* ============================================================================
 * L1 — H场环探头灵敏度
 * ============================================================================
 */

/**
 * nf_h_loop_sensitivity — 法拉第定律计算H场环探头输出电压
 *
 * V_out = ω · μ₀ · N · π · r² · |H_inc|
 *
 * 知识点: 法拉第电磁感应定律在近场探头中的应用
 * 课程对标: MIT 6.630 / Berkeley EE117 EM
 * 复杂度: O(1)
 */
double nf_h_loop_sensitivity(double radius_m, int turns,
                              double freq_hz, double H_incident_Apm)
{
    double omega = 2.0 * M_PI * freq_hz;
    double area = M_PI * radius_m * radius_m;
    double flux_density = MU_0 * H_incident_Apm;
    double flux = flux_density * area;
    double voltage_per_turn = omega * flux;
    return voltage_per_turn * (double)turns;
}

/* ============================================================================
 * L1 — E场单极探头灵敏度
 * ============================================================================
 */

/**
 * nf_e_monopole_sensitivity — 电小单极探头输出电压
 *
 * V_out = E_inc · h_eff
 *
 * 知识点: 电小天线在近场中的接收特性
 * 复杂度: O(1)
 */
double nf_e_monopole_sensitivity(double effective_height_m,
                                  double E_incident_Vpm)
{
    return E_incident_Vpm * effective_height_m;
}

/* ============================================================================
 * L2 — 天线因子 (Antenna Factor)
 * ============================================================================
 */

/**
 * nf_antenna_factor_farfield — 远场天线因子计算 (C63.2)
 *
 * AF_dB = 20·log₁₀(f_MHz) - 29.77 - G_dBi
 *
 * 推导: 基于Friis传输公式和天线互易性
 *
 * 知识点: IEEE C63.2 天线校准天线因子定义
 * 课程对标: Stanford EE359 Wireless
 * 复杂度: O(1)
 */
double nf_antenna_factor_farfield(double freq_hz, double gain_dbi)
{
    double freq_MHz = freq_hz / 1.0e6;
    if (freq_MHz <= 0.0) return INFINITY;
    double af_db = 20.0 * log10(freq_MHz) - 29.77 - gain_dbi;
    return af_db;
}

/* ============================================================================
 * L2 — 探头因子 (Probe Factor, 近场专用)
 * ============================================================================
 */

/**
 * nf_probe_factor_e_field — E场探头因子
 *
 * PF_E = E_incident / V_out   (1/m)
 *
 * 知识点: 近场探头校准中的探头因子定义 (IEC 61967-3)
 * 复杂度: O(1)
 */
double nf_probe_factor_e_field(double v_out_V, double e_incident_Vpm)
{
    if (v_out_V == 0.0) return INFINITY;
    return e_incident_Vpm / v_out_V;
}

/**
 * nf_probe_factor_h_field — H场探头因子
 *
 * PF_H = H_incident / V_out   (A/(V·m))
 *
 * 知识点: H场探头校准的探头因子
 * 复杂度: O(1)
 */
double nf_probe_factor_h_field(double v_out_V, double h_incident_Apm)
{
    if (v_out_V == 0.0) return INFINITY;
    return h_incident_Apm / v_out_V;
}

/* ============================================================================
 * L2 — 互易性验证 (Reciprocity Check)
 * ============================================================================
 */

/**
 * nf_reciprocity_check — Lorentz 互易性定理验证
 *
 * 检查: |tx_impedance - rx_impedance| < ε
 * 和:   |tx_gain - rx_gain| < ε
 *
 * 知识点: 电磁互易性定理 (Lorentz Reciprocity)
 * 条件: 线性、各向同性、无源介质
 * 课程对标: Berkeley EE117 EM / ETH 227-0455
 * 复杂度: O(1)
 */
int nf_reciprocity_check(const nf_reciprocity_t *rcp)
{
    if (!rcp) return 0;

    double z_diff = cabs(rcp->tx_impedance - rcp->rx_impedance);
    double z_avg = cabs(rcp->tx_impedance + rcp->rx_impedance) * 0.5;
    double g_diff = fabs(rcp->tx_gain_db - rcp->rx_gain_db);

    int z_ok = (z_avg > 0.0) ? (z_diff / z_avg < 0.01) : (z_diff < 1e-12);
    int g_ok = (g_diff < 0.1);

    return z_ok && g_ok;
}

/* ============================================================================
 * L2 — 扰动分析 (Perturbation Analysis)
 * ============================================================================
 */

/**
 * nf_perturbation_estimate — 探头对被测场的扰动估计
 *
 * 经验模型:
 *   扰动误差 (%) ≈ 100 × (d_probe / λ) × (λ / (2πd_sep))²
 *
 * 其中 d_probe = 探头尺寸, d_sep = 探头-DUT距离
 *
 * 知识点: 探头的场扰动效应是近场测量最大误差源之一
 * 参考: Kanda (1993) IEEE Trans EMC "Standard Probes for EM Field Measurements"
 * 复杂度: O(1)
 */
double nf_perturbation_estimate(double probe_size_m, double freq_hz,
                                 double distance_m)
{
    double lambda = C_0 / freq_hz;
    if (lambda <= 0.0 || distance_m <= 0.0) return 100.0;

    double electrical_size = probe_size_m / lambda;
    double near_field_factor = lambda / (2.0 * M_PI * distance_m);
    double perturbation = 100.0 * electrical_size
                          * near_field_factor * near_field_factor;

    if (perturbation > 100.0) perturbation = 100.0;
    return perturbation;
}

/* ============================================================================
 * L2 — 波阻抗 (Wave Impedance)
 * ============================================================================
 */

/**
 * nf_wave_impedance_nearfield — 近场波阻抗近似
 *
 * 电偶极子源 (高阻抗):
 *   |Z_w| ≈ η₀ · (λ / (2πr))   当 r << λ
 *
 * 磁偶极子源 (低阻抗):
 *   |Z_w| ≈ η₀ · (2πr / λ)     当 r << λ
 *
 * 知识点: 近场波阻抗不同于自由空间 η₀
 *         取决于源类型和距离
 * 课程对标: MIT 6.630 / Paul EMC Ch.8
 * 复杂度: O(1)
 */
double nf_wave_impedance_nearfield(double r, double lambda,
                                    int is_electric_source)
{
    if (r <= 0.0 || lambda <= 0.0) return ETA_0;

    double kr = 2.0 * M_PI * r / lambda;

    if (kr < 0.1) {
        /* 极近场区域 — 使用准静态近似 */
        if (is_electric_source) {
            return ETA_0 / kr;
        } else {
            return ETA_0 * kr;
        }
    }

    /* 过渡区域 — 使用精确公式 */
    double kr2 = kr * kr;
    double kr3 = kr * kr * kr;
    double num, den;

    if (is_electric_source) {
        num = sqrt(1.0 + 1.0 / kr2 + 1.0 / (kr2 * kr2));
        den = sqrt(1.0 + 1.0 / kr2);
    } else {
        num = sqrt(1.0 + 1.0 / kr2);
        den = sqrt(1.0 + 1.0 / kr2 + 1.0 / (kr2 * kr2));
    }

    double result = ETA_0 * num / den;
    if (isfinite(result)) return result;
    return ETA_0;
}

/**
 * nf_wave_impedance_schelkunoff — Schelkunoff波阻抗公式
 *
 * 通用波阻抗公式，适用于任意距离和任意源阻抗:
 *   Z_w(r) = η₀ · (Z_s + jη₀ tan(kr)) / (η₀ + jZ_s tan(kr))
 *
 * 其中 Z_s = 源阻抗 (Ω)
 *
 * 知识点: Schelkunoff 阻抗变换公式描述近场到远场的波阻抗过渡
 * 课程对标: Stanford EE252 Antennas / Illinois ECE 451
 * 复杂度: O(1)
 */
double nf_wave_impedance_schelkunoff(double r, double lambda,
                                      double source_impedance_ohm)
{
    if (r <= 0.0 || lambda <= 0.0) return 0.0;

    double k = 2.0 * M_PI / lambda;
    double kr = k * r;
    double tan_kr = tan(kr);

    double complex Zs = source_impedance_ohm;
    double complex numerator = Zs + I * ETA_0 * tan_kr;
    double complex denominator = ETA_0 + I * Zs * tan_kr;

    double complex Zw = ETA_0 * numerator / denominator;
    return cabs(Zw);
}

/* ============================================================================
 * 打印/调试辅助函数
 * ============================================================================
 */

void nf_probe_spec_print(const nf_probe_spec_t *spec)
{
    if (!spec) { printf("NULL probe spec\n"); return; }
    printf("Probe: %s by %s\n", spec->model, spec->manufacturer);
    printf("Type: %d | Freq: %.2e - %.2e Hz\n",
           spec->type, spec->freq_min_hz, spec->freq_max_hz);
    printf("Spatial Res: %.2e m | Sensitivity E: %.2e V/(V/m) H: %.2e V/(A/m)\n",
           spec->spatial_res_m, spec->sensitivity_v_per_vm,
           spec->sensitivity_v_per_am);
}

void nf_scan_grid_print(const nf_scan_grid_t *grid)
{
    if (!grid) { printf("NULL grid\n"); return; }
    printf("Grid: X[%.4f:%.4f] x%zu | Y[%.4f:%.4f] x%zu\n",
           grid->x_start_m, grid->x_end_m, grid->nx,
           grid->y_start_m, grid->y_end_m, grid->ny);
    printf("Step: dx=%.4e dy=%.4e | Z=%.4e\n",
           grid->dx_m, grid->dy_m, grid->z_height_m);
}

void nf_field_sample_print(const nf_field_sample_t *sample)
{
    if (!sample) { printf("NULL sample\n"); return; }
    printf("E: (%.4e%+.4ej, %.4e%+.4ej, %.4e%+.4ej) V/m\n",
           creal(sample->Ex), cimag(sample->Ex),
           creal(sample->Ey), cimag(sample->Ey),
           creal(sample->Ez), cimag(sample->Ez));
    printf("H: (%.4e%+.4ej, %.4e%+.4ej, %.4e%+.4ej) A/m\n",
           creal(sample->Hx), cimag(sample->Hx),
           creal(sample->Hy), cimag(sample->Hy),
           creal(sample->Hz), cimag(sample->Hz));
}
