# Knowledge Graph — Conducted Emission LISN Testing

## L1: Definitions (Complete)
- LISN / AMN (Artificial Mains Network)
- V-network, Delta network, T-network topologies
- 50Ω / 50μH standard LISN
- Voltage Division Factor (VDF)
- Insertion Loss (IL)
- Common-Mode / Differential-Mode noise
- CISPR 16-1-2 impedance specification
- Detector types (Peak, QP, AVG, RMS)
- Conducted emission frequency bands (A/B/C/D)
- Measurement uncertainty per CISPR 16-4-2

## L2: Core Concepts (Complete)
- LISN provides defined RF impedance to EUT
- Isolation of EUT from mains at RF
- Coupling path to measurement receiver
- CM vs DM noise propagation
- Ground plane effects
- LISN impedance tolerance (±20%)
- Measurement setup with ground reference plane
- LCL (Longitudinal Conversion Loss)

## L3: Mathematical Structures (Complete)
- Complex impedance: Z = R + jX
- Parallel RL network: Z = R || jwL
- s-domain transfer functions: H(s) = Vout/Vin
- S-parameters for 2-port LISN characterization
- ABCD transmission matrices
- Pole-zero analysis
- Fourier transform relationship (time/frequency)
- dB/dBuV/dBm conversion mathematics

## L4: Fundamental Laws (Complete)
- Kirchhoff's Current Law (KCL) in LISN network
- Kirchhoff's Voltage Law (KVL) in conducted paths
- Ohm's Law for complex impedances
- Thevenin/Norton equivalent circuits for LISN
- Maximum power transfer theorem (impedance matching)
- Shannon-Hartley theorem (SNR for measurement)
- CISPR 16-1-2 regulatory requirements

## L5: Algorithms/Methods (Complete)
- Quasi-Peak detector simulation (Euler ODE integration)
- Log-frequency sweep generation
- Limit line log-linear interpolation
- CM/DM mathematical decomposition
- RF combiner method for mode separation
- Measurement uncertainty budget aggregation
- Ambient noise correction
- Monte Carlo tolerance analysis
- Bayesian threshold adjustment
- Impedance sweep computation

## L6: Canonical Problems (Complete)
- CISPR 22/32 Class A/B compliance testing
- CISPR 25 automotive CE testing (Class 1-5)
- MIL-STD-461 CE102 military testing
- FCC Part 15 Class B verification
- DO-160 aircraft conducted emissions
- LISN impedance calibration verification
- CM/DM filter attenuation requirement calculation

## L7: Applications (Complete — 15+)
- Automotive: Toyota, Tesla Cybertruck 48V, Detroit OEM
- Aerospace: Boeing 787, SpaceX Starship, Apollo LM, Lunar Rover
- Medical: NHS, IEC 60601-1-2 life-support equipment
- Industrial: ISO 7637 pulses, smart grid inverter, maglev
- Consumer: iPhone USB-C, Bass amplifier, Easter egg tracker
- Infrastructure: Katy Freeway, prison security, flood sensor
- Wildlife: Mammal tracking collar (GPS/VHF telemetry)
- Robotics: Fukushima robot, Mars rover
- Production: Supplier EMC audit, Toyota EOL test

## L8: Advanced Topics (Partial — 8/10)
- Lyapunov exponent for EMC stability
- Markov blanket for coupling path simplification
- Bayesian decision theory for adaptive testing
- Monte Carlo impedance tolerance propagation
- Fuzzy logic compliance assessment
- Game of Life coupling path optimization
- BZ oscillator / Oregonator nonlinear EMC models
- Category-theoretic filter composition
- (Missing: agent-based EMC simulation, balanced network synthesis)

## L9: Research Frontiers (Partial)
- 6G RIS (Reconfigurable Intelligent Surfaces) for adaptive EMC
- Quantum EMC (quantum-limited measurements)
- Semantic communication for EMC
- Terahertz CMOS LISN design
- AI/ML-driven adaptive EMC test sequencing
