/-
Radiated Emission Antenna Chamber — Lean 4 Formalization

L1: Core definitions (AntennaFactor, FieldStrength, SiteAttenuation)
L3: Mathematical structures (dB conversions, Friis equation)
L4: Fundamental laws (Friis transmission, reciprocity, image theory)

Reference: Balanis (2016), CISPR 16-1-4:2019, Paul (2006)
-/

/-! ## L1: Core Type Definitions -/

/-- Frequency represented in Hz. Must be positive. -/
structure Frequency where
  hz : Float
  pos : hz > 0.0
  deriving Repr

/-- Antenna Factor in dB(1/m). Represents the conversion factor between
    incident electric field and terminal voltage. -/
structure AntennaFactor where
  db_per_m : Float
  deriving Repr

/-- Electric Field Strength in dB(uV/m). -/
structure FieldStrength where
  dbuv_per_m : Float
  deriving Repr

/-- Measurement Distance in meters. Must be positive. -/
structure MeasurementDistance where
  meters : Float
  pos : meters > 0.0
  deriving Repr

/-- Antenna Gain (linear, not dB). Must be positive. -/
structure AntennaGain where
  linear : Float
  pos : linear > 0.0
  deriving Repr

/-! ## L3: Physical Constants -/

/-- Speed of light in vacuum (m/s). -/
def speedOfLight : Float := 299792458.0

/-- Free-space impedance (ohm). η₀ = √(μ₀/ε₀) ≈ 376.73 Ω -/
def freeSpaceImpedance : Float := 376.730313668

/-! ## L3: Mathematical Conversions -/

/-- Convert dBm to dBuV for a given system impedance.
    V_dBuV = P_dBm + 90 + 10*log10(Z)
    Reference: Agilent AN 57-2 -/
def dbmToDBuV (dbm : Float) (impedance : Float) : Float :=
  dbm + 90.0 + 10.0 * Float.log10 impedance

/-- Convert dBuV/m to V/m.
    E_V/m = 10^(E_dBuV/m / 20) * 1e-6 -/
def dbuvmToVm (e_dbuvm : Float) : Float :=
  (Float.pow 10.0 (e_dbuvm / 20.0)) * 1e-6

/-- Convert V/m to dBuV/m.
    E_dBuV/m = 20*log10(E_V/m / 1e-6) -/
def vmToDBuvm (e_vm : Float) : Float :=
  20.0 * Float.log10 (e_vm / 1e-6)

/-! ## L4: Antenna Factor Theorem (Balanis 2016, eqn 2-106)

The Antenna Factor AF relates incident E-field to terminal voltage:
    AF_dB = 20*log10(f_MHz) - 10*log10(G_linear) - 29.78  [dB(1/m)]

where f_MHz = frequency in MHz, G_linear = antenna gain (linear).
-/

/-- Compute Antenna Factor from frequency and gain.
    This is the fundamental AF equation from Balanis (2016) sec 2.16. -/
def antennaFactorFromGain (freq : Frequency) (gain : AntennaGain) : Float :=
  let f_mhz := freq.hz / 1e6
  20.0 * Float.log10 f_mhz - 10.0 * Float.log10 gain.linear - 29.78

/-! ## L4: Friis Transmission Equation

The Friis equation relates received power to transmitted power:
    P_rx = P_tx * G_tx * G_rx * (λ / (4πd))^2

In free-space path loss form:
    FSPL_dB = 20*log10(4πd/λ) = 20*log10(d) + 20*log10(f_MHz) + 32.45

Reference: Friis (1946)
-/

/-- Free-Space Path Loss (FSPL) in dB.
    FSPL = 20*log10(4*pi*d/λ) -/
def freeSpacePathLoss (dist : MeasurementDistance) (freq : Frequency) : Float :=
  let lambda := speedOfLight / freq.hz
  let numerator := 4.0 * Float.pi * dist.meters
  20.0 * Float.log10 (numerator / lambda)

/-- Compute received power from Friis equation (linear form).
    P_rx = P_tx * G_tx * G_rx * (λ/(4πd))^2 -/
def friisReceivedPower (p_tx : Float) (g_tx g_rx : AntennaGain)
    (dist : MeasurementDistance) (freq : Frequency) : Float :=
  let lambda := speedOfLight / freq.hz
  let factor := lambda / (4.0 * Float.pi * dist.meters)
  p_tx * g_tx.linear * g_rx.linear * (factor * factor)

/-- Compute E-field from transmitter power (far-field Friis).
    E = √(30 * P_tx * G_tx) / d    [V/m]
    Reference: Balanis (2016) eqn 2-117 -/
def eFieldFromPower (p_tx : Float) (g_tx : AntennaGain)
    (dist : MeasurementDistance) : Float :=
  Float.sqrt (30.0 * p_tx * g_tx.linear) / dist.meters

/-! ## L4: Wavelength Computation -/

/-- Compute wavelength from frequency. λ = c / f -/
def wavelength (freq : Frequency) : Float :=
  speedOfLight / freq.hz

/-- Compute wavenumber. k = 2π/λ = 2πf/c -/
def wavenumber (freq : Frequency) : Float :=
  2.0 * Float.pi * freq.hz / speedOfLight

/-! ## L4: Field Strength Measurement Equation

