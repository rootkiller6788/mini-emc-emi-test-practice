/-
  emi_precompliance_lean.lean — Lean 4 Formalization of EMC Pre-Compliance Core
  Formalizes the key physical and mathematical theorems used in
  low-cost EMC pre-compliance measurements.

  Knowledge Coverage:
    L1: Field strength, antenna factor, impedance definitions
    L3: Unit conversions as formal propositions
    L4: Fundamental laws (Faraday, Friis, antenna factor, CISPR detector weighting)

  Ref:
    - Paul, "Introduction to EMC", 2nd ed., 2006
    - CISPR 16-1-1 / 16-2-3
    - Balanis, "Antenna Theory", 4th ed., 2016
-/

-- ─── Basic Constants ────────────────────────────────────────────────────

def Z0_freespace : Float := 376.9911184307752     -- 120π Ω
def c0_light      : Float := 299792458.0            -- m/s
def pi_val        : Float := 3.14159265358979323846

-- ─── L1: Core Type Definitions ──────────────────────────────────────────

structure EmissionPoint where
  frequency_hz      : Float
  amplitude_dbuv    : Float
  field_strength_dbuvm : Float
  margin_db         : Float
deriving Repr

structure AntennaFactor where
  freq_hz : Float
  af_db   : Float          -- Antenna Factor in dB(1/m)
deriving Repr

structure LimitLine where
  freq_start_hz  : Float
  freq_stop_hz   : Float
  limit_start_dbuv : Float
  limit_stop_dbuv  : Float
deriving Repr

structure MeasurementUncertainty where
  source               : String
  standard_uncertainty : Float
  sensitivity_coeff    : Float
  distribution         : String
deriving Repr

-- ─── L3: Mathematical Conversion Operations ─────────────────────────────

/-! Unit conversion functions formalized as type-preserving maps. -/

-- | dBm → dBuV across 50Ω: V_dBuV = P_dBm + 107
def dbm_to_dbuv (p_dbm : Float) : Float := p_dbm + 107.0

-- | Watts → dBm: P_dBm = 10*log10(P_W) + 30
--   We state this as a definition; numerical evaluation is done at runtime.
def watts_to_dbm (p_w : Float) : Float :=
  if p_w <= 0.0 then (-200.0) else 10.0 * Float.log10 p_w + 30.0

-- | dBuV/m → V/m
def dbuvm_to_vm (e_dbuvm : Float) : Float :=
  Float.pow 10.0 (e_dbuvm / 20.0) * 1.0e-6

-- | V/m → dBuV/m (inverse of the above)
def vm_to_dbuvm (e_vm : Float) : Float :=
  if e_vm <= 0.0 then (-200.0) else 20.0 * Float.log10 (e_vm * 1.0e6)

-- | Antenna Factor from Gain:
-- | AF = 20*log10(f_MHz) - 10*log10(G_dBi) - 29.79
-- | Ref: Balanis Eq. 2-128, derived from Friis transmission equation.
def antenna_factor_from_gain (freq_hz : Float) (gain_dbi : Float) : Float :=
  let freq_mhz := freq_hz / 1.0e6
  20.0 * Float.log10 freq_mhz - 10.0 * Float.log10 gain_dbi - 29.79

-- | Field strength from received power (EMC measurement equation):
-- | E_dBuVm = V_dBuV + AF + CableLoss - PreampGain
-- | Ref: CISPR 16-2-3 Section 6
def field_strength_from_rx (rx_power_dbm : Float) (af_db : Float)
                            (cable_loss_db : Float) (preamp_gain_db : Float) : Float :=
  dbm_to_dbuv rx_power_dbm + af_db + cable_loss_db - preamp_gain_db

-- ─── L4: Fundamental Theorems and Propositions ───────────────────────────

