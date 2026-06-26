# Prerequisite Dependency Tree — Near-Field Probe Diagnostic

## Dependency Graph
```
L1: Definitions
 ├─► L2: Core Concepts (reciprocity, perturbation, wave impedance)
 │    ├─► L3: Math Structures (PWS, Green, spherical expansion)
 │    │    ├─► L4: Fundamental Laws (Maxwell, Poynting, image)
 │    │    │    ├─► L5: Algorithms (FFT, calibration, filters)
 │    │    │    │    ├─► L6: Canonical Problems (microstrip, IC, slot)
 │    │    │    │    │    ├─► L7: Applications (PCB, IC, auto, medical)
 │    │    │    │    │    │    └─► L8: Advanced (stochastic, Bayesian, ML)
 │    │    │    │    │    │         └─► L9: Research (RIS, quantum, semantic)
```

## External Dependencies
- Vector calculus (gradient, divergence, curl)
- Complex analysis (phasors, complex impedance)
- Fourier analysis (FFT, spectrum, convolution)
- Linear algebra (matrix operations, SVD for calibration)
- Probability theory (stochastic processes, Bayesian inference)
- Electromagnetic theory (Maxwell equations, boundary conditions)
- Transmission line theory (impedance matching, S-parameters)
- Signal processing (filtering, windowing, correlation)

## Internal Dependencies Within This Module
- nf_probe_core → nf_field_theory → nf_probe_calibration
- nf_probe_core → nf_scanning → nf_diagnostic
- nf_field_theory → nf_signal_processing
- nf_signal_processing → nf_diagnostic
