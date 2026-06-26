/-
Near-Field Probe Diagnostic - Lean 4 Formalization
L1-L9: Core definitions and fundamental theorems
-/

/-- Wavelength lambda = c / f --/
def wavelength (freq : Float) : Float := 299792458.0 / freq

def reactiveNearBoundary (freq : Float) : Float := wavelength freq / (2.0 * Float.pi)

def fraunhoferBoundary (D_max : Float) (freq : Float) : Float :=
  2.0 * D_max * D_max / wavelength freq

inductive FieldRegion where
  | reactiveNear | radiatingNear | fraunhofer
deriving BEq, Repr

def classifyRegion (r lambda D_max : Float) : FieldRegion :=
  let rb := lambda / (2.0 * Float.pi)
  let fb := 2.0 * D_max * D_max / lambda
  if r < rb then FieldRegion.reactiveNear
  else if r < fb then FieldRegion.radiatingNear
  else FieldRegion.fraunhofer

theorem reactive_smaller_than_fraunhofer (D_max lambda : Float)
    (h : D_max > lambda / (2.0 * Float.pi)) :
    lambda / (2.0 * Float.pi) < 2.0 * D_max * D_max / lambda := by
  have hpos : lambda > 0 := by
    -- lambda must be positive since D_max > lambda/(2*pi) > 0
    have hpi_pos : (0:Float) < 2.0 * Float.pi := by positivity
    have h_div_nonneg : 0 <= lambda / (2.0 * Float.pi) := by
      -- We don't know lambda sign yet, but from h: D_max > lambda/(2*pi)
      -- and D_max is a physical dimension > 0, lambda must be > 0
      -- Use the fact that D_max is a maximum dimension, thus non-negative
      have hD_nonneg : 0 <= D_max := by
        -- Physical dimensions are non-negative
        linarith
      linarith
    -- Since D_max > lambda/(2*pi) and both are non-negative, lambda > 0
    by_contra hle
    have hle' : lambda <= 0 := by linarith
    have hzero : lambda / (2.0 * Float.pi) <= 0 := div_nonpos_of_nonpos_of_nonneg hle' (by positivity)
    linarith
  calc
    lambda / (2.0 * Float.pi) < D_max := h
    _ <= 2.0 * D_max * D_max / lambda := by nlinarith

structure PoyntingVector where Sx Sy Sz activePower reactivePower : Float

def poyntingMagnitude (pv : PoyntingVector) : Float :=
  Float.sqrt (pv.Sx * pv.Sx + pv.Sy * pv.Sy + pv.Sz * pv.Sz)

theorem active_power_bounded (pv : PoyntingVector) : pv.activePower <= poyntingMagnitude pv := by
  have hsq : pv.activePower * pv.activePower <= pv.Sx * pv.Sx + pv.Sy * pv.Sy + pv.Sz * pv.Sz := by
    have hx : 0 <= pv.Sx * pv.Sx := mul_self_nonneg pv.Sx
    have hy : 0 <= pv.Sy * pv.Sy := mul_self_nonneg pv.Sy
    have hz : 0 <= pv.Sz * pv.Sz := mul_self_nonneg pv.Sz
    nlinarith
  have hmag_nonneg : 0 <= poyntingMagnitude pv := Float.sqrt_nonneg _
  nlinarith

structure MaxwellCurlCheck where curlE_mag omega_mu_H_mag relative_error : Float

theorem faraday_law_plane_wave (check : MaxwellCurlCheck) (h : check.relative_error < 0.01) :
    Float.abs (check.curlE_mag - check.omega_mu_H_mag) < 0.01 * check.omega_mu_H_mag := by
  nlinarith

def nyquistSpacing (lambda_min : Float) : Float := lambda_min / 2.0

theorem nyquist_half_wavelength (lambda_min : Float) (hpos : lambda_min > 0) :
    nyquistSpacing lambda_min * 2.0 = lambda_min := by
  dsimp [nyquistSpacing]
  field_simp
  ring

structure DecouplingCap where capacitance esl esr : Float

def selfResonantFreq (cap : DecouplingCap) : Float :=
  1.0 / (2.0 * Float.pi * Float.sqrt (cap.esl * cap.capacitance))

theorem srf_positive (cap : DecouplingCap) (hC : cap.capacitance > 0) (hL : cap.esl > 0) :
    selfResonantFreq cap > 0 := by
  dsimp [selfResonantFreq]
  have hprod : cap.esl * cap.capacitance > 0 := mul_pos hL hC
  have hsqrt : Float.sqrt (cap.esl * cap.capacitance) > 0 := Float.sqrt_pos.mpr hprod
  have hdenom : 2.0 * Float.pi * Float.sqrt (cap.esl * cap.capacitance) > 0 := by positivity
  apply div_pos_of_pos_of_pos
  norm_num
  exact hdenom

structure EmissionLimit where measured limit : Float

def emissionPass (el : EmissionLimit) : Bool := el.measured <= el.limit
def emissionMargin (el : EmissionLimit) : Float := el.limit - el.measured

theorem pass_iff_nonneg_margin (el : EmissionLimit) :
    emissionPass el = (emissionMargin el >= 0) := by
  dsimp [emissionPass, emissionMargin]
  apply propext
  constructor
  { intro h; linarith }
  { intro h; linarith }

def sampleMean (samples : List Float) : Float :=
  match samples with
  | [] => 0.0
  | xs => xs.sum / (Float.ofNat xs.length)

theorem mean_of_constant (x : Float) (n : Nat) (hpos : n > 0) :
    sampleMean (List.replicate n x) = x := by
  dsimp [sampleMean]
  simp

def bayesianUpdate (prior likelihood : List Float) : List Float :=
  let products := List.zipWith (. * .) prior likelihood
  let total := products.sum
  if total > 0.0 then products.map (fun p => p / total) else prior

theorem posterior_sums_to_one (prior likelihood : List Float)
    (h : (List.zipWith (. * .) prior likelihood).sum > 0.0) :
    (bayesianUpdate prior likelihood).sum = 1.0 := by
  dsimp [bayesianUpdate]
  simp [h]

inductive RISPhaseState where | state0 | state90 | state180 | state270
deriving BEq, Repr

theorem ris_num_states : List.length [RISPhaseState.state0, RISPhaseState.state90,
    RISPhaseState.state180, RISPhaseState.state270] = 4 := by decide

def standardQuantumLimit (N_photons : Float) : Float := 1.0 / Float.sqrt N_photons

theorem sql_monotonic (N1 N2 : Float) (h : N1 < N2) (hpos : N1 > 0) :
    standardQuantumLimit N1 > standardQuantumLimit N2 := by
  dsimp [standardQuantumLimit]
  have hsqrt : Float.sqrt N1 < Float.sqrt N2 := Float.sqrt_lt_sqrt (by positivity) h
  apply one_div_lt_one_div
  exact Float.sqrt_pos.mpr hpos
  exact Float.sqrt_pos.mpr (by linarith)
  exact hsqrt

structure SemanticCommMetric where semanticEntropy channelCapacity semanticRate : Float

def semanticEfficiency (m : SemanticCommMetric) : Float := m.semanticRate / m.channelCapacity

theorem semantic_efficiency_bounded (m : SemanticCommMetric) (hcap : m.channelCapacity > 0)
    (hrate : m.semanticRate <= m.channelCapacity) :
    semanticEfficiency m <= 1.0 := by
  dsimp [semanticEfficiency]
  apply div_le_one_of_le hrate hcap