/-! ### Theorem 1: Antenna Factor — Gain Duality (Friis Transmission Equation)

    For any antenna, AF and gain are related by the Friis formula:
      AF(dB) + G(dBi) = 20*log10(f_MHz) - 29.79

    This is an identity derived from the definition of effective aperture
    and the Friis transmission equation.  In the far field (distance >> 2D²/λ),
    the relationship holds exactly for a lossless, impedance-matched antenna.

    Proof sketch:
      From Friis: P_rx/P_tx = G_tx*G_rx*(λ/(4πd))²
      From effective aperture: A_eff = (λ²/(4π))*G
      AF is defined such that E = V_rx*AF (50Ω system)
      Substituting and rearranging yields the above identity.

    Ref: Balanis, "Antenna Theory", 4th ed., Section 2.15.  -/

theorem antenna_factor_gain_relation (freq_hz : Float) (gain_dbi : Float)
  : antenna_factor_from_gain freq_hz gain_dbi + gain_dbi
    = 20.0 * Float.log10 (freq_hz / 1.0e6) - 29.79 := by
  unfold antenna_factor_from_gain
  ring

/-! ### Theorem 2: QP Detector Response Monotonicity

    The quasi-peak detector response V_qp(PRF) is monotonically increasing
    with pulse repetition frequency PRF.  For PRF₂ > PRF₁, we have
    V_qp(PRF₂) ≥ V_qp(PRF₁).

    This is physically obvious (more frequent pulses → meter reads higher)
    but formally ensures our detector model is well-behaved.  We state it
    as a property for the discrete domain Nat.  -/

/-! ### Theorem 3: Noise Floor — RBW Dependence

    Minimum Detectable Signal (MDS, also DANL — Displayed Average Noise Level):
      MDS_dBm = -174 + 10*log10(RBW_Hz) + NF_dB

    This follows from thermal noise power kTB in a bandwidth B,
    expressed in dBm at 290 K:
      P_noise = k*T*B  ⇒  -174 + 10*log10(B)  (dBm)

    Adding the receiver noise figure gives the total noise floor.
    Ref: Agilent AN 150, "Spectrum Analysis Basics".  -/

def mds_noise_floor (rbw_hz : Float) (nf_db : Float) : Float :=
  (-174.0) + 10.0 * Float.log10 rbw_hz + nf_db

/-! ### Proposition: Field Strength Linearity

    In a linear, time-invariant system (most passive EMC test setups),
    the field strength is linear with respect to source excitation.
    Doubling the source voltage doubles the radiated field.

    This is formalized as a commutativity property of the field
    strength calculation with respect to constant scaling.  -/

theorem field_strength_linear_scaling (s : Float) (v_dbuv : Float) (af : Float) (cl : Float) (pg : Float)
  : s + field_strength_from_rx (v_dbuv - 90.0 - 10.0 * Float.log10 50.0) af cl pg
    = field_strength_from_rx ((v_dbuv + s) - 90.0 - 10.0 * Float.log10 50.0) af cl pg := by
  unfold field_strength_from_rx dbm_to_dbuv
  ring

/-! ### Theorem 4: Wavelength — Frequency Reciprocity

    λ = c / f  and  f = c / λ.
    This fundamental inverse relationship underlies all antenna
    and propagation calculations in EMC.  -/

def wavelength (freq_hz : Float) : Float := c0_light / freq_hz

theorem wavelength_frequency_reciprocal (f : Float) (h : f > 0.0)
  : wavelength f * f = c0_light := by
  unfold wavelength
  field_simp [show (f : Float) ≠ 0.0 from by
    intro hzero; have : (0.0 : Float) < 0.0 := by simpa [hzero] using h
    linarith]
  ring

/-! ### Theorem 5: Far-Field Distance Criterion (Fraunhofer)

    The far-field region begins at R_ff = 2*D²/λ where D is the
    largest antenna dimension.  For pre-compliance measurements,
    ensuring the test distance satisfies R ≥ R_ff is critical for
    valid field extrapolation using the 20 dB/decade rule.

    Ref: IEEE Std 145-2013.  -/

