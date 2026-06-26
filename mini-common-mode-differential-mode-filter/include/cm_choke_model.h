/**
 * @file cm_choke_model.h
 * @brief Common-Mode Choke Physical Model & Core Magnetics
 *
 * Detailed physical modeling of common-mode chokes including:
 *  - Core material properties (ferrite, nanocrystalline, amorphous)
 *  - Core geometry calculations (toroid, E-core, U-core)
 *  - Winding parasitics (leakage inductance, capacitance, skin/proximity effect)
 *  - Saturation prediction and core loss (Steinmetz, improved generalized)
 *  - Frequency-dependent impedance with all parasitics
 *  - Temperature effects on permeability and saturation
 *
 * Reference:
 *  - McLyman, C.W.T. "Transformer and Inductor Design Handbook" (2011)
 *  - Snelling, E.C. "Soft Ferrites: Properties and Applications" (1988)
 *  - Paul, C.R. "Introduction to EMC" (2006) — Chapters 6, 10
 */

#ifndef CM_CHOKE_MODEL_H
#define CM_CHOKE_MODEL_H

#include "cm_dm_filter.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * L1: Core material database entries
 * ======================================================================== */

/** @brief Pre-defined ferrite materials (common types used in EMC filters) */
typedef enum {
    CORE_MAT_3C90 = 0,      /**< Ferroxcube 3C90 — general purpose MnZn */
    CORE_MAT_3F3,           /**< Ferroxcube 3F3 — high freq MnZn */
    CORE_MAT_3F4,           /**< Ferroxcube 3F4 — >1 MHz MnZn */
    CORE_MAT_N87,           /**< TDK/Epcos N87 — power MnZn */
    CORE_MAT_N97,           /**< TDK/Epcos N97 — low loss MnZn */
    CORE_MAT_N49,           /**< TDK/Epcos N49 — NiZn for CM chokes >10 MHz */
    CORE_MAT_T38,           /**< TDK/Epcos T38 — telecom MnZn */
    CORE_MAT_79,            /**< Magnetics 79 — NiZn */
    CORE_MAT_F,             /**< Magnetics F — power ferrite */
    CORE_MAT_MPP,           /**< Molypermalloy powder */
    CORE_MAT_HF,            /**< High-flux powder */
    CORE_MAT_KOOL_MU,       /**< Kool Mμ sendust */
    CORE_MAT_NANOCRYST,     /**< Nanocrystalline (Vitroperm, Finemet) */
    CORE_MAT_AMORPHOUS,     /**< Amorphous metal (Metglas) */
    CORE_MAT_FERRITE_CUSTOM /**< User-defined ferrite */
} core_material_id_t;

/** @brief Core shape family */
typedef enum {
    CORE_SHAPE_TOROID = 0,          /**< Toroidal core */
    CORE_SHAPE_E_CORE,              /**< E-core (E-E or E-I) */
    CORE_SHAPE_U_CORE,              /**< U-core (U-U) */
    CORE_SHAPE_PQ,                  /**< PQ core */
    CORE_SHAPE_ETD,                 /**< ETD core */
    CORE_SHAPE_RM,                  /**< RM core */
    CORE_SHAPE_PLANAR_E,            /**< Planar E-core (PCB integration) */
    CORE_SHAPE_PLANAR_ER            /**< Planar ER core */
} core_shape_t;

/**
 * @brief Complete core description including shape-specific calculations.
 *
 * All cores are parameterized by their effective parameters A_e, l_e, V_e
 * which allow any shape to be analyzed using the same magnetic circuit model.
 */
typedef struct {
    core_material_id_t mat_id;      /**< Material identifier */
    core_shape_t shape;             /**< Core shape */
    core_material_t material;       /**< Material properties */
    core_geometry_t geometry;       /**< Geometric effective parameters */
    double window_area;             /**< Winding window area [m²] */
    double window_width;            /**< Winding window width [m] */
    double mean_turn_length;        /**< Mean length per turn [m] */
    double surface_area;            /**< Core surface area (cooling) [m²] */
    double mass;                    /**< Core mass [kg] */
} core_description_t;

/* ========================================================================
 * L1: Winding configuration
 * ======================================================================== */

