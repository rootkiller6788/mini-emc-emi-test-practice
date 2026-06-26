# mini-esd-gun-air-contact-test

**IEC 61000-4-2 ESD Gun Air/Contact Discharge Test Module**

Electrostatic Discharge (ESD) immunity testing using the IEC 61000-4-2
ESD simulator (ESD gun), covering both contact and air discharge methods.
Implements waveform modeling, gun circuit simulation, test setup
configuration, protection device selection, cross-standard compliance,
and TLP characterization.

---

## Module Status: COMPLETE ✅

- L1-L6: Complete
- L7: Complete (4 applications: automotive, medical, consumer, telecom)
- L8: Complete (SEED methodology, VFTLP, HMM)
- L9: Partial (documented in knowledge-graph.md)

---

## 九层知识覆盖摘要表 (Nine-Level Knowledge Coverage)

| Level | Name | Status | Key Items |
|-------|------|--------|-----------|
| **L1** | Definitions | **Complete** | ESD test levels, contact/air discharge, waveform parameters, TVS/Varistor/GDT definitions, HBM/MM/CDM models, TLP/VFTLP definitions |
| **L2** | Core Concepts | **Complete** | Air gap breakdown, contact mechanism, RLC damping regimes, ESD protection window, snapback behavior, system vs device-level ESD, coupling mechanisms |
| **L3** | Math Structures | **Complete** | Heidler function, double-exponential model, RLC state-space ODEs, Paschen's law, TVS piecewise I-V, varistor power-law, mutual inductance, TLP transmission line |
| **L4** | Fundamental Laws | **Complete** | IEC 61000-4-2 waveform equation, energy conservation (E=½CV²), Paschen's law, TLP pulse width theorem, charge conservation, RLC natural frequency |
| **L5** | Algorithms/Methods | **Complete** | RK4 integration, analytical RLC solution, waveform parameter extraction, TVS selection algorithm, TLP I-V extraction, RC filter design, compliance margin |
| **L6** | Canonical Problems | **Complete** | IEC compliance waveform generation, air discharge arc simulation, full test plan execution, TVS protection network design, TLP curve analysis |
| **L7** | Applications | **Complete** | Automotive ESD (ISO 10605), Medical device ESD (IEC 60601-1-2), Consumer electronics PCB layout, Telecom ESD (GR-1089-CORE) |
| **L8** | Advanced Topics | **Complete** | System-Efficient ESD Design (SEED), Very-Fast TLP (VFTLP), Human Metal Model (HMM), Transient latch-up characterization |
| **L9** | Research Frontiers | **Partial** | 3D ESD protection, sub-nm node CDM, mm-wave ESD protection (documented only) |

---

## 核心定义列表 (Core Definitions)

| 定义 | 符号 | 值/范围 |
|------|------|---------|
| IEC Contact Discharge Levels | Level 1-4 | 2, 4, 6, 8 kV |
| IEC Air Discharge Levels | Level 1-4 | 2, 4, 8, 15 kV |
| Peak Current @ 8kV Contact | Ip | 30 A |
| Rise Time (Contact) | tr | 0.7-1.0 ns |
| ESD Gun Capacitance | Cs | 150 pF |
| ESD Gun Discharge Resistor | Rd | 330 Ω |
| Parasitic Inductance | Lpar | 50-200 nH |
| HBM Capacitance | Chbm | 100 pF |
| HBM Resistance | Rhbm | 1500 Ω |
| TVS Clamping Voltage | Vc | 5-40 V (device-dependent) |
| Varistor Nonlinearity | α | 20-60 (ZnO) |
| TLP Pulse Width | τtlp | 100 ns (standard) |
| Paschen Minimum (Air) | Vb,min | ~3000 V |

---

## 核心定理列表 (Core Theorems)

### 1. IEC 61000-4-2 Peak Current Scaling
```
I_peak [A] = V_charge [kV] × 3.75
```
Verified across all four IEC levels: 2kV→7.5A, 4kV→15A, 6kV→22.5A, 8kV→30A.
*Implemented in `esd_peak_current_from_voltage()`, formalized in `peak_current_linear_scaling` (Lean)*

