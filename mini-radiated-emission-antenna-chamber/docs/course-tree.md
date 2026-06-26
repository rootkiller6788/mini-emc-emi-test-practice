# Course Tree — Radiated Emission Antenna Chamber

## Prerequisites (Dependency Graph)

```
L0: High School Physics
  → EM waves, dB notation, logarithms
  → C programming basics

L1-L2: Undergraduate EE Core
  → Circuit Analysis (impedance, VSWR, dBm)
  → EM Fields (Maxwell, plane waves, polarization)
  → Signal Processing (frequency domain, filtering)

L3: Intermediate EE
  → Antenna Theory (Balanis Ch 2, 4, 10, 11, 13)
  → Microwave Engineering (Friis, S-parameters)
  → Measurement Theory (uncertainty, calibration)

L4: Advanced Topics
  → EMC Theory (Paul 2006)
  → Standardization (CISPR, FCC, MIL)
  → Chamber Design (anechoic, absorber)

Current Module: Radiated Emission Antenna Chamber
  ↓ dependencies satisfied by:
  mini-electromagnetic-wave (EM fundamentals)
  mini-antenna-theory (antenna gain, patterns)
  mini-communication-principle (Friis, dB)
  mini-circuit-analysis (impedance, VSWR)

↓ Leads to:
  L8: Advanced EMC
    → Time-Domain EMI (TDEMI)
    → Reverberation Chamber
    → Full-Vehicle Testing
  L9: Research Frontiers
    → AI-Driven Emission Prediction
    → mmWave OTA Chamber Testing
    → Virtual Chamber Simulation
    → 6G EMC
    → Quantum EMI Sensing
```

## Dependency on Other mini-xxx modules
- **mini-electromagnetic-wave**: EM field equations, wave propagation
- **mini-antenna-theory**: (planned) Antenna gain, radiation patterns
- **mini-communication-principle**: Friis equation, dB conversions
- **mini-circuit-analysis**: Impedance matching, VSWR
- **mini-emc-signal-integrity**: EMC fundamentals, signal integrity
- **mini-digital-signal-process**: Frequency domain analysis, FFT

## Recommended Learning Path
1. Study EM wave basics → near_far_field.c
2. Learn antenna types → antenna_types.c
3. Understand antenna factor → antenna_factor.c
4. Compute field strength → field_strength.c
5. Study site attenuation → site_attenuation.c
6. Practice chamber qualification → chamber_qualify.c
7. Learn emission scanning → emission_scan.c
8. Apply standards → standard_limits.c
9. Run examples → examples/
10. Review formalization → radiated_emission_formal.lean