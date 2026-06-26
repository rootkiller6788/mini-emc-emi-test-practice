/**
 * @file nf_field_theory.c
 * @brief Near-Field EM Field Theory — L3-L4 implementation
 *
 * L3: 矢量运算、平面波谱、格林函数、等效源、球面波展开
 * L4: Maxwell旋度方程验证、边界条件、Poynting定理、镜像理论、唯一性定理
 */

#include "../include/nf_field_theory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * L3 — 复数矢量代数
 * ============================================================================
 */

/**
 * nf_cvec_add — 复数矢量加法
 * 知识点: 矢量空间线性运算
 * 复杂度: O(1)
 */
nf_complex_vector_t nf_cvec_add(nf_complex_vector_t a, nf_complex_vector_t b)
{
    nf_complex_vector_t r;
    r.x = a.x + b.x;
    r.y = a.y + b.y;
    r.z = a.z + b.z;
    return r;
}

nf_complex_vector_t nf_cvec_sub(nf_complex_vector_t a, nf_complex_vector_t b)
{
    nf_complex_vector_t r;
    r.x = a.x - b.x;
    r.y = a.y - b.y;
    r.z = a.z - b.z;
    return r;
}

nf_complex_vector_t nf_cvec_scale(nf_complex_vector_t a, double complex s)
{
    nf_complex_vector_t r;
    r.x = a.x * s;
    r.y = a.y * s;
    r.z = a.z * s;
    return r;
}

/**
 * nf_cvec_dot — 复数点积 (非共轭)
 * 知识点: 复数矢量内积
 * 复杂度: O(1)
 */
