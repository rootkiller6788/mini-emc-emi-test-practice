# Course Tree - mini-surge-eft-burst-immunity

## Prerequisite Dependency Graph

```
                      [EMC Basics]
                           |
            +--------------+--------------+
            |              |              |
      [Circuit Theory] [EM Waves] [Semiconductor]
            |              |        Physics]
            +--------------+--------------+
                           |
               [Surge/EFT Immunity]
                           |
            +--------------+--------------+
            |              |              |
        [Waveform       [Protection    [Coordination
         Generation]     Devices]       Networks]
            |              |              |
            +--------------+--------------+
                           |
            +--------------+--------------+
            |              |              |
        [Thermal       [Statistical   [EMC Test
         Analysis]      Analysis]      Methods]
```

## Knowledge Dependencies

### This module DEPENDS ON:

1. **Circuit Theory** (RLC transients, time constants, Laplace domain)
   - Required for: waveform modeling, coupling network analysis
   - Source: mini-circuit-analysis (module 1)

2. **Electromagnetic Waves** (coupling, transmission lines, impedance)
   - Required for: coupling clamp physics, surge propagation
   - Source: mini-electromagnetic-wave (module 7)

3. **Semiconductor Device Physics** (p-n junction, avalanche, ZnO)
   - Required for: MOV/TVS/GDT device physics
   - Source: mini-analog-electronics (module 2)

4. **EMC Basics** (standards framework, test methodology)
   - Required for: IEC standards interpretation
   - Source: mini-emc-signal-integrity (module 18)

5. **Digital Signal Processing** (Fourier analysis, spectrum)
   - Required for: surge spectrum analysis
   - Source: mini-digital-signal-process (module 6)

### Modules that DEPEND ON this module:

1. **EMC Compliance Testing** (pre-compliance and full compliance)
   - Uses: surge/EFT test configurations
   - Module: mini-emc-compliance (18. mini-emc-signal-integrity)

2. **Power Supply Design** (surge protection for AC-DC converters)
   - Uses: MOV selection, multi-stage coordination
   - Module: mini-power-supply-design-practice (21)

3. **Electronic Measurement** (surge generator calibration)
   - Uses: waveform parameter measurement
   - Module: mini-electronic-measurement-instr (22)

4. **Motor Drive Power Practice** (inductive load EFT)
   - Uses: EFT burst analysis, EMI filter design
   - Module: mini-motor-drive-power-practice (29)

## Learning Path

```
Week 1: L1+L2 - Definitions + Core Concepts
  - Surge waveform types, test levels, protection device types
  - MOV/TVS/GDT/TSS operating principles
  - Reading: IEC 61000-4-4/-5 overview, Paul Ch. 8

Week 2: L3+L4 - Math + Fundamental Laws
  - Double-exponential waveform mathematics
  - Energy integral, I^2t, charge transfer
  - Reading: Heidler (1985), Standler Ch. 3

Week 3: L5 - Algorithms
  - Device selection algorithms
  - Thermal analysis and lifetime estimation
  - Lab: Characterize MOV V-I curve

Week 4: L6 - Canonical Problems
  - Multi-stage protection design
  - EFT filter design
  - Coupling/decoupling network synthesis
  - Lab: Design and test surge protector for 230V AC

Week 5: L7+L8 - Applications + Advanced
  - Industrial EFT test simulation
  - Automotive surge protection
  - Statistical burst analysis
  - Final project: Complete protection scheme for given requirements
```