/** @brief Winding arrangement on the core */
typedef enum {
    WINDING_BIFILAR = 0,            /**< Bifilar (twisted) — best coupling, worst CM isolation */
    WINDING_SECTIONAL = 1,          /**< Sectional (separate bobbins) — best isolation */
    WINDING_LAYER = 2,              /**< Layered — moderate coupling */
    WINDING_SINGLE_LAYER = 3,       /**< Single layer — lowest capacitance */
    WINDING_MULTI_LAYER = 4         /**< Multi-layer — highest inductance per volume */
} winding_arrangement_t;

/** @brief Detailed winding parameters */
typedef struct {
    double num_turns;               /**< Number of turns */
    winding_arrangement_t arrangement; /**< Winding arrangement */
    double wire_diameter;           /**< Wire diameter [m] (bare) */
    double wire_diameter_insulated; /**< Wire diameter with insulation [m] */
    double wire_resistivity;        /**< Wire resistivity [Ω·m] (Cu = 1.68e-8) */
    double num_layers;              /**< Number of layers */
    double turns_per_layer;         /**< Turns per layer */
    double interlayer_insul_thick;  /**< Insulation thickness between layers [m] */
    double bobbin_width;            /**< Bobbin winding width [m] */
    double creepage_distance;       /**< Creepage distance between windings [m] */
} winding_params_t;

/**
 * @brief Complete CM choke model with windings and parasitics calculated.
 */
typedef struct {
    core_description_t core;        /**< Magnetic core description */
    winding_params_t winding;       /**< Winding parameters */
    cm_choke_model_t base;          /**< Basic CM choke model (integrated) */
    /* Calculated parasitics */
    double dc_resistance;           /**< DC winding resistance [Ω] */
    double ac_resistance_100kHz;    /**< AC resistance at 100 kHz [Ω] (skin+proximity) */
    double leakage_inductance;      /**< Leakage inductance [H] */
    double intra_winding_cap;       /**< Intra-winding capacitance [F] */
    double inter_winding_cap;       /**< Inter-winding capacitance [F] */
    double total_parallel_cap;      /**< Total parallel capacitance [F] */
    double self_resonant_freq_cm;   /**< CM self-resonant frequency [Hz] */
    double self_resonant_freq_dm;   /**< DM self-resonant frequency [Hz] */
    double temp_rise_estimate;      /**< Estimated temperature rise [K] */
    double window_fill_factor;      /**< Window utilization factor (0-1) */
} cm_choke_full_t;

/* ========================================================================
 * L3: Core loss models
 * ======================================================================== */

/** @brief Core loss model selection */
typedef enum {
    LOSS_MODEL_STEINMETZ = 0,       /**< Original Steinmetz: P = k*f^α*B^β */
    LOSS_MODEL_MODIFIED_STEINMETZ,  /**< MSE: uses equivalent frequency */
    LOSS_MODEL_GENERALIZED_STEINMETZ, /**< GSE: dB/dt based */
    LOSS_MODEL_IMPROVED_GSE,        /**< iGSE: instantaneous dB/dt */
    LOSS_MODEL_JILES_ATHERTON,      /**< Jiles-Atherton hysteresis model */
    LOSS_MODEL_COMPOSITE            /**< Composite: hysteresis + eddy + excess */
} loss_model_t;

/**
 * @brief Core loss calculation result.
 *
 * Core loss has three components (Bertotti model):
 *   P_total = P_hysteresis + P_eddy + P_excess
 *   P_hyst ∝ f * B^β
 *   P_eddy ∝ f² * B²
 *   P_excess ∝ f^(1.5) * B^(1.5)
 */
typedef struct {
    double total_loss_w;            /**< Total core loss [W] */
    double hysteresis_loss_w;       /**< Hysteresis loss component [W] */
    double eddy_current_loss_w;     /**< Eddy current loss component [W] */
    double excess_loss_w;           /**< Excess (anomalous) loss component [W] */
    double loss_density_w_m3;       /**< Loss density [W/m³] */
    double temperature_rise_k;      /**< Estimated core temperature rise [K] */
} core_loss_result_t;

/* ========================================================================
 * L2: Frequency-dependent permeability
 * ======================================================================== */

/**
 * @brief Complex permeability of a ferrite material.
 *
 * Ferrite permeability is frequency-dependent and complex:
 *   μ(f) = μ'(f) - j·μ''(f)
 *
 * μ'(f) is the inductive component (energy storage).
 * μ''(f) is the resistive component (energy loss).
 *
 * The frequency response follows the Snoek limit:
 *   (μ_i - 1) * f_r ≤ (γ/2π) * M_s / 3  [Snoek's limit]
 *
 * Typically modeled as a first-order Debye relaxation or
 * Cole-Cole empirical fit.
 */