### 2. Stored Energy
```
E = ½·C·V²
```
For IEC gun (150 pF): 2kV→300 µJ, 4kV→1200 µJ, 6kV→2700 µJ, 8kV→4800 µJ.
*Implemented in `esd_gun_stored_energy()`, formalized in `energy_ratio_level4_to_level1` (Lean)*

### 3. Paschen's Law (Air Breakdown)
```
V_bd [V] = 3000 + 1000·d [mm]  (1 atm)
```
Monotonic in gap length; non-zero minimum (~3000 V at d→0).
*Implemented in `esd_paschen_breakdown_voltage()`, formalized in `paschen_monotonic` (Lean)*

### 4. RLC Natural Frequency
```
ω₀ = 1/√(L·C),  ζ = R/(2)·√(C/L)
```
For standard gun (R=332Ω, L=80nH, C=150pF): ζ ≈ 7.2 (overdamped).
*Implemented in `esd_gun_rlc_natural()`, formalized in `iec_gun_overdamped` (Lean)*

### 5. TLP Pulse Width
```
τ = 2·l_cable / (v_prop_factor · c)
```
10m RG-58 (vf=0.66) → τ ≈ 101 ns.
*Implemented in `esd_tlp_pulse_width_from_length()`, formalized in `tlp_10m_cable_100ns` (Lean)*

### 6. Charge Conservation
```
Q = C·V
```
IEC gun Level 4: Q = 150 pF × 8 kV = 1.2 µC.
*Implemented in `esd_waveform_total_charge()`, formalized in `charge_iec_level4_nc` (Lean)*

### 7. Compliance Margin
```
Margin = V_failure / V_required,  Margin_dB = 20·log₁₀(Margin)
```
*Implemented in `esd_compliance_margin_compute()`*

### 8. Mutual Inductance (Parallel Conductors)
```
M = (μ₀·l/2π)·[ln(2l/d) - 1 + d/l]
```
*Implemented in `esd_mutual_inductance_parallel()`*

---

## 核心算法列表 (Core Algorithms)

| 算法 | 文件 | 复杂度 |
|------|------|--------|
| Heidler Function Evaluation | `esd_waveform.c:esd_heidler_waveform()` | O(1) |
| IEC Waveform Generation | `esd_waveform.c:esd_waveform_generate_iec()` | O(N) |
| Waveform Parameter Extraction | `esd_waveform.c:esd_waveform_extract_params()` | O(N) |
| IEC Compliance Check | `esd_waveform.c:esd_waveform_check_compliance()` | O(1) |
| RK4 RLC Simulation | `esd_gun_model.c:esd_gun_simulate_rlc_rk4()` | O(N) |
| Analytical RLC Solution | `esd_gun_model.c:esd_gun_simulate_rlc_analytical()` | O(N) |
| Arc Resistance Simulation (Rompe-Weizel) | `esd_gun_model.c:esd_gun_simulate_with_arc_rw()` | O(N) |
| Arc Resistance Simulation (Toepler) | `esd_gun_model.c:esd_gun_simulate_with_arc_toepler()` | O(N) |
| TVS Selection | `esd_protection.c:esd_tvs_select()` | O(1) |
| TVS Bandwidth Limit | `esd_protection.c:esd_tvs_bandwidth_limit()` | O(1) |
| RC Filter Design | `esd_protection.c:esd_rc_filter_design()` | O(1) |
| Two-Stage Protection Simulation | `esd_protection.c:esd_network_simulate()` | O(1) |
| Test Plan Sizing | `esd_test_setup.c:esd_test_plan_total_discharges()` | O(1) |
| ESD Coupling Voltage | `esd_test_setup.c:esd_coupling_induced_voltage()` | O(1) |
| HBM Classification | `esd_compliance.c:esd_hbm_classify()` | O(1) |
| Cross-Standard Mapping | `esd_compliance.c:esd_cross_standard_map()` | O(1) |
| TLP Parameter Extraction | `esd_tlp.c:esd_tlp_extract_params()` | O(N) |
| Snapback Detection | `esd_tlp.c:esd_tlp_detect_snapback()` | O(N) |
| TLP FOM Calculation | `esd_tlp.c:esd_tlp_figure_of_merit()` | O(1) |
| SEED Analysis | `esd_tlp.c:esd_seed_analyze()` | O(1) |

