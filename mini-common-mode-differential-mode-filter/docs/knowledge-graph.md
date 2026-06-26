# Knowledge Graph — CM/DM EMI Filter

## L1: Definitions (COMPLETE ✅)

| # | Entry | C Definition | Lean Definition |
|---|-------|-------------|-----------------|
| 1 | Common-Mode Voltage | `cm_quantities_t.voltage_cm` | `CM_DM_Int.vCM` |
| 2 | Differential-Mode Voltage | `dm_quantities_t.voltage_dm` | `CM_DM_Int.vDM` |
| 3 | CM Current | `cm_quantities_t.current_cm` | — |
| 4 | DM Current | `dm_quantities_t.current_dm` | — |
| 5 | CM/DM Decomposition | `cm_dm_decomposition_t` | `VoltagePairInt` |
| 6 | Common-Mode Rejection Ratio | `cmrr_result_t.cmrr_db` | `cmrrQuality` |
| 7 | Insertion Loss | `insertion_loss_result_t.il_db` | — |
| 8 | Cutoff Frequency | `filter_stage_t.cutoff_freq_design` | `cutoffQuality` |
| 9 | Filter Element Type | `filter_elem_type_t` enum | — |
| 10 | X-Capacitor | `X_CLASS_X1/X2/X3` | — |
| 11 | Y-Capacitor | `Y_CLASS_Y1/Y2/Y3/Y4` | — |
| 12 | Core Material | `core_material_t` | `CoreParamsNat` |
| 13 | Core Geometry | `core_geometry_t` | — |
| 14 | Complex Impedance | `complex_t` | `ComplexImpedance` |
| 15 | S-Parameters | `s_matrix_t` | — |
| 16 | ABCD Parameters | `abcd_matrix_t` | — |
| 17 | Quality Factor | — | `dampingMetric` |
| 18 | Bleed Resistor | — | `bleedResistorMetric` |

## L2: Core Concepts (COMPLETE ✅)

| # | Concept | Implementation |
|---|---------|---------------|
| 1 | CM Choke Principle (flux add/cancel) | `cm_choke_create()` Faraday's law |
| 2 | Filter Topologies (L,C,LC,π,T) | `filter_topology_t` + `build_filter_abcd()` |
| 3 | Parasitic Elements (ESR, ESL, EPC) | `filter_element_set_parasitics()` |
| 4 | Y-Cap Safety Limits (leakage current) | `y_cap_leakage_current()` |
| 5 | X-Cap Bleed Resistor | `x_cap_bleed_resistor()` |
| 6 | CISPR 17 Standard Impedances | `cispr17_impedance_t` |
| 7 | Mixed-Mode S-Parameters | `s_4port_to_mixed_mode()` |
| 8 | Safety Spacing (IEC 62368-1) | `safety_spacing_calculate()` |
| 9 | Thermal Management | `thermal_analysis()` |
| 10 | Filter-Impedance Interaction | `dm_filter_impedances()` |

## L3: Mathematical Structures (COMPLETE ✅)

| # | Structure | Implementation |
|---|-----------|---------------|
| 1 | Complex Number Arithmetic | `complex_add/sub/mul/div/mag/phase` |
| 2 | s-Domain Transfer Functions | `filter_stage_transfer()` |
| 3 | Bode Plot Generation | `filter_stage_bode()` |
| 4 | Network Admittance Matrix | `build_admittance_matrix()` |
| 5 | Linear System Solver (Complex) | `solve_complex_linear_system()` |
| 6 | Z ↔ Y Matrix Conversion | `z_to_y_matrix()` |
| 7 | Z ↔ ABCD Conversion | `z_to_abcd_matrix()` |
| 8 | S ↔ Z Conversion | `s_to_z_matrix()`, `z_to_s_matrix()` |
| 9 | ABCD Cascade Multiplication | `abcd_cascade_two()`, `abcd_cascade_n()` |
| 10 | Mixed-Mode S-Parameter Transform | `s_4port_to_mixed_mode()` |
| 11 | Complex Permeability (Cole-Cole) | `complex_permeability_calc()` |
| 12 | Frequency Sweep | `il_frequency_sweep()` |

## L4: Fundamental Laws (COMPLETE ✅)