typedef struct {
    double mu_real;                 /**< μ'(f) — real part (inductive) */
    double mu_imag;                 /**< μ''(f) — imaginary part (lossy) */
    double tan_delta;               /**< Loss tangent = μ''/μ' */
    double quality_factor;          /**< Q = μ'/μ'' */
    double frequency;               /**< Frequency [Hz] */
} complex_permeability_t;

/* ========================================================================
 * L4: Hysteresis loop model
 * ======================================================================== */

typedef struct {
    double h_coercive;              /**< Coercive field H_c [A/m] */
    double b_retentivity;           /**< Retentivity B_r [T] */
    double b_saturation;            /**< Saturation flux density B_sat [T] */
    double h_saturation;            /**< Field at saturation [A/m] */
    double initial_mu;              /**< Initial permeability */
    double max_mu;                  /**< Maximum differential permeability */
    double remanence_ratio;         /**< B_r/B_sat */
} hysteresis_params_t;

/* ========================================================================
 * L6: DC bias effects on inductance
 * ======================================================================== */

/**
 * @brief Inductance derating under DC bias.
 *
 * L6: When DM load current flows through a CM choke, it creates a DC
 * magnetic bias in the core, reducing permeability and thus CM inductance.
 *
 * For ferrite: L(IDC) / L(0) ≈ 1 / (1 + (IDC/I_sat)²)
 * This is a critical practical limit on CM choke performance.
 */
typedef struct {
    double dc_current;              /**< Applied DC bias current [A] */
    double inductance_ratio;        /**< L(bias) / L(0) */
    double percent_inductance;      /**< Remaining inductance [%] */
    double b_dc;                    /**< DC flux density [T] */
    double b_ac_pk;                 /**< Peak AC flux density [T] */
    int is_saturated;               /**< Nonzero if core is saturated */
} dc_bias_result_t;

/* ========================================================================
 * L7: Temperature effects
 * ======================================================================== */

/**
 * @brief Temperature-dependent parameter shifts.
 *
 * Ferrite permeability peaks near the Curie temperature (typically
 * 200-250°C for MnZn) and drops sharply above T_c.
 *
 * At T = T_curie, μ_r → 1 (ferromagnetic → paramagnetic transition).
 * In practice, safe operation requires T < T_curie - 50°C.
 */
typedef struct {
    double temperature_c;           /**< Operating temperature [°C] */
    double mu_ratio_to_25c;         /**< μ(T) / μ(25°C) */
    double b_sat_ratio_to_25c;      /**< B_sat(T) / B_sat(25°C) */
    double resistivity_ratio;       /**< ρ(T) / ρ(25°C) */
    double loss_ratio_to_25c;       /**< P_loss(T) / P_loss(25°C) */
    double recommended_derating;    /**< Recommended flux derating factor */
} temp_derating_t;

/* ========================================================================
 * API FUNCTIONS
 * ======================================================================== */

/**
 * @brief Look up core material parameters from the built-in database.
 *
 * Contains data for 14 standard ferrite/powder/nanocrystalline materials.
 * Returns NULL if material ID is out of range.
 */
core_material_t *core_material_lookup(core_material_id_t mat_id);

/**
 * @brief Calculate effective core geometry parameters from dimensions.
 *
 * For toroid:
 *   A_e = (OD-ID)/2 * height
 *   l_e = π * (OD+ID)/2
 *   V_e = A_e * l_e
 *   AL = μ₀ * μ_r * A_e / l_e * 1e9  [nH/turn²]
 *
 * For E-core/U-core, effective parameters are from manufacturer datasheets.
 */
core_geometry_t core_geom_calculate(core_shape_t shape,
                                     double dim1, double dim2, double dim3,
                                     double mu_r);

/**
 * @brief Calculate winding parameters based on geometry and arrangement.
 *
 * Computes DC resistance, AC resistance at reference frequency,
 * window fill factor, and winding parasitics.
 *
 * @param core     Core description
 * @param params   Winding specification [input/updated]
 * @return 0 on success, -1 if winding doesn't fit
 */
int winding_parameters_calculate(const core_description_t *core,
                                  winding_params_t *params);