---

## 经典问题列表 (Canonical Problems)

| 问题 | 示例文件 | 描述 |
|------|---------|------|
| IEC Waveform Compliance | `examples/example_waveform.c` | Generate & verify IEC-compliant ESD waveform |
| ESD Protection Design | `examples/example_protection.c` | Select TVS for USB 3.0 port, multi-stage network |
| Full Compliance Test | `examples/example_compliance.c` | Complete IEC/ISO/HBM test simulation |

---

## 九校课程映射 (Course Alignment)

| 学校 | 课程 | 对应知识点 |
|------|------|-----------|
| **MIT** | 6.630 EM Waves | Pulse propagation, RLC transients, transmission lines |
| **Stanford** | EE359 Wireless | ESD in wireless systems, system-level immunity |
| **Berkeley** | EE117 EM | Electromagnetic coupling, mutual inductance, shielding |
| **Berkeley** | EE105 Analog | Protection circuits, TVS diodes, snapback |
| **ETH** | 227-0455 High-Frequency | Transmission line pulses, fast transients |
| **清华** | 电磁场 | 静电放电物理, 气体击穿 (Paschen定律) |
| **Michigan** | EECS 411 Microwave | Transmission line theory, TLP principles |
| **TU Munich** | High-Frequency Eng. | ESD in automotive electronics, ISO 10605 |

---

## 文件结构

```
mini-esd-gun-air-contact-test/
├── Makefile                    # make test builds and runs all tests
├── README.md                   # This file
├── include/
│   ├── esd_waveform.h          # ESD waveform parameters & IEC model (L1-L5)
│   ├── esd_gun_model.h         # ESD gun RLC circuit & arc models (L2-L4)
│   ├── esd_test_setup.h        # Test setup geometry & plan management (L1,L6)
│   ├── esd_protection.h        # TVS/Varistor/RC filter models (L1-L6)
│   ├── esd_compliance.h        # Cross-standard compliance (L1,L7)
│   └── esd_tlp.h               # TLP/VFTLP/SEED/HMM (L1-L8)
├── src/
│   ├── esd_waveform.c          # Heidler/double-exp waveform computation
│   ├── esd_gun_model.c         # RK4 simulation, analytical solution, arc models
│   ├── esd_test_setup.c        # Test plan execution, coupling analysis
│   ├── esd_protection.c        # TVS selection, thermal analysis, PCB layout
│   ├── esd_compliance.c        # HBM class, margin, cross-standard mapping
│   ├── esd_tlp.c               # TLP extraction, snapback, SEED, HMM
│   └── esd_theorems.lean       # Lean 4 formalization (18 theorems)
├── tests/
│   └── test_esd.c              # 40 assert-based tests, all passing
├── examples/
│   ├── example_waveform.c      # IEC waveform generation & compliance check
│   ├── example_protection.c    # USB 3.0 ESD protection design
│   └── example_compliance.c    # Full compliance test simulation
├── demos/                      # (reserved)
├── benches/                    # (reserved)
└── docs/
    ├── knowledge-graph.md
    ├── coverage-report.md
    ├── gap-report.md
    ├── course-alignment.md
    └── course-tree.md
```

---

## Build & Test

```bash
make          # Build library and test
make test     # Build and run 40 tests (all passing)
make examples # Build all 3 example programs
make clean    # Remove build artifacts
```

---

## Key References

- **IEC 61000-4-2:2008** — Electromagnetic compatibility (EMC) — Part 4-2: Testing and measurement techniques — Electrostatic discharge immunity test
- **ISO 10605:2008** — Road vehicles — Test methods for electrical disturbances from electrostatic discharge
- **ANSI/ESDA/JEDEC JS-001-2017** — For Electrostatic Discharge Sensitivity Testing — Human Body Model (HBM)
- **ANSI/ESD SP5.5.1** — Transmission Line Pulse (TLP) testing
- **Paul, C.R.** — "Introduction to Electromagnetic Compatibility" (2006), Chapter 13: Electrostatic Discharge
- **Industry Council on ESD Target Levels** — White Paper 3: System-Efficient ESD Design (SEED)
