# Knowledge Graph — ESD Gun Air/Contact Test

Nine-level knowledge coverage map for IEC 61000-4-2 ESD testing.

---

## L1 — Definitions

| # | Definition | C Implementation | Lean Formalization |
|---|-----------|-----------------|-------------------|
| 1 | IEC 61000-4-2 severity levels (1-4) | `esd_test_level_t` enum | `ESDLevel` inductive type |
| 2 | Contact discharge voltage (2,4,6,8 kV) | `contact_voltage_kv[]` | `contactVoltage` |
| 3 | Air discharge voltage (2,4,8,15 kV) | `air_voltage_kv[]` | `airVoltage` |
| 4 | ESD current waveform parameters (Ip, tr, I30, I60) | `esd_waveform_spec_t` | `peakCurrent`, `currentAt30ns`, `currentAt60ns` |
| 5 | ESD gun equivalent circuit (150pF/330Ω) | `esd_gun_rc_params_t` | — |
| 6 | Ground Reference Plane (GRP) | `esd_test_geometry_t` | — |
| 7 | Horizontal/Vertical Coupling Plane (HCP/VCP) | `esd_test_geometry_t` | — |
| 8 | Performance criteria (Pass A/B/C, Fail D) | `esd_test_result_t` enum | — |
| 9 | TVS diode parameters (VRWM, VBR, VC, IPP) | `esd_tvs_params_t` | — |
| 10 | Varistor parameters (V1mA, α, energy rating) | `esd_varistor_params_t` | — |
| 11 | HBM classification (Class 0-3B) | `esd_hbm_class_t` enum | `HBMClass` inductive type |
| 12 | TLP system parameters (Z₀, τ, tr) | `esd_tlp_system_t` | — |
| 13 | VFTLP parameters | `esd_vftlp_params_t` | — |
| 14 | SEED methodology parameters | `esd_seed_params_t` | — |
| 15 | HMM definition | `esd_hmm_measurement_t` | — |

---

## L2 — Core Concepts

| # | Concept | Implementation |
|---|---------|---------------|
| 1 | Air discharge vs contact discharge mechanism | `esd_discharge_type_t`, arc simulation |
| 2 | RLC damping regimes (under/critical/overdamped) | `esd_gun_rlc_damping_regime()` |
| 3 | ESD coupling mechanisms (inductive, capacitive) | `esd_coupling_induced_voltage()` |
| 4 | ESD protection window concept | `esd_protection_window_t` |
| 5 | Snapback behavior in protection devices | `esd_tlp_detect_snapback()` |
| 6 | System-level vs device-level ESD | Cross-standard equivalence mapping |
| 7 | Indirect vs direct ESD discharge | `esd_discharge_point_t` |
| 8 | Common-mode vs differential-mode ESD coupling | Mutual inductance model |
| 9 | Arc resistance nonlinearity during discharge | Rompe-Weizel & Toepler models |
| 10 | TLP quasi-static I-V characterization | `esd_tlp_curve_t` |

---

## L3 — Mathematical Structures

| # | Structure | Implementation |
|---|-----------|---------------|
| 1 | Heidler function (two-component sigmoid-exponential) | `esd_heidler_waveform()` |
| 2 | Double-exponential pulse model | `esd_double_exp_waveform()` |
| 3 | RLC state-space ODEs (dVc/dt, dI/dt) | `rlc_derivatives()` |
| 4 | Characteristic equation: Ls² + Rs + 1/C = 0 | `esd_gun_rlc_natural()` |
| 5 | Paschen's law: V_bd = f(p·d) | `esd_paschen_breakdown_voltage()` |
| 6 | TVS piecewise I-V model | `esd_tvs_clamp_voltage()` |
| 7 | Varistor power-law: I = k·V^α | `esd_varistor_voltage()` |
| 8 | Mutual inductance of parallel conductors | `esd_mutual_inductance_parallel()` |
| 9 | Transmission line pulse equation | `esd_tlp_pulse_width_from_length()` |
| 10 | TLP reflection coefficient: Γ = (R-Z₀)/(R+Z₀) | `esd_tlp_expected_current()` |
| 11 | Rompe-Weizel spark resistance: R ∝ d/√(∫i²dt) | `esd_gun_simulate_with_arc_rw()` |
| 12 | Toepler spark law: R ∝ d/∫i dt | `esd_gun_simulate_with_arc_toepler()` |

---

## L4 — Fundamental Laws

| # | Law | Code Verification | Lean Formalization |
|---|-----|------------------|-------------------|
| 1 | IEC peak current: Ip = V × 3.75 A/kV | `esd_peak_current_from_voltage()` | `peak_current_linear_scaling` |
| 2 | Energy conservation: E = ½CV² | `esd_gun_stored_energy()` | `energy_ratio_level4_to_level1` |
| 3 | Paschen's law (monotonic, non-zero minimum) | `esd_paschen_breakdown_voltage()` | `paschen_monotonic`, `paschen_positive_at_zero` |
| 4 | RLC overdamping: ζ > 1 for IEC gun | `esd_gun_rlc_damping_regime()` | `iec_gun_overdamped` |
| 5 | TLP pulse width: τ = 2l/(vf·c) | `esd_tlp_pulse_width_from_length()` | `tlp_10m_cable_100ns`, `tlp_width_linear` |
| 6 | Charge conservation: Q = CV | `esd_waveform_total_charge()` | `charge_iec_level4_nc` |
| 7 | Compliance margin: M = Vfail/Vreq | `esd_compliance_margin_compute()` | — |
| 8 | Faraday's law: V_induced = M·dI/dt | `esd_coupling_induced_voltage()` | — |
| 9 | Power dissipation: P = I²R | `esd_waveform_energy()` | — |
| 10 | Levels strictly increasing by voltage | `esd_level_voltage_kv()` | `levels_strictly_increasing` |

