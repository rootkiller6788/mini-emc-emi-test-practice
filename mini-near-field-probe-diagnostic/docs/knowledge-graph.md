# Knowledge Graph — Near-Field Probe Diagnostic

## L1: Definitions (Complete)
- Near-field region (reactive/radiating), far-field (Fraunhofer) — IEEE 145-2013
- H-field loop probe, E-field monopole/dipole probe — IEC 61967-3
- Probe factor (PF_E, PF_H), antenna factor (AF) — CISPR 16-1-4
- Spatial resolution, sensitivity, frequency range
- Free-space impedance η₀ = 377Ω, wavelength λ = c/f
- Measurement coordinate system, scan grid, field sample

## L2: Core Concepts (Complete)
- Reciprocity theorem (Lorentz) for probe calibration
- Probe perturbation analysis (Kanda 1993)
- Reactive near-field: energy storage dominant, E-H phase quadrature
- Radiating near-field (Fresnel): field pattern evolves with distance
- Far-field (Fraunhofer): field pattern stable, |E/H| = η₀
- Wave impedance: electric source (high Z) vs magnetic source (low Z)
- C63.2 antenna factor derivation from Friis formula
- CISPR 16-1-1 measurement receiver parameters

## L3: Mathematical Structures (Complete)
- Complex vector algebra (add, dot, cross) for field computation
- Plane wave spectrum (PWS) decomposition — angular spectrum method
- Dyadic Green's function for free space — EFIE kernel
- Equivalent source method (electric + magnetic dipole moments)
- Spherical wave expansion (TE/TM modes) — NFFF transformation
- Evanescent wave propagation — near-field super-resolution basis
- Poynting vector (complex) — active/reactive power separation
- 3×3 tensor for anisotropic media

## L4: Fundamental Laws (Complete)
- Maxwell curl equations (frequency domain): ∇×E = -jωμH, ∇×H = jωεE
- Boundary conditions: PEC, PMC, impedance, radiation, periodic
- Poynting theorem — electromagnetic energy conservation
- Image theory (PEC ground plane) — horizontal/vertical dipole images
- Uniqueness theorem — boundary field determines interior field
- Equivalence principle (Huygens): J_s = n×H, M_s = -n×E
- Helmholtz equation: ∇²u + k²u = 0 — finite difference solver
- Stratified media input impedance — transmission line analogy

## L5: Algorithms/Methods (Complete)
- TEM cell field computation for probe calibration
- Microstrip effective permittivity (Hammerstad-Jensen)
- 3-antenna calibration method — solve 3×3 linear system
- VNA de-embedding: SOLT and TRL calibration
- Raster/serpentine scan path generation
- Spatial Nyquist criterion checker
- Adaptive scan grid refinement (gradient-based)
- Compressive sampling reconstruction (ISTA with L1 regularization)
- Cooley-Tukey Radix-2 FFT (DIT) with twiddle factor precomputation
- Real FFT using Hermitian symmetry packing
- Window functions: Hann, Hamming, Blackman, Kaiser, Flat-top
- IIR Butterworth filter design via bilinear transform
- Time-domain gating for multipath removal
- Cross-correlation analysis for source localization
- Phase unwrapping (Itoh algorithm)
- CISPR 16-1-1 quasi-peak detector model
- Noise figure / MDS / dynamic range analysis
- Field statistics (mean, median, std, skewness, kurtosis, P95, P99)
- Magnitude-squared coherence (Welch method)
- PSD estimation (Welch periodogram)
- Moving average and median filters
- NFFFT — plane wave spectrum to far-field mapping
- S-parameter to impedance conversion

## L6: Canonical Problems (Complete)
- Microstrip line near-field mapping (quasi-static + TL model)
- IC radiated emission hot-spot detection
- Differential pair radiation: DM cancellation vs CM addition
- Ground plane slot coupling analysis (half-wave resonance)
- Decoupling capacitor PDN impedance analysis (SRF, ESR, ESL)
- Transmission line resonance finder (standing wave pattern)
- Cable common-mode radiation (electrically short dipole model)
- Heatsink emission as equivalent dipole
- Via radiation from impedance discontinuity
- Emission mechanism classification (DM/CM, high-Z/low-Z)
- Field map statistics and spatial correlation
- Peak detection in emission spectrum

## L7: Applications (Complete — 8 applications)
1. PCB EMC diagnostic (FCC Class B / CISPR 22/25)
2. IC-level debugging (IEC 61967-3 workflow)
3. Automotive EMC (CISPR 25 / ISO 11452 — Toyota/ISO)
4. Medical device EMC (IEC 60601-1-2 — NHS risk assessment)
5. Smartphone EMI debugging (self-interference/desense detection)
6. Aerospace EMC (RTCA DO-160 — Boeing)
7. Industrial motor drive EMI (IEC 61800-3 — Toyota)
8. 5G base station EMC (3GPP spurious emission)

## L8: Advanced Topics (Complete — 5 topics)
1. Stochastic near-field analysis (stationarity test, temporal correlation)
2. Bayesian source localization (posterior distribution, confidence ellipse)
3. Machine learning EMI classifier (KNN, SVM — adaptive policy for EMC)
4. Time-varying field analysis (Hilbert envelope, instantaneous frequency)
5. Monte Carlo uncertainty propagation (GUM supplement 1)
6. Adaptive probe positioning via Lyapunov function gradient ascent

## L9: Research Frontiers (Partial — documented)
- Reconfigurable Intelligent Surfaces (RIS) phase quantization
- Quantum sensing standard quantum limit for near-field
- Semantic communication efficiency metric
- 6G antenna-in-package diagnostic concepts
