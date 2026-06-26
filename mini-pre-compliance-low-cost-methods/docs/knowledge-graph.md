# Knowledge Graph — Low-Cost EMC Pre-Compliance Methods

## L1: Definitions (Complete)
| # | Term | C Type | Description |
|---|------|--------|-------------|
| 1 | Emission Point | `emi_emission_point_t` | Frequency-domain measurement: amplitude, margin, status |
| 2 | Scan Configuration | `emi_scan_config_t` | RBW/VBW/sweep time/detector settings |
| 3 | Scan Result | `emi_scan_result_t` | Complete pre-scan dataset with peak/margin stats |
| 4 | Limit Line | `emi_limit_line_t` | Piecewise log-linear emission limit curve |
| 5 | Path Loss | `emi_path_loss_t` | Cable/attenuator frequency-dependent loss |
| 6 | E-Field Probe Spec | `emi_eprobe_spec_t` | Monopole near-field probe characteristics |
| 7 | H-Field Probe Spec | `emi_hprobe_spec_t` | Shielded loop near-field probe characteristics |
| 8 | LISN Spec | `emi_lisn_spec_t` | Line Impedance Stabilization Network (CISPR 16-1-2) |
| 9 | TEM Cell Spec | `emi_tem_cell_spec_t` | Transverse EM cell geometry and field factor |
| 10 | Antenna Spec | `emi_antenna_spec_t` | Measurement antenna with AF calibration table |
| 11 | SA Spec | `emi_sa_spec_t` | Spectrum analyzer / EMI receiver specs |
| 12 | Current Probe Spec | `emi_current_probe_spec_t` | RF current clamp transfer impedance |
| 13 | DIY Probe Spec | `emi_diy_probe_spec_t` | Low-cost probe construction parameters |
| 14 | Uncertainty Budget | `emi_uncertainty_budget_t` | CISPR 16-4-2 uncertainty budget |
| 15 | Correlation Result | `emi_correlation_result_t` | Pre-compliance ↔ full compliance mapping |
| 16 | Statistics | `emi_statistics_t` | Mean/variance/skewness/kurtosis |
| 17 | Regression | `emi_regression_t` | Linear fit with R² and Pearson r |

## L2: Core Concepts (Complete)
| # | Concept | Implementation |
|---|---------|---------------|
| 1 | Pre-compliance vs. full compliance | `emi_compute_correlation()`, `emi_recommend_precompliance_margin()` |
| 2 | E-field vs. H-field probe loading | `emi_eprobe_output_voltage()`, `emi_hprobe_output_voltage()` |
| 3 | LISN impedance at EUT port | `emi_lisn_impedance_mag()`, `emi_lisn_voltage_division_factor_db()` |
| 4 | TEM cell field uniformity | `emi_tem_cell_field_strength()`, `emi_tem_cell_max_frequency()` |
| 5 | Measurement distance effects | `emi_distance_correction_db()`, `emi_far_field_distance()` |
| 6 | Ground plane and site effects | `emi_normalized_site_attenuation()` |
| 7 | Spectrum analyzer RBW/VBW trade-offs | `emi_min_sweep_time()`, `emi_auto_vbw()`, `emi_optimize_rbw()` |
| 8 | Diy probe vs commercial probe | `emi_diy_hprobe_init()`, `emi_diy_eprobe_init()` |
| 9 | Ambient noise management | `emi_ambient_cancellation()`, `emi_signal_ambient_ratio()` |
| 10 | Cost-accuracy trade-offs | `emi_diy_current_probe_init()`, `emi_sa_init_rtlsdr()` |

## L3: Mathematical Structures (Complete)
| # | Structure | Implementation |
|---|-----------|---------------|
| 1 | Unit conversions (dBm/dBuV/V/m/W) | 14 inline conversion functions in core.h |
| 2 | Log-linear limit interpolation | `emi_limit_line_interpolate()` |
| 3 | Complex impedance network (LISN) | `emi_lisn_impedance_mag()` |
| 4 | Faraday's law (H-field probe) | `emi_hprobe_output_voltage()` |
| 5 | Biot-Savart / Coulomb (E-field probe) | `emi_eprobe_capacitance()` |
| 6 | Elliptic integrals (mutual inductance) | `elliptic_k()`, `elliptic_e()` in probe_theory.c |
| 7 | Statistical descriptors | `emi_compute_statistics()` |
| 8 | Linear regression | `emi_linear_regression()` |
| 9 | Correlation coefficients | `emi_pearson_correlation()`, `emi_spearman_correlation()` |
| 10 | Wave number and impedance transition | `emi_near_field_wave_impedance()` |