The fundamental measurement equation (CISPR 16-2-3 eqn 1):
    E_dBuV/m = V_r_dBuV + AF_dB + CableLoss_dB - PreampGain_dB
-/

/-- Compute field strength from receiver reading and correction factors. -/
def fieldStrengthFromReceiver (v_rdbuv : Float) (af_db : Float)
    (cableLoss_db : Float) (preampGain_db : Float) : Float :=
  v_rdbuv + af_db + cableLoss_db - preampGain_db

/-! ## L4: Near-Field / Far-Field Boundary

Fraunhofer criterion for far-field:
    R_ff = 2 * D^2 / λ

Reactive near-field boundary:
    R_nf = 0.62 * √(D^3 / λ)

Reference: IEEE Std 145-2013, Balanis (2016) sec 2.2.3
-/

/-- Maximum antenna dimension (meters). Must be positive. -/
structure AntennaDimension where
  meters : Float
  pos : meters > 0.0
  deriving Repr

/-- Fraunhofer far-field distance. R_ff = 2*D^2/λ -/
def fraunhoferDistance (dim : AntennaDimension) (freq : Frequency) : Float :=
  2.0 * (dim.meters * dim.meters) / (wavelength freq)

/-- Reactive near-field boundary. R_nf = 0.62*√(D^3/λ) -/
def reactiveNearFieldDistance (dim : AntennaDimension) (freq : Frequency) : Float :=
  0.62 * Float.sqrt ((dim.meters * dim.meters * dim.meters) / (wavelength freq))

/-! ## L4: Ground Plane Reflection (Image Theory)

For a perfect conductor ground plane:
    Γ_horizontal = -1    (E tangential = 0 at surface)
    Γ_vertical   = +1    (at grazing incidence)

Two-ray model:
    r_direct    = √(d^2 + (h_tx - h_rx)^2)
    r_reflected = √(d^2 + (h_tx + h_rx)^2)
    Δr = r_reflected - r_direct
    E_total = E_direct * (1 + Γ * exp(-j*k*Δr))

Reference: ANSI C63.4 Annex A
-/

/-- Two-ray path length difference for OATS/SAC ground plane. -/
def twoRayPathDiff (d : MeasurementDistance) (h_tx h_rx : Float) : Float :=
  let rDirect := Float.sqrt (d.meters * d.meters + (h_tx - h_rx) * (h_tx - h_rx))
  let rReflected := Float.sqrt (d.meters * d.meters + (h_tx + h_rx) * (h_tx + h_rx))
  rReflected - rDirect

/-- Field enhancement factor from ground reflection.
    |1 + Γ * exp(-j*k*Δr)|^2 = 1 + Γ^2 + 2*Γ*cos(k*Δr) -/
def groundReflectionFactor (gamma : Float) (k_delta_r : Float) : Float :=
  let cosTerm := Float.cos k_delta_r
  1.0 + gamma * gamma + 2.0 * gamma * cosTerm

/-! ## L3: NSA (Normalized Site Attenuation)

NSA = -20*log10(|V_site / V_direct|)

Theoretical NSA over perfect ground plane accounts for the
two-ray interference pattern.

Reference: ANSI C63.4 Annex A, CISPR 16-1-4 Annex D
-/

/-- Compute theoretical NSA for horizontal polarization over perfect ground.
    Uses the two-ray model with Γ = -1. -/
def nsaTheoretical (dist : MeasurementDistance) (h_tx h_rx : Float)
    (freq : Frequency) : Float :=
  let k := wavenumber freq
  let delta_r := twoRayPathDiff dist h_tx h_rx
  let fieldFactor := groundReflectionFactor (-1.0) (k * delta_r)
  let rDirect := Float.sqrt (dist.meters * dist.meters + (h_tx - h_rx) * (h_tx - h_rx))
  -20.0 * Float.log10 (Float.sqrt fieldFactor)
   + 20.0 * Float.log10 rDirect
   + 20.0 * Float.log10 (4.0 * Float.pi / (wavelength freq))

/-! ## L3: Distance Extrapolation

Far-field: E2 = E1 * (d1/d2)
In dB: E2_dB = E1_dB + 20*log10(d1/d2)
-/

/-- Extrapolate field strength to a different distance (far-field 1/r rule). -/
def extrapolateFarField (e1_dbuvm : Float) (d1 d2 : Float) : Float :=
  e1_dbuvm + 20.0 * Float.log10 (d1 / d2)

/-! ## L3: Poynting Vector

In far-field: |S| = |E|^2 / η₀   [W/m^2]
-/

/-- Compute Poynting vector magnitude from E-field in far-field. -/
def poyntingFromEField (e_vm : Float) : Float :=
  e_vm * e_vm / freeSpaceImpedance

/-! ## L1: Compliance Margin

Margin = E_measured_dBuV/m - Limit_dBuV/m
Positive margin = FAIL (over limit)
Negative margin = PASS (under limit)
-/

/-- Compute margin against limit. -/
def complianceMargin (e_dbuvm : Float) (limit_dbuvm : Float) : Float :=
  e_dbuvm - limit_dbuvm

/-- Determine pass/fail.
    Pass if margin <= 0 (emission at or below limit).
    Fail if margin > 0 (emission exceeds limit). -/
def isPass (margin : Float) : Bool :=
  margin <= 0.0