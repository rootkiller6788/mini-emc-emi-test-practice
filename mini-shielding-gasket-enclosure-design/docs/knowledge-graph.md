# Knowledge Graph — mini-shielding-gasket-enclosure-design

## L1: Definitions (Complete)
- Shielding Effectiveness (SE): SE_E, SE_H, SE_P in dB
- Schelkunoff SE decomposition: A + R + M = SE_total
- Skin depth: δ = 1/√(πfμσ) [m]
- Transfer impedance: Z_T = V_drop / (I_src·l) [Ω·m]
- Wave impedance in conductors: |η| = √(2πfμ/σ) [Ω]
- Gasket types: 10 types (elastomer, wire mesh, spring finger, etc.)
- Shielding materials: 11 entries (Cu, Al, Steel, Mu-metal, etc.)
- Enclosure geometries: rectangular, cylindrical, spherical
- Aperture types: slot, circular, rectangular, honeycomb
- Seam treatments: none, gasket, overlap, tape, weld, spring contact
- Cavity resonance: TE/TM_mnp modes
- Near-field / far-field boundary: r = λ/(2π)
- Gasket compression states: linear elastic, plateau, densification

## L2: Core Concepts (Complete)
- Electromagnetic shielding theory (Schelkunoff impedance approach)
- Plane-wave reflection/absorption mechanisms
- Near-field shielding (E-field enhanced, H-field reduced reflection)
- Multiple reflection correction for thin shields
- Conductive gasket compression mechanics
- Transfer impedance vs. contact resistance
- Enclosure cavity resonance fundamentals
- Aperture coupling (slot antenna model)
- Waveguide-below-cutoff attenuation
- Galvanic corrosion in shielding joints
- Seam leakage as slot antenna
- Temperature effects on conductivity and permeability
- Curie temperature limits for magnetic shields
- Multi-layer shield optimization

## L3: Mathematical Structures (Complete)
- Complex wave propagation in lossy media
- Transmission line analogy for plane-wave shielding
- Impedance transformation at shield interfaces
- dB arithmetic for cascaded attenuation
- Piecewise nonlinear stress-strain for gaskets
- Arrhenius thermal aging model
- Wohler fatigue curve model
- Binary search for compression optimization
- Logarithmic frequency sweep analysis
- Cavity eigenmode equation

## L4: Fundamental Laws (Complete)
- Schelkunoff SE equation: SE = A + R + M (1943)
- Schelkunoff absorption law: A = 8.686·t/δ [dB]
- Schelkunoff reflection laws (plane wave, near E, near H)
- Bethe small aperture coupling theory (1944)
- Waveguide cutoff principle: f_c = c₀/(1.706·d)
- Skin depth formula: δ = 1/√(πfμσ)
- Faraday cage principle
- Kaden multi-layer shielding theory (1959)
- Maxwell boundary conditions at conductor interface

## L5: Algorithms/Methods (Complete)
- Single-layer SE computation
- Multi-layer SE with gap resonance correction
- Gasket compression piecewise model (3-region)
- Gasket transfer impedance frequency model
- Cavity mode enumeration and sorting
- Aperture leakage estimation (Bethe + waveguide)
- Honeycomb vent SE computation
- Gasket ranking by multi-criteria decision analysis
- Frequency sweep SE analysis
- Minimum thickness optimization (iterative)
- Service life estimation (Arrhenius + fatigue)
- Galvanic compatibility checking
- Screw spacing rule check (λ/20)
- Closure force budget calculation
- IP rating compliance check
- Perforated panel SE estimation
- Faraday cage low-frequency H-field SE
- PCB shield can via spacing check
- Temperature-dependent material properties
- Material recommendation by frequency band

## L6: Canonical Problems (Complete)
- Compute SE for rectangular enclosure with apertures
- Gasket selection for specific attenuation target
- Multi-layer shield SE optimization
- Cavity resonance prediction for enclosure
- Slot resonance degradation analysis
- Galvanic compatibility assessment
- Thermal-EMC co-design tradeoff

## L7: Applications (Partial - 4 implemented)
- Automotive ECU enclosure (CISPR 25)
- Medical device enclosure (IEC 60601-1-2)
- 5G NR base station enclosure (3GPP TS 38.104)
- Aerospace avionics (DO-160G)
- Consumer electronics (FCC Part 15)
- Industrial motor drive (IEC 61800-3)
- Industry requirements database (10 sectors)
- MIL-STD-285 / IEEE-299 measurement methods

## L8: Advanced Topics (Partial - 3 implemented)
- Thermal-EMC co-design optimization
- Multi-layer shield with magnetic+conductive layers
- Service life prediction combining fatigue + thermal aging
- Multi-criteria gasket selection

## L9: Research Frontiers (Partial)
- Formal verification of SE design rules (Lean 4)
- Material-property formalization for verification
- Future: conductive composite shielding optimization
- Future: metamaterial-based shielding surfaces