---

## L5 — Algorithms/Methods

| # | Algorithm | Implementation | Complexity |
|---|-----------|---------------|------------|
| 1 | Heidler waveform evaluation | `esd_heidler_waveform()` | O(1) |
| 2 | IEC waveform generation (sampled) | `esd_waveform_generate_iec()` | O(N) |
| 3 | Waveform parameter extraction (tr, Ip, I30, I60) | `esd_waveform_extract_params()` | O(N) |
| 4 | IEC compliance checking with tolerances | `esd_waveform_check_compliance()` | O(1) |
| 5 | RK4 integration for RLC discharge | `esd_gun_simulate_rlc_rk4()` | O(N) |
| 6 | Analytical RLC solution (3 damping regimes) | `esd_gun_simulate_rlc_analytical()` | O(N) |
| 7 | Arc discharge simulation (nonlinear R_arc) | `esd_gun_simulate_with_arc_rw()` | O(N) |
| 8 | Rise time measurement (10%-90% interpolation) | `esd_waveform_rise_time()` | O(N) |
| 9 | FWHM pulse width measurement | `esd_waveform_fwhm()` | O(N) |
| 10 | Trapezoidal charge integration | `esd_waveform_total_charge()` | O(N) |
| 11 | TVS selection algorithm (multi-criteria) | `esd_tvs_select()` | O(1) |
| 12 | TVS bandwidth estimation (digital/analog) | `esd_tvs_bandwidth_limit()` | O(1) |
| 13 | RC ESD filter design | `esd_rc_filter_design()` | O(1) |
| 14 | Test plan discharge counting | `esd_test_plan_total_discharges()` | O(1) |
| 15 | ESD test statistics aggregation | `esd_test_report_statistics()` | O(N) |
| 16 | HBM classification algorithm | `esd_hbm_classify()` | O(1) |
| 17 | TLP parameter extraction (Vt1, Vh, Ron, It2) | `esd_tlp_extract_params()` | O(N) |
| 18 | Snapback detection (voltage drop on current rise) | `esd_tlp_detect_snapback()` | O(N) |
| 19 | TLP Figure of Merit: FOM = It2/(Cj·Vh) | `esd_tlp_figure_of_merit()` | O(1) |
| 20 | SEED current division analysis | `esd_seed_analyze()` | O(1) |
| 21 | Gap length estimation from voltage | `esd_voltage_to_gap_length()` | O(1) |

---

## L6 — Canonical Problems

| # | Problem | Solution | Example |
|---|---------|----------|---------|
| 1 | Generate IEC-compliant ESD waveform | `esd_waveform_generate_iec()` + compliance check | `example_waveform.c` |
| 2 | Simulate contact discharge with arc model | `esd_gun_simulate_with_arc_rw()` | Arc simulation test |
| 3 | Verify ESD waveform meets IEC spec | `esd_waveform_check_compliance()` | `example_waveform.c` |
| 4 | Select TVS for high-speed interface | `esd_tvs_select()` + bandwidth check | `example_protection.c` |
| 5 | Design two-stage ESD protection network | `esd_network_simulate()` | `example_protection.c` |
| 6 | Execute full IEC compliance test | `esd_test_plan_standard()` + report | `example_compliance.c` |
| 7 | Extract TLP I-V parameters from measurement | `esd_tlp_extract_params()` | TLP extraction test |
| 8 | Determine HBM classification from failure V | `esd_hbm_classify()` | `example_compliance.c` |

---

## L7 — Applications

| # | Application | Implementation | Keywords |
|---|------------|---------------|----------|
| 1 | Automotive ESD (ISO 10605) | `esd_auto_test_params()` | ISO, Toyota, Detroit |
| 2 | Medical device ESD (IEC 60601-1-2) | `esd_medical_requirements()` | NHS, life-support |
| 3 | Consumer electronics PCB layout | `esd_pcb_trace_width()` | iPhone, USB protection |
| 4 | Telecom equipment ESD (GR-1089-CORE) | `esd_standard_get(ESD_STD_GR_1089_CORE)` | Telecom, supplier |

---

## L8 — Advanced Topics

| # | Topic | Implementation |
|---|-------|---------------|
| 1 | System-Efficient ESD Design (SEED) | `esd_seed_analyze()`, `esd_seed_optimal_impedance()` |
| 2 | Very-Fast TLP (VFTLP) | `esd_tlp_system_vftlp()` |
| 3 | Human Metal Model (HMM) | `esd_hmm_equivalent_current()` |
| 4 | Stochastic ESD failure analysis | Thermal failure model: `esd_tvs_temp_rise()` |

---

## L9 — Research Frontiers

| # | Topic | Status |
|---|-------|--------|
| 1 | 3D ESD protection for advanced CMOS nodes | Documented only |
| 2 | Sub-nanometer node CDM characterization | Documented only |
| 3 | mm-Wave ESD protection structures | Documented only |
| 4 | AI-assisted ESD protection optimization | Documented only |
| 5 | System-level ESD for autonomous vehicles | Documented only |