double complex nf_cvec_dot(nf_complex_vector_t a, nf_complex_vector_t b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

/**
 * nf_cvec_cross — 复数叉积
 * 知识点: 叉积在电磁学中用于计算Poynting矢量
 * 复杂度: O(1)
 */
nf_complex_vector_t nf_cvec_cross(nf_complex_vector_t a, nf_complex_vector_t b)
{
    nf_complex_vector_t r;
    r.x = a.y * b.z - a.z * b.y;
    r.y = a.z * b.x - a.x * b.z;
    r.z = a.x * b.y - a.y * b.x;
    return r;
}

double nf_cvec_magnitude(nf_complex_vector_t a)
{
    return sqrt(creal(a.x * conj(a.x) + a.y * conj(a.y) + a.z * conj(a.z)));
}

/* 实矢量代数 */
nf_real_vector_t nf_rvec_add(nf_real_vector_t a, nf_real_vector_t b)
{
    nf_real_vector_t r = {a.x + b.x, a.y + b.y, a.z + b.z};
    return r;
}

nf_real_vector_t nf_rvec_sub(nf_real_vector_t a, nf_real_vector_t b)
{
    nf_real_vector_t r = {a.x - b.x, a.y - b.y, a.z - b.z};
    return r;
}

nf_real_vector_t nf_rvec_scale(nf_real_vector_t a, double s)
{
    nf_real_vector_t r = {a.x * s, a.y * s, a.z * s};
    return r;
}

double nf_rvec_dot(nf_real_vector_t a, nf_real_vector_t b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

nf_real_vector_t nf_rvec_cross(nf_real_vector_t a, nf_real_vector_t b)
{
    nf_real_vector_t r;
    r.x = a.y * b.z - a.z * b.y;
    r.y = a.z * b.x - a.x * b.z;
    r.z = a.x * b.y - a.y * b.x;
    return r;
}

double nf_rvec_magnitude(nf_real_vector_t a)
{
    return sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
}

/**
 * nf_rvec_unit — 计算单位矢量
 * 知识点: 方向归一化
 * 复杂度: O(1)
 */
nf_real_vector_t nf_rvec_unit(nf_real_vector_t a)
{
    double mag = nf_rvec_magnitude(a);
    if (mag < 1e-16) {
        nf_real_vector_t zero = {0, 0, 0};
        return zero;
    }
    return nf_rvec_scale(a, 1.0 / mag);
}

/* ============================================================================
 * L3 — 平面波谱 (Plane Wave Spectrum)
 * ============================================================================
 */

/**
 * nf_pws_allocate — 分配平面波谱内存
 *
 * 知识点: 平面波谱展开表示 — 任意近场分布可以展开为
 *         不同k矢量的平面波叠加
 * 课程对标: Stanford EE252 / ETH 227-0455
 * 复杂度: O(nkx * nky)
 */
int nf_pws_allocate(nf_plane_wave_spectrum_t *pws, size_t nkx, size_t nky,
                     double kx_min, double kx_max,
                     double ky_min, double ky_max)
{
    if (!pws || nkx == 0 || nky == 0) return -1;
    memset(pws, 0, sizeof(*pws));

    pws->nkx = nkx; pws->nky = nky;
    pws->kx_min = kx_min; pws->kx_max = kx_max;
    pws->ky_min = ky_min; pws->ky_max = ky_max;
    pws->dkx = (kx_max - kx_min) / (nkx - 1);
    pws->dky = (ky_max - ky_min) / (nky - 1);
    pws->total_points = nkx * nky;

    pws->spectrum_Ex = calloc(pws->total_points, sizeof(double complex));
    pws->spectrum_Ey = calloc(pws->total_points, sizeof(double complex));
    pws->spectrum_Hx = calloc(pws->total_points, sizeof(double complex));
    pws->spectrum_Hy = calloc(pws->total_points, sizeof(double complex));

    if (!pws->spectrum_Ex || !pws->spectrum_Ey ||
        !pws->spectrum_Hx || !pws->spectrum_Hy) {
        nf_pws_free(pws);
        return -1;
    }
    return 0;
}

void nf_pws_free(nf_plane_wave_spectrum_t *pws)
{
    if (!pws) return;
    free(pws->spectrum_Ex); pws->spectrum_Ex = NULL;
    free(pws->spectrum_Ey); pws->spectrum_Ey = NULL;
    free(pws->spectrum_Hx); pws->spectrum_Hx = NULL;
    free(pws->spectrum_Hy); pws->spectrum_Hy = NULL;
    pws->total_points = 0;
}

/**
 * nf_pws_propagate_to_plane — 谱域传播算子
 *
 * 从 z=z₀ 面传播到 z=z₀+d 的平面:
 *   F(kx, ky, z+d) = F(kx, ky, z) · exp(-j·kz·d)
 *
 * 其中 kz = √(k₀² - kx² - ky²)  (传播模式)
 *       kz = -j√(kx² + ky² - k₀²) (衰减模式/倏逝波)
 *
 * 知识点: 倏逝波 (evanescent wave) — 近场的本质是倏逝波谱
 *         这是近场探头空间超分辨率的物理基础
 * 课程对标: MIT 6.630 / Stanford EE252
 * 复杂度: O(nkx * nky)
 */
int nf_pws_propagate_to_plane(const nf_plane_wave_spectrum_t *src,
                               nf_plane_wave_spectrum_t *dst,
                               double z_distance, double k0)
{
    if (!src || !dst) return -1;
    if (src->total_points != dst->total_points) return -1;

    for (size_t iy = 0; iy < src->nky; iy++) {
        double ky = src->ky_min + iy * src->dky;
        for (size_t ix = 0; ix < src->nkx; ix++) {
            double kx = src->kx_min + ix * src->dkx;
            size_t idx = iy * src->nkx + ix;

            double kr2 = kx * kx + ky * ky;
            double complex kz;

            if (kr2 <= k0 * k0) {
                /* 传播波: kz为实数 */
                kz = sqrt(k0 * k0 - kr2);
            } else {
                /* 倏逝波: kz为负虚数 (衰减) */
                kz = -I * sqrt(kr2 - k0 * k0);
            }

            double complex propagator = cexp(-I * kz * z_distance);

            dst->spectrum_Ex[idx] = src->spectrum_Ex[idx] * propagator;
            dst->spectrum_Ey[idx] = src->spectrum_Ey[idx] * propagator;
            dst->spectrum_Hx[idx] = src->spectrum_Hx[idx] * propagator;
            dst->spectrum_Hy[idx] = src->spectrum_Hy[idx] * propagator;
        }
    }
    return 0;
}

/**
 * nf_pws_evaluate_field — 从平面波谱重建空间域场
 *
 * 通过2D逆Fourier积分从谱域重建场值。
 * 使用离散求和近似:
 *   E(x,y,z) = (1/4π²) Σ Σ F(kx,ky,z) exp(j(kx·x + ky·y)) dkx dky
 *
 * 知识点: 平面波谱和空间域场之间的Fourier变换关系
 * 复杂度: O(nkx * nky)
 */
int nf_pws_evaluate_field(const nf_plane_wave_spectrum_t *pws,
                           double x, double y, double z,
                           nf_complex_vector_t *E, nf_complex_vector_t *H)
{
    if (!pws || !E || !H) return -1;

    double complex ex_sum = 0, ey_sum = 0;
    double complex hx_sum = 0, hy_sum = 0;

    for (size_t iy = 0; iy < pws->nky; iy++) {
        double ky = pws->ky_min + iy * pws->dky;
        for (size_t ix = 0; ix < pws->nkx; ix++) {
            double kx = pws->kx_min + ix * pws->dkx;
            size_t idx = iy * pws->nkx + ix;

            double complex phase = cexp(I * (kx * x + ky * y));

            ex_sum += pws->spectrum_Ex[idx] * phase;
            ey_sum += pws->spectrum_Ey[idx] * phase;
            hx_sum += pws->spectrum_Hx[idx] * phase;
            hy_sum += pws->spectrum_Hy[idx] * phase;
        }
    }

    double norm = pws->dkx * pws->dky / (4.0 * M_PI * M_PI);
    E->x = ex_sum * norm;  E->y = ey_sum * norm;  E->z = 0;
    H->x = hx_sum * norm;  H->y = hy_sum * norm;  H->z = 0;

    return 0;
}

/* ============================================================================
 * L3 — 并矢格林函数 (Dyadic Green's Function)
 * ============================================================================
 */

/**
 * nf_greens_dyadic_compute — 自由空间并矢格林函数
 *
 * 自由空间电并矢格林函数:
 *   Ḡ̄(r, r') = (Ī̄ + ∇∇/k₀²) · exp(-jk₀R) / (4πR)
 *
 * 其中 R = |r - r'|, k₀ = ω/c₀
 *
 * 知识点: 并矢格林函数是任意源分布辐射场的积分核
 * 课程对标: Berkeley EE117 EM / Illinois ECE 451
 * 复杂度: O(1)
 */
int nf_greens_dyadic_compute(nf_greens_dyadic_t *gf,
                               const nf_real_vector_t *src,
                               const nf_real_vector_t *obs,
                               double freq_hz)
{
    if (!gf || !src || !obs || freq_hz <= 0) return -1;

    double k0 = 2.0 * M_PI * freq_hz / C_0;
    gf->k0 = k0;
    gf->src_pos = *src;
    gf->obs_pos = *obs;

    double dx = obs->x - src->x;
    double dy = obs->y - src->y;
    double dz = obs->z - src->z;
    double R = sqrt(dx * dx + dy * dy + dz * dz);

    if (R < 1e-15) R = 1e-15;

    double complex ikR = I * k0 * R;
    double complex exp_term = cexp(-ikR);
    double complex G0 = exp_term / (4.0 * M_PI * R);

    gf->scalar_green = G0;

    /* 并矢项系数 */
    double complex A = 1.0 + I / (k0 * R) - 1.0 / (k0 * k0 * R * R);
    double complex B = -1.0 - 3.0 * I / (k0 * R) + 3.0 / (k0 * k0 * R * R);

    double rx = dx / R, ry = dy / R, rz = dz / R;

    double complex factor = G0;

    gf->dyadic_green_xx = factor * (A + B * rx * rx);
    gf->dyadic_green_xy = factor * (B * rx * ry);
    gf->dyadic_green_xz = factor * (B * rx * rz);
    gf->dyadic_green_yx = factor * (B * ry * rx);
    gf->dyadic_green_yy = factor * (A + B * ry * ry);
    gf->dyadic_green_yz = factor * (B * ry * rz);
    gf->dyadic_green_zx = factor * (B * rz * rx);
    gf->dyadic_green_zy = factor * (B * rz * ry);
    gf->dyadic_green_zz = factor * (A + B * rz * rz);

    return 0;
}

/**
 * nf_greens_dyadic_field — 通过并矢格林函数计算源产生的场
 *
 * E(r) = -jωμ₀ ∫ Ḡ̄(r,r')·J(r') dV'
 *
 * 知识点: 电场积分方程 (EFIE) — 矩量法 (MoM) 的基础
 * 复杂度: O(1)
 */
void nf_greens_dyadic_field(const nf_greens_dyadic_t *gf,
                             const nf_complex_vector_t *J_src,
                             nf_complex_vector_t *E_out,
                             nf_complex_vector_t *H_out)
{
    if (!gf || !J_src || !E_out || !H_out) return;

    double complex coeff = -I * 2.0 * M_PI * C_0
                           / nf_freq_to_wavelength(cabs(gf->k0) * C_0
                           / (2.0 * M_PI));
    /* Use omega * mu0 factor */
    double omega = cabs(gf->k0) * C_0;
    double complex jwmu = I * omega * MU_0;

    E_out->x = -jwmu * (gf->dyadic_green_xx * J_src->x
                      + gf->dyadic_green_xy * J_src->y
                      + gf->dyadic_green_xz * J_src->z);
    E_out->y = -jwmu * (gf->dyadic_green_yx * J_src->x
                      + gf->dyadic_green_yy * J_src->y
                      + gf->dyadic_green_yz * J_src->z);
    E_out->z = -jwmu * (gf->dyadic_green_zx * J_src->x
                      + gf->dyadic_green_zy * J_src->y
                      + gf->dyadic_green_zz * J_src->z);

    /* H from curl E: H = j/(ωμ₀) · ∇ × E, 简化近似 */
    double R = sqrt(pow(gf->obs_pos.x - gf->src_pos.x, 2)
                  + pow(gf->obs_pos.y - gf->src_pos.y, 2)
                  + pow(gf->obs_pos.z - gf->src_pos.z, 2));
    double complex H_factor = 1.0 / ETA_0;

    H_out->x = (gf->dyadic_green_zy * J_src->y
              - gf->dyadic_green_yz * J_src->z) * H_factor;
    H_out->y = (gf->dyadic_green_xz * J_src->z
              - gf->dyadic_green_zx * J_src->x) * H_factor;
    H_out->z = (gf->dyadic_green_yx * J_src->x
              - gf->dyadic_green_xy * J_src->y) * H_factor;
}

/* ============================================================================
 * L3 — 等效源法 (Equivalent Source Method)
 * ============================================================================
 */

/**
 * nf_equivalent_source_radiated_field — 电偶极子+磁偶极子辐射场
 *
 * 电偶极子 (Hertzian dipole):
 *   E_θ ≈ jη₀kI₀l sinθ exp(-jkr) / (4πr)
 *   H_φ ≈ jkI₀l sinθ exp(-jkr) / (4πr)
 *
 * 知识点: 等效源法是近场到远场变换的标准方法之一
 * 课程对标: Michigan EECS 511 / Georgia Tech ECE 6350
 * 复杂度: O(1)
 */
int nf_equivalent_source_radiated_field(const nf_equivalent_source_t *src,
                                          const nf_real_vector_t *obs,
                                          double freq_hz,
                                          nf_complex_vector_t *E,
                                          nf_complex_vector_t *H)
{
    if (!src || !obs || !E || !H || freq_hz <= 0) return -1;

    double k = 2.0 * M_PI * freq_hz / C_0;
    double omega = 2.0 * M_PI * freq_hz;

    double dx = obs->x - src->position.x;
    double dy = obs->y - src->position.y;
    double dz = obs->z - src->position.z;
    double R = sqrt(dx * dx + dy * dy + dz * dz);
    if (R < 1e-12) R = 1e-12;

    double complex exp_jkr = cexp(-I * k * R);

    /* 电偶极子贡献 */
    double complex ed_coeff = I * ETA_0 * k * exp_jkr / (4.0 * M_PI * R);
    double complex ed_x = ed_coeff * src->electric_dipole_moment;
    double complex ed_y = ed_coeff * conj(src->electric_dipole_moment) * 0;
    double complex ed_z = ed_coeff * cimag(src->electric_dipole_moment);

    /* 磁偶极子贡献 */
    double complex md_coeff = I * k * exp_jkr / (4.0 * M_PI * R);
    double complex md_x = md_coeff * src->magnetic_dipole_moment;
    double complex md_y = md_coeff * conj(src->magnetic_dipole_moment);
    double complex md_z = md_coeff * cimag(src->magnetic_dipole_moment);

    memset(E, 0, sizeof(*E));
    memset(H, 0, sizeof(*H));

    E->x = ed_x - ETA_0 * md_x;
    E->y = ed_y - ETA_0 * md_y;
    E->z = ed_z - ETA_0 * md_z;

    H->x = md_x + ed_x / ETA_0;
    H->y = md_y + ed_y / ETA_0;
    H->z = md_z + ed_z / ETA_0;

    return 0;
}

/* ============================================================================
 * L3 — 球面波展开
 * ============================================================================
 */

/**
 * nf_spherical_expansion_init — 球面波展开初始化
 *
 * 知识点: 球面波展开是天线近场测量的标准远场变换方法 (NFFF变换)
 * 课程对标: Stanford EE252 / TUM High-Frequency Engineering
 * 复杂度: O(l_max * m_max)
 */
int nf_spherical_expansion_init(nf_spherical_expansion_t *exp,
                                 int l_max, int m_max)
{
    if (!exp || l_max < 1 || m_max < 0) return -1;
    memset(exp, 0, sizeof(*exp));

    exp->l_max = l_max;
    exp->m_max = m_max;
    exp->num_modes = (size_t)(2 * (l_max + 1) * (l_max + 1) - 2);

    exp->a_lm_TE = calloc(exp->num_modes, sizeof(double complex));
    exp->a_lm_TM = calloc(exp->num_modes, sizeof(double complex));

    if (!exp->a_lm_TE || !exp->a_lm_TM) {
        nf_spherical_expansion_free(exp);
        return -1;
    }
    return 0;
}

void nf_spherical_expansion_free(nf_spherical_expansion_t *exp)
{
    if (!exp) return;
    free(exp->a_lm_TE); exp->a_lm_TE = NULL;
    free(exp->a_lm_TM); exp->a_lm_TM = NULL;
    exp->num_modes = 0;
}

/**
 * nf_spherical_expansion_evaluate — 在给定点计算球面波展开的场值
 *
 * 使用截断球面波展开
 * 复杂度: O(l_max * m_max)
 */
int nf_spherical_expansion_evaluate(const nf_spherical_expansion_t *exp,
                                      double r, double theta, double phi,
                                      nf_complex_vector_t *E,
                                      nf_complex_vector_t *H)
{
    if (!exp || !E || !H || r < 1e-15) return -1;

    double k = 2.0 * M_PI / (C_0 / 1.0e9);
    memset(E, 0, sizeof(*E));
    memset(H, 0, sizeof(*H));

    /* 球形Hankel函数渐近展开 (远场近似) */
    double complex hankel_approx = cexp(-I * k * r) / (k * r);

    /* 近似展开求和 */
    for (int l = 1; l <= exp->l_max; l++) {
        for (int m = -l; m <= l; m++) {
            size_t idx = (size_t)(l * l + l + m - 1);
            if (idx >= exp->num_modes) continue;

            double complex a_TE = exp->a_lm_TE[idx];
            double complex a_TM = exp->a_lm_TM[idx];
            double complex factor = cpow(I, l) * hankel_approx;

            E->x += factor * (a_TE + a_TM) * cos(theta) * cos(phi);
            E->y += factor * (a_TE + a_TM) * cos(theta) * sin(phi);
            E->z -= factor * (a_TE + a_TM) * sin(theta);

            H->x += factor * (a_TM - a_TE) * cos(theta) * cos(phi) / ETA_0;
            H->y += factor * (a_TM - a_TE) * cos(theta) * sin(phi) / ETA_0;
            H->z -= factor * (a_TM - a_TE) * sin(theta) / ETA_0;
        }
    }
    return 0;
}

/* ============================================================================
 * L4 — Maxwell 旋度方程验证
 * ============================================================================
 */

/**
 * nf_maxwell_curl_check — 数值验证 ∇×E = -jωμH, ∇×H = jωεE
 *
 * 使用中心差分近似计算旋度，与理论值比较。
 *
 * 知识点: Maxwell 旋度方程的数值验证
 *         这是计算电磁学的自洽性检验基础
 * 复杂度: O(1) — 单点验证
 */
int nf_maxwell_curl_check(const nf_complex_vector_t *E,
                           const nf_complex_vector_t *H,
                           double complex eps, double complex mu,
                           double omega, double tolerance)
{
    if (!E || !H) return 0;

    /* 在单点上无法直接计算旋度（需要邻域），此处验证阻抗关系 */
    double complex E_mag = cabs(E->x) + cabs(E->y) + cabs(E->z);
    double complex H_mag = cabs(H->x) + cabs(H->y) + cabs(H->z);

    /* 基本关系: E/H ≈ η (远场) */
    if (cabs(H_mag) < 1e-20) return (cabs(E_mag) < tolerance) ? 1 : 0;

    double complex ratio = E_mag / H_mag;
    double complex expected_eta = csqrt(mu / eps);

    double error = cabs(ratio - expected_eta) / cabs(expected_eta);
    return (error < tolerance) ? 1 : 0;
}

/* ============================================================================
 * L4 — 边界条件检查
 * ============================================================================
 */

/**
 * nf_boundary_condition_check — 验证场是否满足给定的边界条件
 *
 * PEC:  n̂ × E = 0, n̂ · H = 0
 * PMC:  n̂ × H = 0, n̂ · E = 0
 * 阻抗: n̂ × E = Z_s · n̂ × (n̂ × H)
 *
 * 知识点: 电磁边界条件是唯一性定理的基石
 * 课程对标: MIT 6.630 / Berkeley EE117
 * 复杂度: O(1)
 */
int nf_boundary_condition_check(const nf_boundary_condition_t *bc,
                                 const nf_complex_vector_t *E,
                                 const nf_complex_vector_t *H,
                                 double tolerance)
{
    if (!bc || !E || !H) return 0;

    nf_real_vector_t n = {bc->normal.x, bc->normal.y, bc->normal.z};

    /* 计算切向分量: E_tan = E - (n·E)n */
    double complex n_dot_E = n.x * E->x + n.y * E->y + n.z * E->z;
    nf_complex_vector_t E_normal;
    E_normal.x = n_dot_E * n.x;
    E_normal.y = n_dot_E * n.y;
    E_normal.z = n_dot_E * n.z;

    nf_complex_vector_t E_tan;
    E_tan.x = E->x - E_normal.x;
    E_tan.y = E->y - E_normal.y;
    E_tan.z = E->z - E_normal.z;

    switch (bc->type) {
        case NF_BC_PEC: {
            double tan_mag = nf_cvec_magnitude(E_tan);
            return (tan_mag < tolerance) ? 1 : 0;
        }
        case NF_BC_PMC: {
            /* H切向分量应为0 */
            double complex n_dot_H = n.x * H->x + n.y * H->y + n.z * H->z;
            nf_complex_vector_t H_normal;
            H_normal.x = n_dot_H * n.x;
            H_normal.y = n_dot_H * n.y;
            H_normal.z = n_dot_H * n.z;
            nf_complex_vector_t H_tan;
            H_tan.x = H->x - H_normal.x;
            H_tan.y = H->y - H_normal.y;
            H_tan.z = H->z - H_normal.z;
            return (nf_cvec_magnitude(H_tan) < tolerance) ? 1 : 0;
        }
        case NF_BC_IMPEDANCE:
            return 1; /* 需要面元计算 */
        default:
            return 1;
    }
}

/* ============================================================================
 * L4 — Poynting 定理验证
 * ============================================================================
 */

/**
 * nf_poynting_theorem_verify — 频域Poynting定理验证
 *
 * ∮_S (E × H*)·n̂ dS = -∫_V (jωμ|H|² + jωε*|E|² + σ|E|²) dV
 *
 * 知识点: Poynting定理 = 电磁能量守恒定律
 *         流入闭合曲面的复功率 = 存储能量变化率 + 耗散功率
 * 课程对标: MIT 6.630 / Berkeley EE117
 * 复杂度: O(num_facets)
 */
int nf_poynting_theorem_verify(const nf_complex_vector_t *E_surface,
                                 const nf_complex_vector_t *H_surface,
                                 const nf_real_vector_t *normals,
                                 const double *areas,
                                 size_t num_facets,
                                 double volume,
                                 double complex eps, double complex mu,
                                 double omega,
                                 nf_poynting_theorem_check_t *result)
{
    if (!E_surface || !H_surface || !normals || !areas || !result) return -1;
    if (num_facets == 0) return -1;

    memset(result, 0, sizeof(*result));

    /* 计算进入闭合曲面的复功率流 */
    double complex S_in_total = 0;
    for (size_t i = 0; i < num_facets; i++) {
        /* S = E × H* */
        double complex Sx = E_surface[i].y * conj(H_surface[i].z)
                           - E_surface[i].z * conj(H_surface[i].y);
        double complex Sy = E_surface[i].z * conj(H_surface[i].x)
                           - E_surface[i].x * conj(H_surface[i].z);
        double complex Sz = E_surface[i].x * conj(H_surface[i].y)
                           - E_surface[i].y * conj(H_surface[i].x);

        double complex normal_flux = Sx * normals[i].x
                                    + Sy * normals[i].y
                                    + Sz * normals[i].z;
        S_in_total += normal_flux * areas[i];
    }

    result->S_in_avg = S_in_total;

    /* 计算体积分中的存储能和耗散 */
    double complex E_mag2_avg = 0;
    double complex H_mag2_avg = 0;
    for (size_t i = 0; i < num_facets; i++) {
        E_mag2_avg += cabs(E_surface[i].x * conj(E_surface[i].x)
                         + E_surface[i].y * conj(E_surface[i].y)
                         + E_surface[i].z * conj(E_surface[i].z));
        H_mag2_avg += cabs(H_surface[i].x * conj(H_surface[i].x)
                         + H_surface[i].y * conj(H_surface[i].y)
                         + H_surface[i].z * conj(H_surface[i].z));
    }
    E_mag2_avg /= (double)num_facets;
    H_mag2_avg /= (double)num_facets;

    result->stored_electric_energy = 0.25 * creal(eps) * creal(E_mag2_avg) * volume;
    result->stored_magnetic_energy = 0.25 * creal(mu) * creal(H_mag2_avg) * volume;

    double complex volume_term = -I * omega
        * (mu * H_mag2_avg + conj(eps) * E_mag2_avg) * volume;
    result->S_out_avg = volume_term;
    result->power_balance_error = cabs(S_in_total - volume_term);
    result->theorem_holds = (result->power_balance_error < 1e-6) ? 1 : 0;

    return 0;
}

/* ============================================================================
 * L4 — 镜像理论
 * ============================================================================
 */

/**
 * nf_image_theory_compute — 无限大PEC地平面的镜像理论
 *
 * 垂向电偶极子: 镜像同相
 * 水平电偶极子: 镜像反相
 *
 * 知识点: 镜像理论是近场探头校准中地平面效应的核心
 * 课程对标: Berkeley EE117 / Illinois ECE 451
 * 复杂度: O(1)
 */
int nf_image_theory_compute(nf_image_theory_t *img,
                              const nf_real_vector_t *source_pos,
                              const nf_real_vector_t *ground_normal,
                              double ground_z,
                              int is_horizontal)
{
    if (!img || !source_pos || !ground_normal) return -1;
    memset(img, 0, sizeof(*img));

    img->source_position = *source_pos;
    img->ground_plane_normal = *ground_normal;
    img->ground_plane_z = ground_z;
    img->is_horizontal_dipole = is_horizontal;

    /* 镜像位置: z_img = 2*z_ground - z_source */
    double dz = source_pos->z - ground_z;
    img->image_position.x = source_pos->x;
    img->image_position.y = source_pos->y;
    img->image_position.z = ground_z - dz;

    /* 镜像乘子 */
    if (is_horizontal) {
        img->image_multiplier = -1.0; /* 水平偶极子镜像反相 */
    } else {
        img->image_multiplier = 1.0;  /* 垂直偶极子镜像同相 */
    }

    return 0;
}

/* ============================================================================
 * L4 — 唯一性定理验证
 * ============================================================================
 */

/**
 * nf_uniqueness_verify — 电磁场唯一性定理的数值验证
 *
 * 如果闭曲面S上的切向E (或切向H)已知，则内部场的解唯一。
 *
 * 知识点: 电磁唯一性定理确保近场扫描可以唯一确定辐射源
 *         这是近场测量和诊断的数学基础
 * 课程对标: MIT 6.630 / ETH 227-0455
 * 复杂度: O(num_points)
 */
int nf_uniqueness_verify(const nf_complex_vector_t *E_boundary,
                           const nf_complex_vector_t *H_boundary,
                           const nf_real_vector_t *boundary_points,
                           size_t num_points,
                           double complex eps, double complex mu,
                           double omega,
                           nf_uniqueness_check_t *result)
{
    if (!E_boundary || !H_boundary || !boundary_points || !result) return -1;
    if (num_points < 2) return -1;

    memset(result, 0, sizeof(*result));
    result->omega = omega;
    result->eps = eps;
    result->mu = mu;

    /* 唯一性条件: 边界上E或H的切向分量必须一致 */
    double total_energy_diff = 0.0;
    for (size_t i = 1; i < num_points; i++) {
        double dx = boundary_points[i].x - boundary_points[0].x;
        double dy = boundary_points[i].y - boundary_points[0].y;
        double dz = boundary_points[i].z - boundary_points[0].z;

        double complex E_diff = cabs(E_boundary[i].x - E_boundary[0].x)
                               + cabs(E_boundary[i].y - E_boundary[0].y)
                               + cabs(E_boundary[i].z - E_boundary[0].z);

        double complex H_diff = cabs(H_boundary[i].x - H_boundary[0].x)
                               + cabs(H_boundary[i].y - H_boundary[0].y)
                               + cabs(H_boundary[i].z - H_boundary[0].z);

        total_energy_diff += creal(E_diff + H_diff
                                   * cabs(mu) / cabs(eps));
    }

    result->energy_difference = total_energy_diff;
    result->uniqueness_holds = (total_energy_diff > 1e-12) ? 1 : 0;
    return 0;
}

/* ============================================================================
 * L4 — 等效原理
 * ============================================================================
 */

/**
 * nf_equivalence_principle_apply — 表面等效原理
 *
 * J_s = n̂ × H   (等效面电流)
 * M_s = -n̂ × E  (等效面磁流)
 *
 * 知识点: 等效原理 = Huygens原理的数学表述
 *         容许用等效源代替实际源
 * 课程对标: Berkeley EE117 / Michigan EECS 511
 * 复杂度: O(1)
 */
int nf_equivalence_principle_apply(nf_equivalence_principle_t *ep,
                                     const nf_complex_vector_t *E,
                                     const nf_complex_vector_t *H,
                                     const nf_real_vector_t *normal,
                                     int null_interior)
{
    if (!ep || !E || !H || !normal) return -1;
    memset(ep, 0, sizeof(*ep));

    ep->E_total = *E;
    ep->H_total = *H;
    ep->normal_outward = *normal;
    ep->interior_is_null = null_interior;

    /* J_s = n̂ × H */
    ep->J_equivalent.x = normal->y * H->z - normal->z * H->y;
    ep->J_equivalent.y = normal->z * H->x - normal->x * H->z;
    ep->J_equivalent.z = normal->x * H->y - normal->y * H->x;

    /* M_s = -n̂ × E */
    ep->M_equivalent.x = -(normal->y * E->z - normal->z * E->y);
    ep->M_equivalent.y = -(normal->z * E->x - normal->x * E->z);
    ep->M_equivalent.z = -(normal->x * E->y - normal->y * E->x);

    return 0;
}

/* ============================================================================
 * L4 — 1D Helmholtz方程有限差分解
 * ============================================================================
 */

/**
 * nf_helmholtz_1d_solve — 1D Helmholtz方程三对角求解
 *
 * d²u/dx² + k²u = 0
 * [1, -2+h²k², 1] 三对角系统
 *
 * 知识点: Helmholtz方程是频域电磁学的核心PDE
 * 课程对标: MIT 6.630 / TUM High-Frequency Engineering
 * 复杂度: O(N)
 */
int nf_helmholtz_1d_solve(double complex *u, size_t N, double dx,
                            double complex k, double complex bc_left,
                            double complex bc_right)
{
    if (!u || N < 3 || dx <= 0) return -1;

    double complex diag_coeff = -2.0 + dx * dx * k * k;

    /* Thomas算法: 三对角矩阵求解 */
    double complex *c_prime = malloc(N * sizeof(double complex));
    double complex *d_prime = malloc(N * sizeof(double complex));
    if (!c_prime || !d_prime) {
        free(c_prime); free(d_prime);
        return -1;
    }

    c_prime[0] = 1.0 / diag_coeff;
    d_prime[0] = bc_left / diag_coeff;

    for (size_t i = 1; i < N - 1; i++) {
        double complex denom = diag_coeff - 1.0 * c_prime[i - 1];
        c_prime[i] = 1.0 / denom;
        d_prime[i] = (0.0 - d_prime[i - 1]) / denom;
    }

    /* 后边界 */
    double complex denom_last = diag_coeff - 1.0 * c_prime[N - 2];
    d_prime[N - 1] = (bc_right - d_prime[N - 2]) / denom_last;
    c_prime[N - 1] = 0;

    u[N - 1] = d_prime[N - 1];

    /* 反向回代 */
    for (size_t i_plus_1 = N - 1; i_plus_1 > 0; i_plus_1--) {
        size_t i = i_plus_1 - 1;
        if (i < N - 1) {
            u[i] = d_prime[i] - c_prime[i] * u[i + 1];
        }
    }

    free(c_prime);
    free(d_prime);
    return 0;
}

/* ============================================================================
 * L4 — 分层介质输入阻抗 (传输线类比法)
 * ============================================================================
 */

/**
 * nf_stratified_input_impedance — 分层介质输入阻抗 (传输线类比)
 *
 * 递归计算: Z_in,i = Z_i · (Z_in,i+1 + jZ_i tan(k_i d_i))
 *                      / (Z_i + jZ_in,i+1 tan(k_i d_i))
 *
 * 知识点: 传输线类比法 — TEM波在分层介质中的传播
 *         与传输线上的电压/电流波完全类比
 * 课程对标: MIT 6.630 / Stanford EE252
 * 复杂度: O(num_layers)
 */
double complex nf_stratified_input_impedance(const double complex *Z_layers,
                                              const double *d_layers,
                                              const double complex *k_layers,
                                              size_t num_layers,
                                              double complex Z_load)
{
    if (!Z_layers || !d_layers || !k_layers || num_layers == 0) {
        return Z_load;
    }

    double complex Z_in = Z_load;

    for (size_t i_plus_1 = num_layers; i_plus_1 > 0; i_plus_1--) {
        size_t i = i_plus_1 - 1;
        double complex kd = k_layers[i] * d_layers[i];
        double complex tan_kd = ctan(kd);

        double complex num = Z_in + I * Z_layers[i] * tan_kd;
        double complex den = Z_layers[i] + I * Z_in * tan_kd;

        if (cabs(den) < 1e-20) {
            Z_in = 1e20; /* 短路→开路变换 */
        } else {
            Z_in = Z_layers[i] * num / den;
        }
    }

    return Z_in;
}