## L4: Fundamental Laws (Complete)
| # | Law/Theorem | C Verification | Lean Statement |
|---|------------|---------------|----------------|
| 1 | Friis Transmission Equation (AF-Gain) | `emi_antenna_factor_from_gain()` | `antenna_factor_gain_relation` |
| 2 | Faraday's Law (probe response) | `emi_hprobe_output_voltage()` | — |
| 3 | CISPR 16-1-1 QP detector weighting | `emi_qp_detector_response()` | `qp_detector_steady_state` |
| 4 | Thermal noise floor (kTB) | `emi_noise_floor_dbm()` | `mds_noise_floor` |
| 5 | GUM Law of Propagation | `emi_combined_standard_uncertainty()` | — |
| 6 | Welch-Satterthwaite formula | `emi_effective_degrees_of_freedom()` | — |
| 7 | Fraunhofer far-field criterion | `emi_far_field_distance()` | `far_field_distance`, `is_far_field` |
| 8 | Wavelength-frequency reciprocity | `emi_wavelength()` | `wavelength_frequency_reciprocal` |
| 9 | CISPR decision rule | `emi_ucispr_decision()` | `margin_decision`, `MarginDecision` |
| 10 | NSA ground-plane reflection model | `emi_normalized_site_attenuation()` | — |

## L5: Algorithms/Methods (Complete)
| # | Algorithm | Function | Complexity |
|---|-----------|----------|------------|
| 1 | Peak pre-scan automation | `emi_peak_prescan()` | O(N) |
| 2 | Final QP measurement | `emi_final_qp_measurement()` | O(N) |
| 3 | Emission peak search | `emi_find_peaks()` | O(N) |
| 4 | Ambient noise cancellation | `emi_ambient_cancellation()` | O(N) |
| 5 | NB/BB signal classification | `emi_classify_nb_bb()` | O(1) |
| 6 | E-field probe calibration | `emi_calibrate_eprobe()` | O(N) |
| 7 | H-field probe calibration | `emi_calibrate_hprobe()` | O(N) |
| 8 | Three-antenna method (AF) | `emi_three_antenna_method()` | O(1) |
| 9 | Cal factor interpolation | `emi_interpolate_cal_factor()` | O(log N) |
| 10 | RBW optimization | `emi_optimize_rbw()` | O(1) |
| 11 | Spearman rank correlation | `emi_spearman_correlation()` | O(N²) |
| 12 | Ucispr budget finalization | `emi_ucispr_finalize()` | O(M) |
| 13 | Pre-compliance confidence | `emi_precompliance_confidence()` | O(1) |
| 14 | MC coverage interval | `emi_mc_coverage_interval()` | O(N log N) |

## L6: Canonical Problems (Complete)
| # | Problem | Implementation |
|---|---------|---------------|
| 1 | CISPR 32 conducted pre-scan | `emi_limit_init_cispr32_conducted_classb()` + `test_smps` example |
| 2 | FCC Part 15 radiated pre-scan | `emi_limit_init_fcc15_radiated_classb()` + `test_arduino` example |
| 3 | DIY near-field probe calibration | `test_probe_calibration` example |
| 4 | Ucispr uncertainty budget (conducted) | `emi_ucispr_conducted_init()` + test |
| 5 | Ucispr uncertainty budget (radiated) | `emi_ucispr_radiated_init()` + test |
| 6 | Correlation pre-compliance ↔ full | `emi_compute_correlation()` + test |

## L7: Applications (Complete — 4 applications)
| # | Application | File |
|---|-------------|------|
| 1 | SMPS conducted emissions pre-scan | `examples/example_smps_prescan.c` |
| 2 | Arduino board radiated emissions check | `examples/example_arduino_emi_check.c` |
| 3 | DIY probe calibration workflow | `examples/example_probe_calibration.c` |
| 4 | IoT/embedded device EMC pre-check | Implemented in arduino example + core lib |

## L8: Advanced Topics (Complete — 2 topics)
| # | Topic | Implementation |
|---|-------|---------------|
| 1 | Monte Carlo uncertainty propagation | `emi_monte_carlo_uncertainty()` |
| 2 | Stochastic compliance confidence | `emi_compliance_pass_probability()` |

## L9: Research Frontiers (Partial — documented)
| # | Topic | Status |
|---|-------|--------|
| 1 | 6G RIS-based EMI suppression | Documented |
| 2 | AI-driven EMC diagnostics | Documented |
| 3 | Time-domain EMI (TDEMI) | Documented |
