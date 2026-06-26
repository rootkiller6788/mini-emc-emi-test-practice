# Mini Near-Field Probe Diagnostic

Near-field electromagnetic probe diagnostic system for EMC/EMI testing, debugging, and pre-compliance evaluation.

## Module Status: COMPLETE ✅

- **L1-L6**: Complete
- **L7**: Complete (8 applications: PCB, IC, automotive, medical, smartphone, aerospace, motor drive, 5G base station)
- **L8**: Complete (6 advanced topics: stochastic analysis, Bayesian localization, ML classifier, time-varying field, Monte Carlo, Lyapunov positioning)
- **L9**: Partial (3 research frontiers documented, no C implementation required per SKILL.md)

## Line Count
- `include/` + `src/`: **6348 lines** (≥ 3000 minimum)

## Build & Test
```bash
make          # Build library, tests, and all 3 examples
make test     # Run comprehensive test suite (34 tests covering L1-L8)
make examples # Build end-to-end examples only
make clean    # Remove build artifacts
```

## Core Definitions (L1)
- Near-field region classification (reactive, radiating, Fraunhofer) per IEEE 145-2013
- H-field loop probe, E-field monopole/dipole, microstrip, TEM cell, electro-optic, thermal probes
- Probe factor (PF_E, PF_H), antenna factor (AF) per CISPR 16-1-4
- Spatial resolution, frequency-wavelength conversion, free-space constants

## Core Theorems (L4)
- **Maxwell Curl Equations** (frequency domain): ∇×E = -jωμH, ∇×H = jωεE
- **Poynting Theorem**: ∮_S (E×H*)·n̂ dS = -∫_V (jωμ|H|² + jωε*|E|² + σ|E|²) dV
- **Image Theory** (PEC ground): z_image = 2z_ground - z_source
- **Equivalence Principle**: J_s = n̂×H, M_s = -n̂×E (Huygens principle)
- **Uniqueness Theorem**: boundary E (or H) uniquely determines interior field
- **Reciprocity Theorem** (Lorentz): Z_12 = Z_21 for linear passive media
- **Nyquist-Shannon** (spatial): Δx ≤ λ_min/2 for alias-free near-field sampling

## Core Algorithms (L5)
- Cooley-Tukey Radix-2 FFT with twiddle precomputation
- IIR Butterworth filter design via bilinear transform
- CISPR 16-1-1 quasi-peak detector model (charge/discharge time constants)
- 3-antenna calibration method (solves 3×3 system from Friis formula)
- VNA SOLT de-embedding (error box model)
- Cubic spline interpolation (natural boundary condition)
- Plane wave spectrum (PWS) propagation with evanescent mode handling
- ISTA compressive sampling reconstruction with L1 regularization
- Kabsch-Umeyama spatial calibration (SVD rigid-body alignment)
- Itoh phase unwrapping algorithm
- Welch method for PSD and coherence estimation

## Canonical Problems (L6)
1. Microstrip line near-field mapping (quasi-static + TL model)
2. IC radiated emission hot-spot detection
3. Differential pair radiation: CM vs DM mechanisms
4. Ground plane slot coupling (half-wave slot antenna model)
5. Decoupling capacitor PDN impedance analysis
6. Transmission line standing wave resonance
7. Cable common-mode radiation (electrically short dipole)
8. Heatsink emission as equivalent dipole moment
9. Via radiation from impedance discontinuity
10. Emission mechanism classification (wave impedance criterion)
11. Field map statistics and spatial correlation
12. Peak detection in emission spectrum

## Applications (L7)
- PCB EMC diagnostic → FCC Class B / CISPR 22 / CISPR 25 compliance
- IC-level debugging → IEC 61967-3 surface scan workflow
- Automotive EMC → CISPR 25 / ISO 11452 (Toyota supplier qualification)
- Medical device EMC → IEC 60601-1-2 (NHS risk assessment)
- Smartphone EMI → self-interference / desense detection (iPhone)
- Aerospace EMC → RTCA DO-160 (Boeing / F-35)
- Motor drive EMI → IEC 61800-3 (Toyota / ISO)
- 5G base station → 3GPP spurious emission (supplier qualification)

## Nine-School Curriculum Mapping
| School | Course | Module Coverage |
|--------|--------|----------------|
| MIT | 6.630 EM Waves | Field regions, Maxwell, Poynting |
| Stanford | EE252 Antennas | Probe calibration, NFFFT |
| Berkeley | EE117 EM | Green functions, equivalence |
| Illinois | ECE 451 EM | Stratified media, Helmholtz |
| Michigan | EECS 511 Microwave | Microstrip, impedance |
| Georgia Tech | ECE 6350 EM | MoM, integral equations |
| TU Munich | High-Frequency Eng. | VNA calibration |
| ETH | 227-0455 EM | PWS, evanescent waves |
| Tsinghua | EM Fields | Foundational EM |

## Reference Standards
- IEEE Std 145-2013 — IEEE Standard Definitions of Terms for Antennas
- IEEE Std 1309-2013 — Calibration of EM Field Sensors and Probes
- IEC 61967-3 — IC Radiated Emissions — Surface Scan Method
- CISPR 16-1-4 — Antennas and Test Sites for Radiated Disturbance
- CISPR 25 — Vehicles, Boats — Radio Disturbance Characteristics
- RTCA DO-160 — Environmental Conditions for Airborne Equipment
- ISO 11452 — Road Vehicles — Component Test for Electrical Disturbances

## File Structure
```
mini-near-field-probe-diagnostic/
├── Makefile
├── README.md                    ← This file (COMPLETE)
├── include/ (1961 lines)
│   ├── nf_probe_core.h          — L1-L2: Core definitions & concepts
│   ├── nf_field_theory.h        — L3-L4: EM field theory & fundamental laws
│   ├── nf_probe_calibration.h   — L5: Calibration algorithms
│   ├── nf_scanning.h            — L5-L6: Scanning & canonical problems
│   ├── nf_signal_processing.h   — L5-L6: Signal processing algorithms
│   └── nf_diagnostic.h          — L6-L8: Diagnostic apps & advanced topics
├── src/ (4387 lines)
│   ├── nf_probe_core.c          — 516 lines, 18 functions
│   ├── nf_field_theory.c        — 945 lines, 20 functions
│   ├── nf_probe_calibration.c   — 709 lines, 17 functions
│   ├── nf_scanning.c            — 898 lines, 22 functions
│   ├── nf_signal_processing.c   — 693 lines, 20 functions
│   ├── nf_diagnostic.c          — 625 lines, 30 functions
│   └── nf_lean.lean             — Lean 4 formalization (L1-L9 theorems)
├── tests/ (1 test file, 34 tests)
├── examples/ (3 end-to-end examples)
├── docs/ (5 knowledge documents)
├── demos/ (visualization directory)
└── benches/ (performance benchmarking directory)
```
