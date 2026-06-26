# Course Tree — CM/DM EMI Filter Prerequisites

```
CM/DM EMI Filter Design
│
├── ELECTROMAGNETICS (L4)
│   ├── Faraday's Law → CM choke inductance
│   │   └── L = N²·μ₀·μ_r·A_e/l_e
│   ├── Maxwell's Equations
│   │   └── Gauss → capacitance, Ampere → inductance
│   ├── Magnetic Materials
│   │   ├── Ferrite (MnZn, NiZn)
│   │   ├── Powder cores (MPP, Kool Mμ, High Flux)
│   │   └── Nanocrystalline / Amorphous
│   └── Core Loss Physics
│       ├── Hysteresis loss (Steinmetz)
│       ├── Eddy current loss
│       └── Excess (anomalous) loss
│
├── CIRCUIT THEORY (L2-L3)
│   ├── Complex Impedance → Z(jω) = R + jX(ω)
│   ├── s-Domain Transfer Functions → H(s) = V_out(s)/V_in(s)
│   ├── Two-Port Networks → Z, Y, ABCD, S parameters
│   ├── Network Cascading → ABCD multiplication
│   ├── Mixed-Mode Analysis → S_dd, S_cc, S_cd, S_dc
│   ├── Kirchhoff's Laws → Nodal/mesh admittance matrix
│   └── Resonance & Damping → f_res, Q, damping resistance
│
├── SIGNAL PROCESSING (L3-L5)
│   ├── Fourier Analysis → noise spectrum decomposition
│   ├── Bode Plots → frequency response visualization
│   ├── dB and Logarithmic Scales → IL, CMRR, EMC limits
│   ├── CM/DM Decomposition → Clarke transform (3φ)
│   └── Filter Order & Roll-off → 20·N dB/decade
│
├── EMC STANDARDS & REGULATIONS (L6-L7)
│   ├── CISPR 11/22/32 → conducted emission limits
│   ├── FCC Part 15 → USA regulatory limits
│   ├── MIL-STD-461 → military EMC
│   ├── CISPR 17 → IL measurement methods
│   ├── RTCA DO-160 → aerospace EMC
│   └── CISPR 25 → automotive EMC
│
├── SAFETY STANDARDS (L2-L7)
│   ├── IEC 62368-1 → creepage/clearance
│   ├── Y-cap leakage limits → ≤3.5 mA (ITE)
│   ├── X-cap bleed resistor → discharge <1s
│   └── IEC 60601-1 → medical safety (≤0.5 mA)
│
├── POWER ELECTRONICS (L7)
│   ├── SMPS Topologies → flyback, LLC, PFC
│   ├── Motor Drives → PWM, VFD
│   ├── Solar Inverters → grid-tied
│   └── Wide-Bandgap (GaN/SiC) → high dv/dt
│
├── RELIABILITY ENGINEERING (L8)
│   ├── MIL-HDBK-217F → parts count prediction
│   ├── Temperature Derating → component stress
│   └── Arrhenius Model → failure rate vs temperature
│
└── ADVANCED TOPICS (L8-L9)
    ├── Active EMI Filters → GaN/SiC era
    ├── Impedance Interaction → Middlebrook criterion
    ├── AI Optimization → Pareto frontier
    └── Research → 6G sub-THz, quantum EMC sensing
```