| # | Law | Implementation |
|---|-----|---------------|
| 1 | Faraday's Law (inductance from flux) | `cm_choke_create()` — L = N²·μ·A/l |
| 2 | Flux Addition (CM) / Cancellation (DM) | L_cm vs L_dm_leakage calculation |
| 3 | Snoek's Limit (μ·f_r product) | `complex_permeability_calc()` |
| 4 | Kirchhoff's Laws (nodal analysis) | `build_admittance_matrix()` |
| 5 | Maximum Power Transfer | `filter_select_lc()` — Z₀ matching |
| 6 | Gauss's Law (capacitance) | `filter_element_set_parasitics()` EPC model |
| 7 | Induction Law (skin effect) | `skin_depth_copper()`, `ac_dc_resistance_ratio()` |
| 8 | Steinmetz Law (core loss) | `cm_choke_core_loss()`, `core_loss_calculate()` |

## L5: Algorithms/Methods (COMPLETE ✅)

| # | Algorithm | Implementation |
|---|-----------|---------------|
| 1 | CM/DM Decomposition (1φ) | `cm_dm_decompose()` |
| 2 | CM/DM Decomposition (3φ Clarke) | `cm_dm_decompose_3phase()` |
| 3 | Filter Design (EMC Compliance) | `filter_design_emi()` |
| 4 | LC Component Selection | `filter_select_lc()` |
| 5 | Required Attenuation Calculation | `required_attenuation()` |
| 6 | Minimum Filter Order | `minimum_filter_order()` |
| 7 | DM Inductor Design (Area Product) | `dm_inductor_design()` |
| 8 | Filter Optimization | `filter_optimize()` |
| 9 | Standard Filter Selection | `standard_filter_select()` |
| 10 | CISPR 17 IL Evaluation | `il_cispr17_comprehensive()` |
| 11 | Safety Compliance Check | `safety_compliance_check()` |
| 12 | Reliability Prediction | `reliability_predict()` |

## L6: Canonical Problems (COMPLETE ✅)

| # | Problem | Implementation |
|---|---------|---------------|
| 1 | AC-DC SMPS Filter Design | `example_ac_dc_filter.c` |
| 2 | CM Choke Saturation Under DM Load | `cm_choke_check_saturation()` |
| 3 | DC Bias Inductance Loss | `inductance_under_dc_bias()` |
| 4 | Filter Resonance and Damping | `filter_optimal_damping_resistance()` |
| 5 | High-Frequency IL Degradation | `il_hf_degradation_estimate()` |
| 6 | Mode Conversion from Imbalance | `path_imbalance_from_tolerance()` |
| 7 | Cascaded Multi-Stage Analysis | `filter_cascade_transfer()` |
| 8 | IL Measurement Uncertainty | `il_measurement_uncertainty()` |
| 9 | System-Level IL (incl. PCB) | `il_system_level()` |

## L7: Applications (COMPLETE ✅ — 8/8)

| # | Application | Implementation |
|---|-------------|---------------|
| 1 | AC-DC SMPS (65W Laptop) | `filter_preset_get(APP_AC_DC_SMPS)` |
| 2 | Motor Drive (1HP VFD) | `filter_preset_get(APP_MOTOR_DRIVE)` |
| 3 | USB 3.0 Interface | `filter_preset_get(APP_USB_INTERFACE)` |
| 4 | Ethernet (1000Base-T) | `filter_preset_get(APP_ETHERNET_INTERFACE)` |
| 5 | Medical Device PSU | `filter_preset_get(APP_MEDICAL_DEVICE)` |
| 6 | Solar Inverter | `filter_preset_get(APP_INVERTER_SOLAR)` |
| 7 | Commercial Filter Selection | `standard_filter_select()` (Schaffner, TDK, KEMET) |
| 8 | CM Choke Thermal Derating | `temperature_derating()` |

## L8: Advanced Topics (PARTIAL+ ✅ — 4/6)

| # | Topic | Implementation |
|---|-------|---------------|
| 1 | Impedance Interaction (Middlebrook) | `impedance_interaction_analyze()` |
| 2 | Reliability Prediction (MIL-HDBK-217F) | `reliability_predict()` |
| 3 | Core Loss (Multi-Model: Steinmetz, MSE, GSE, iGSE, Composite) | `core_loss_calculate()` |
| 4 | Filter Optimization (Pareto) | `filter_optimize()` |
| 5 | Time-Varying EMI Analysis | *(documented only)* |
| 6 | Stochastic/MCMC Noise Modeling | *(documented only)* |

## L9: Research Frontiers (PARTIAL — 2/4)

| # | Topic | Status |
|---|-------|--------|
| 1 | Active EMI Filter (AEF) for GaN/SiC | `active_emi_filter_design()` ✅ |
| 2 | 6G/Terahertz EMI | Documented only |
| 3 | Quantum Sensing for EMC | Documented only |
| 4 | AI-Based Filter Optimization | Documented only |