/**
 * @brief Calculate all CM choke parasitics and build full model.
 *
 * L2-L3: Complete CM choke model including:
 *  - CM inductance from AL value and turns
 *  - DM leakage inductance (analytical or FEM-approximated)
 *  - Winding capacitance (intra- and inter-winding)
 *  - Frequency-dependent AC resistance (skin + proximity effect)
 *  - Self-resonant frequencies for CM and DM modes
 *
 * @return Fully populated CM choke model [dynamically allocated]
 */
cm_choke_full_t *cm_choke_full_model(const core_description_t *core,
                                      const winding_params_t *winding,
                                      double coupling_coef);

/**
 * @brief Compute complex permeability at a given frequency.
 *
 * L2: Uses a Cole-Cole relaxation model:
 *   μ(ω) = μ_∞ + (μ_i - μ_∞) / (1 + (jωτ)^(1-α))
 *
 * where τ = 1/(2πf_r), f_r is the relaxation frequency,
 * α is the distribution parameter (0 = Debye, >0 = broader).
 */
complex_permeability_t complex_permeability_calc(
    double mu_i, double mu_inf, double f_relax,
    double alpha, double freq);

/**
 * @brief Calculate core loss using selected model.
 *
 * L3: Implements Steinmetz, MSE, GSE, and iGSE for arbitrary waveforms.
 *
 * @param model       Loss model selection
 * @param material    Core material
 * @param volume      Core volume [m³]
 * @param b_peak      Peak flux density [T]
 * @param freq        Fundamental frequency [Hz]
 * @param duty_cycle  Waveform duty cycle (0-1 for rectangular)
 * @return Core loss result
 */
core_loss_result_t core_loss_calculate(loss_model_t model,
                                        const core_material_t *material,
                                        double volume,
                                        double b_peak, double freq,
                                        double duty_cycle);

/**
 * @brief Compute hysteresis loop parameters.
 *
 * L4: From catalog data (B_sat, B_r, H_c), reconstruct the
 * major hysteresis loop parameters.
 */
hysteresis_params_t hysteresis_from_catalog(double b_sat, double b_rem,
                                             double h_coercive,
                                             double mu_i);

/**
 * @brief Model the hysteresis loop path for a given B-H trajectory.
 *
 * Returns B for a given H using a simplified Preisach or
 * Jiles-Atherton model. The function tracks the current
 * magnetization state.
 */
double hysteresis_b_from_h(const hysteresis_params_t *hp,
                            double h, double b_prev);

/**
 * @brief Estimate inductance drop under DC bias.
 *
 * L6 Critical Problem: DC load current in CM choke reduces CM inductance.
 * This uses the manufacturer's DC bias curves fitted to a rational function.
 *
 * @param L0      Zero-bias inductance [H]
 * @param i_bias  DC bias current [A]
 * @param i_sat   Saturation current (L drops 30%) [A]
 * @return Inductance under bias [H]
 */
double inductance_under_dc_bias(double L0, double i_bias, double i_sat);

/**
 * @brief Compute temperature derating factors.
 *
 * L7: Temperature effects on core material parameters.
 * Uses manufacturer's thermal data fitted to empirical curves.
 */
temp_derating_t temperature_derating(const core_material_t *mat,
                                      double temp_c);

/**
 * @brief Compute skin depth in copper at given frequency.
 *
 * δ = √(2ρ / (ω·μ₀))
 * At 100 kHz: δ ≈ 0.21 mm
 * At 1 MHz:   δ ≈ 0.066 mm
 *
 * Wires thicker than ~2δ waste copper (current only flows in skin).
 */
double skin_depth_copper(double freq);

/**
 * @brief Estimate AC-to-DC resistance ratio due to skin and proximity effects.
 *
 * Uses Dowell's equation for layered windings.
 * R_ac/R_dc = f(φ, layers) where φ = d_wire/δ
 */
double ac_dc_resistance_ratio(double wire_diameter, double freq,
                               int num_layers, double porosity_factor);

/**
 * @brief Free a CM choke full model.
 */
void cm_choke_full_free(cm_choke_full_t *choke);

/**
 * @brief Free core material (if dynamically allocated from lookup).
 */
void core_material_free(core_material_t *mat);

#ifdef __cplusplus
}
#endif

#endif /* CM_CHOKE_MODEL_H */