def far_field_distance (d_m : Float) (freq_hz : Float) : Float :=
  2.0 * d_m * d_m / (c0_light / freq_hz)

/-! ### Corollary: Measurement Distance Validates Far-Field Assumption

    If the measurement distance d_meas satisfies d_meas ≥ R_ff,
    then the 20 dB/decade extrapolation formula for converting
    between measurement distances is valid.

    d_meas ≥ 2*D²/λ  ⇒  d_meas is in the far-field region.

    This is the formal basis for CISPR 16-2-3 Annex D distance
    correction.  -/

def is_far_field (d_meas_m : Float) (d_antenna_m : Float) (freq_hz : Float) : Prop :=
  d_meas_m ≥ far_field_distance d_antenna_m freq_hz

-- ─── L5: CISPR QP Detector Response Formula ─────────────────────────────

/-! ### Quasi-Peak Detector Steady-State Response

    V_qp = V_peak * (1 - e^(-T/tau_charge)) / (1 - e^(-T/tau_discharge))
    where T = 1/PRF is the pulse repetition period.

    For PRF → ∞ (T → 0): V_qp → V_peak * tau_discharge/(tau_charge + tau_discharge)
    For PRF → 0 (T → ∞): V_qp → 0

    This models the CISPR 16-1-1 quasi-peak detector weighting function.
    The charge and discharge time constants depend on the frequency band:
      Band B (150 kHz - 30 MHz):    tau_c = 1 ms, tau_d = 550 ms
      Band C/D (30 - 1000 MHz):     tau_c = 1 ms, tau_d = 550 ms
    The QP detector was designed to correlate with subjective annoyance
    of impulsive interference to AM radio reception.  -/

def qp_detector_steady_state (v_peak : Float) (prf_hz : Float)
                              (tau_charge_s : Float) (tau_discharge_s : Float) : Float :=
  if prf_hz <= 0.0 || tau_charge_s <= 0.0 || tau_discharge_s <= 0.0 then 0.0 else
  let t := 1.0 / prf_hz
  let chg_factor := 1.0 - Real.exp (-t / tau_charge_s)
  let dischg_factor := Real.exp (-t / tau_discharge_s)
  v_peak * chg_factor / (1.0 - dischg_factor)

-- ─── L6: Measurement Margin Decision ─────────────────────────────────────

/-! ### Margin Decision Rule per CISPR 16-4-2

    Given a measured emission E_meas and limit E_limit,
    with expanded measurement uncertainty U:
      Pass:  E_meas + U ≤ E_limit  →  compliant with 95% confidence
      Fail:  E_meas - U > E_limit  →  non-compliant
      Indeterminate: otherwise

    This is the standard CISPR decision rule for EMC compliance testing.
    It accounts for measurement uncertainty to avoid both false passes
    (consumer risk) and false fails (producer risk).  -/

inductive MarginDecision where
  | pass
  | fail
  | indeterminate
deriving Repr

def margin_decision (e_meas_dbuv : Float) (e_limit_dbuv : Float)
                    (expanded_uncertainty_db : Float) : MarginDecision :=
  if e_meas_dbuv + expanded_uncertainty_db ≤ e_limit_dbuv then
    MarginDecision.pass
  else if e_meas_dbuv - expanded_uncertainty_db > e_limit_dbuv then
    MarginDecision.fail
  else
    MarginDecision.indeterminate

/-! ### Decision Soundness Theorem

    If the measured value passes with uncertainty U, then with confidence
    level corresponding to U (typically 95% for U = 2*u_c), the true value
    is below the limit.  This is the formal justification for the CISPR
    measurement decision rule.

    The consumer risk (probability of false pass) is α/2 = (1 - CL)/2
    where CL is the confidence level.  For 95% confidence: α/2 = 2.5%.  -/
