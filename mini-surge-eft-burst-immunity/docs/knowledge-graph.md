# Knowledge Graph - mini-surge-eft-burst-immunity

## L1: Definitions (Complete)
- `surge_waveform_type_t` - 8 standard surge waveform types
- `surge_coupling_mode_t` - 5 coupling modes for surge injection
- `surge_source_impedance_t` - 6 standard generator impedances
- `surge_test_level_t` - 5 IEC 61000-4-5 severity levels
- `eft_test_level_t` - 5 IEC 61000-4-4 severity levels
- `protection_device_type_t` - 8 SPD technologies
- `protection_stage_t` - 5 cascaded protection stages
- `surge_waveform_params_t` - waveform parameter struct
- `eft_burst_params_t` - EFT burst characteristics struct
- `protection_device_params_t` - device ratings struct
- `multistage_protection_t` - multi-stage config struct
- `coupling_network_params_t` - coupling network struct
- `eft_filter_params_t` - EFT filter struct
- `surge_test_result_t` - test result struct
- `surge_error_t` - 11 error codes
- Physical constants: EFT standard parameters, surge tau constants

## L2: Core Concepts (Complete)
- MOV grain-boundary physics and V-I power law
- TVS avalanche breakdown and dynamic resistance
- GDT Townsend discharge, glow-to-arc transition
- TSS thyristor crowbar operation
- Cascaded protection stage ordering
- Spark-over vs clamping vs crowbar behaviors
- Adiabatic vs thermal-equilibrium pulse response
- Follow-on current in GDT applications
- Protection device capacitance effects on signals
- Standard device families (SMA/SMB/SMC/DO-201 packages)

## L3: Mathematical Structures (Complete)
- Double-exponential waveform: v(t) = Vp*k*(e^(-t/tau2) - e^(-t/tau1))
- Heidler lightning function with finite-slope at t=0
- Peak time derivation via dv/dt = 0
- Normalization factor ensuring exact peak amplitude
- Fourier transform of double-exponential
- Spectral energy density: S(f) = |V(f)|^2/(2*Z0)
- Ring wave: damped sinusoidal with rise envelope
- FWHM computation via binary search
- 10%-90% rise time per IEC 60060-1
- MOV V-I: I = k*V^alpha with alpha = 20-50

## L4: Fundamental Laws (Complete)
- Energy integral: E = int(v^2/R dt) closed form
- I^2t (Joule integral): critical for fuse selection
- Charge transfer: Q = int(i dt) = Ip*k*(tau2-tau1)
- Spectral corner frequency (-3dB)
- MOV non-linearity power law (alpha-based)
- GDT Paschen curve: V_spark = f(p*d)
- TVS junction temperature rise (adiabatic)
- Multi-stage energy conservation
- Thermal runaway stability criterion
- IEC waveform parameter constraints

## L5: Algorithms/Methods (Complete)
- Double-exponential waveform generation
- Heidler lightning current function
- Rise time / pulse width numerical computation
- Fourier spectrum magnitude
- Corner frequency binary search
- MOV clamping voltage prediction
- TVS clamping voltage model
- GDT arc voltage model
- TSS on-state voltage
- MOV selection algorithm (V_1mA matching)
- TVS selection algorithm (V_standoff, P_pp)
- GDT selection algorithm (V_spark, I_surge)
- Device validation (multi-check)
- Protection margin computation
- Multi-stage protection design
- Energy distribution across stages
- EFT filter design (LC, Pi, T, CM choke, ferrite)
- MOV lifetime estimation (Arrhenius-based)
- MOV sizing for energy absorption
- Thermal runaway stability check

## L6: Canonical Problems (Complete)
- Surge protector design (IEC 61000-4-5 Level 3, 230V AC) - ex1
- EFT immunity test simulation (IEC 61000-4-4) - ex2
- Three-stage protection coordination - ex3
- Coupling network design per IEC
- Decoupling network for auxiliary equipment
- Coupling clamp response for EFT
- Statistical burst amplitude variation
- Worst-case EFT stress analysis

## L7: Applications (Complete - 3 implementations)
- AC mains surge protection (230V, IEC Level 3)
- Industrial EFT testing (factory automation, motor drives)
- Automotive load dump protection (ISO 7637-2 reference)
- Telecom line surge protection (ITU-T K.44)
- Military vehicle surge protection (MIL-STD-1275)

## L8: Advanced Topics (Complete - 3/5 implemented)
- Statistical burst analysis with amplitude distribution
- Thermal runaway prevention modeling
- Surge generator stored energy matching
- (Documented only) SPICE-level device modeling for EMC simulation
- (Documented only) Multi-conductor transmission line surge propagation

## L9: Research Frontiers (Partial)
- GaN power device surge robustness
- AI/ML-based protection device selection optimization
- Wide-bandgap semiconductor TVS (SiC, GaN)
- Physics-based degradation models for MOV aging
- Probabilistic lightning risk assessment for protection coordination