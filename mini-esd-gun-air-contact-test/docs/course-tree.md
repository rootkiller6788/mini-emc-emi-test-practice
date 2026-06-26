# Course Tree — ESD Gun Air/Contact Test

Prerequisite dependency tree for knowledge items in this module.

---

## Dependency Graph

```
L1: Definitions
├── ESD levels, voltages, waveform params
├── ESD gun RC (150pF/330Ω)
├── Test setup geometry (GRP/HCP/VCP)
├── TVS/Varistor/GDT device params
├── HBM classification
└── TLP system parameters
     │
     ▼
L2: Core Concepts
├── Air vs contact discharge  ← L1 ESD levels
├── RLC damping regimes  ← L1 ESD gun RC
├── ESD coupling paths  ← L1 test setup
├── Protection window  ← L1 TVS params
├── Snapback behavior  ← L1 TVS params
├── System vs device ESD  ← L1 HBM classification
└── Arc nonlinearity  ← L2 air discharge
     │
     ▼
L3: Mathematical Structures
├── Heidler function  ← L2 discharge mechanism
├── Double-exponential model  ← L2 discharge mechanism
├── RLC state-space ODEs  ← L2 RLC damping
├── Paschen's law  ← L2 air discharge
├── TVS piecewise I-V  ← L2 protection window
├── Varistor power-law  ← L2 protection window
├── Mutual inductance  ← L2 ESD coupling
├── TLP transmission line  ← L2 TLP
├── Rompe-Weizel resistance  ← L2 arc nonlinearity
└── Toepler spark law  ← L2 arc nonlinearity
     │
     ▼
L4: Fundamental Laws
├── Peak current scaling (Ip = 3.75·V)  ← L3 Heidler function
├── Energy conservation (E = ½CV²)  ← L3 RLC ODEs
├── Paschen's law (formal proof)  ← L3 Paschen's law
├── RLC overdamping (formal proof)  ← L3 RLC ODEs
├── TLP pulse width (formal proof)  ← L3 TLP transmission line
├── Charge conservation (Q = CV)  ← L3 RLC ODEs
├── Compliance margin  ← L3 (all)
└── Faraday coupling (V = M·dI/dt)  ← L3 mutual inductance
     │
     ▼
L5: Algorithms/Methods
├── Waveform evaluation/generation  ← L4 peak current
├── Parameter extraction  ← L4 waveform spec
├── Compliance check  ← L4 waveform spec
├── RK4 integration  ← L4 RLC ODEs
├── Analytical RLC solution  ← L4 RLC ODEs
├── Arc simulation  ← L4 Paschen + L3 arc models
├── TVS selection  ← L4 protection requirements
├── RC filter design  ← L4 signal bandwidth
├── TLP parameter extraction  ← L4 TLP theory
├── Snapback detection  ← L4 TLP theory
├── TLP FOM calculation  ← L4 TLP theory
└── SEED analysis  ← L4 current division
     │
     ▼
L6: Canonical Problems
├── IEC waveform compliance  ← L5 waveform generation + check
├── Contact discharge simulation  ← L5 arc simulation
├── TVS protection design  ← L5 TVS selection
├── Multi-stage network design  ← L5 TVS selection
├── Full compliance test plan  ← L5 compliance methods
└── TLP I-V characterization  ← L5 TLP extraction
     │
     ▼
L7: Applications
├── Automotive ESD (ISO 10605)  ← L6 compliance
├── Medical device ESD  ← L6 compliance
├── Consumer electronics PCB  ← L6 protection design
└── Telecom ESD  ← L6 compliance
     │
     ▼
L8: Advanced Topics
├── SEED methodology  ← L7 system design + L5 TLP
├── VFTLP  ← L5 TLP extraction
├── HMM  ← L7 applications
└── Thermal failure analysis  ← L4 energy + L5 TVS model
     │
     ▼
L9: Research Frontiers
├── 3D ESD protection  ← L8 advanced TVS
├── Sub-nm CDM  ← L8 VFTLP
├── mm-Wave ESD  ← L8 device models
├── AI ESD optimization  ← L7 applications
└── Autonomous vehicle ESD  ← L7 automotive
```

---

## Prerequisite Modules (Inter-module Dependencies)

This module depends on:

| Prerequisite Module | Knowledge Required |
|--------------------|--------------------|
| mini-circuit-analysis | RLC circuit theory, transient analysis |
| mini-electromagnetic-wave | Maxwell's equations, wave propagation, mutual inductance |
| mini-emc-signal-integrity | EMC fundamentals, coupling mechanisms, shielding |
| mini-sensor-measurement | Measurement uncertainty, calibration, data acquisition |

---

## Course Sequence Recommendation

1. **First**: mini-circuit-analysis (RLC fundamentals)
2. **Second**: mini-electromagnetic-wave (EM coupling)
3. **Third**: mini-emc-signal-integrity (EMC concepts)
4. **Fourth**: mini-esd-gun-air-contact-test (this module — applied ESD testing)
