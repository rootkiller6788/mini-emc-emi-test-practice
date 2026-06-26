# mini-pre-compliance-low-cost-methods

Low-cost EMC/EMI pre-compliance testing methods -- near-field probes, LISN, TEM cells, spectrum analyzer settings, measurement uncertainty, and correlation to full compliance.

## Module Status: COMPLETE

- **L1 Definitions**: Complete (17 core data structures)
- **L2 Core Concepts**: Complete (10 concepts with full implementations)
- **L3 Mathematical Structures**: Complete (10 math structures)
- **L4 Fundamental Laws**: Complete (10 laws verified in C + 4 formalized in Lean)
- **L5 Algorithms**: Complete (14 algorithms)
- **L6 Canonical Problems**: Complete (6 EMC problems solved)
- **L7 Applications**: Complete (4 real-world applications)
- **L8 Advanced Topics**: Complete (Monte Carlo uncertainty, stochastic confidence)
- **L9 Research Frontiers**: Partial (3 topics documented)

## Code Metrics

| Metric | Value |
|--------|-------|
| Header files (.h) | 6 files, 1570 lines |
| Source files (.c) | 6 files, 2328 lines |
| Lean 4 formalization | 1 file, 254 lines |
| **include/ + src/ total** | **3898 lines** |
| Test coverage | 41 tests, all passing |
| Examples | 3 end-to-end examples |

## Core Theorems (L4)

| Theorem | Formula | Verification |
|---------|---------|-------------|
| Friis/Antenna Factor Duality | AF = 20*log10(f_MHz) - G_dBi - 29.79 | Passes at 1e-6 |
| Faraday's Law (H-probe) | V = 2*pi*f * N * mu0 * pi*r^2 * H | Verified |
| Thermal Noise (kTB) | MDS = -174 + 10*log10(RBW) + NF | Exact match |
| Fraunhofer Far-Field | R_ff = 2*D^2/lambda | Within 0.01 |
| CISPR QP Weighting | V_qp = Vp*(1-e^(-T/tc))/(1-e^(-T/td)) | Monotonicity verified |
| GUM Propagation | uc^2 = sum(ci*ui)^2 | RSS exact |
| CISPR Decision Rule | E_meas + U <= Limit -> Pass | 3-way verified |

## Build & Run

```bash
make          # Build tests + all examples
make test     # Run 41-test suite (all passing)
make examples # Build examples only
make clean    # Clean build artifacts

./examples/example_smps_prescan          # SMPS CISPR 32 conducted pre-scan
./examples/example_arduino_emi_check     # Arduino FCC Part 15 radiated check
./examples/example_probe_calibration     # DIY probe TEM cell calibration
```

## File Structure

```
mini-pre-compliance-low-cost-methods/
  README.md
  Makefile
  include/
    emi_precompliance_core.h          # Core types, conversions, antenna factor
    emi_probe_theory.h                # E/H near-field probe models
    emi_measurement_setup.h           # LISN, TEM cell, antenna, NSA
    emi_spectrum_analysis.h           # SA specs, detectors, scan automation
    emi_lowcost_methods.h             # DIY probes, current clamp, correlation
    emi_correlation_uncertainty.h     # Statistics, GUM, Ucispr, Monte Carlo
  src/
    emi_precompliance_core.c          # Limit lines, margin eval, scan config
    emi_probe_theory.c                # Elliptic integrals, mutual L/C, wave Z
    emi_measurement_setup.c           # LISN Z(f), TEM cell E/P, NSA calc
    emi_spectrum_analysis.c           # QP/RMS/AVG detectors, peak search
    emi_lowcost_methods.c             # DIY init, three-antenna, SE, correlation
    emi_correlation_uncertainty.c     # Stats, regression, Ucispr, Monte Carlo
    emi_precompliance_lean.lean       # Lean 4 formal proofs (4 theorems)
  tests/
    test_emi_precompliance.c          # 41 tests, all passing
  examples/
    example_smps_prescan.c            # SMPS CISPR 32 conducted pre-scan
    example_arduino_emi_check.c       # Arduino FCC Part 15 radiated check
    example_probe_calibration.c       # DIY H-field probe calibration
  docs/
    knowledge-graph.md                # L1-L9 knowledge coverage table
    coverage-report.md                # Coverage assessment (score 17/18)
    gap-report.md                     # Missing items and future work
    course-alignment.md               # 9-school course mapping
    course-tree.md                    # Prerequisite dependency tree
```

## References

- Paul, C.R., *Introduction to Electromagnetic Compatibility*, 2nd ed., Wiley, 2006.
- Ott, H.W., *Electromagnetic Compatibility Engineering*, Wiley, 2009.
- Williams, T., *EMC for Product Designers*, 5th ed., Newnes, 2017.
- CISPR 16-1-1, 16-1-2, 16-1-4, 16-2-3, 16-4-2 standards.
- ISO/IEC Guide 98-3 (GUM), Guide to the Expression of Uncertainty in Measurement.
- Balanis, C.A., *Antenna Theory: Analysis and Design*, 4th ed., Wiley, 2016.
- Wyatt, K., *Create Your Own EMC Troubleshooting Kit*, Interference Technology, 2013.

---
*Built to SKILL.md standard -- knowledge-first, code-as-carrier.*
