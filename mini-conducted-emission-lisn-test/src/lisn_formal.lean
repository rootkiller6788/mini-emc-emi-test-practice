/- LISN Impedance Network - Lean 4 Formalization -/
/- CISPR 16-1-2, Paul (2006) Ch.8 -/

structure LISNParams where
  L : Float
  R : Float
  hL : 0.0 < L
  hR : 0.0 < R

def LISNParams.cornerFreq (p : LISNParams) : Float :=
  p.R / (2.0 * Float.pi * p.L)

def lisnImpedanceMag (p : LISNParams) (omega : Float) : Float :=
  let XL := omega * p.L
  (p.R * XL) / Float.sqrt (p.R * p.R + XL * XL)

def lisnImpedancePhaseRad (p : LISNParams) (omega : Float) : Float :=
  let XL := omega * p.L
  Float.atan (p.R / XL)

theorem lisn_impedance_pos (p : LISNParams) (omega : Float) (hw : 0.0 < omega) :
    0.0 < lisnImpedanceMag p omega := by
  unfold lisnImpedanceMag
  have hXLpos : 0.0 < omega * p.L := mul_pos hw p.hL
  have hnum : 0.0 < p.R * (omega * p.L) := mul_pos p.hR hXLpos
  have hsumsq : 0.0 < p.R * p.R + (omega * p.L) * (omega * p.L) := by
    positivity
  have hsqrt : 0.0 < Float.sqrt (p.R * p.R + (omega * p.L) * (omega * p.L)) :=
    Float.sqrt_pos.mpr hsumsq
  exact div_pos hnum hsqrt

theorem lisn_impedance_lt_R (p : LISNParams) (omega : Float) (hw : 0.0 < omega) :
    lisnImpedanceMag p omega < p.R := by
  unfold lisnImpedanceMag
  have hXLpos : 0.0 < omega * p.L := mul_pos hw p.hL
  have hsqrt_gt_XL : omega * p.L < Float.sqrt (p.R * p.R + (omega * p.L) * (omega * p.L)) := by
    have hsum_gt : (omega * p.L) * (omega * p.L) < p.R * p.R + (omega * p.L) * (omega * p.L) := by
      nlinarith [p.hR]
    nlinarith
  have hden_pos : 0.0 < Float.sqrt (p.R * p.R + (omega * p.L) * (omega * p.L)) := by
    apply Float.sqrt_pos.mpr; positivity
  exact (div_lt_div_right hden_pos).mpr (mul_lt_mul_of_pos_left hsqrt_gt_XL p.hR)

def voltageDivisionFactor (R_load C_couple omega : Float) : Float :=
  let Zc := 1.0 / (omega * C_couple)
  R_load / Float.sqrt (R_load * R_load + Zc * Zc)

def insertionLoss (R_load C_couple omega : Float) : Float :=
  -20.0 * Float.log10 (voltageDivisionFactor R_load C_couple omega)

theorem vdf_le_one (R_load C_couple omega : Float) (hR : 0.0 < R_load) (hC : 0.0 < C_couple) (hw : 0.0 < omega) :
    voltageDivisionFactor R_load C_couple omega <= 1.0 := by
  unfold voltageDivisionFactor
  have hZc_sq_nonneg : 0.0 <= (1.0 / (omega * C_couple)) * (1.0 / (omega * C_couple)) := by
    apply mul_nonneg; repeat apply div_nonneg (by positivity) (by positivity)
  have hden_ge_R : R_load * R_load <= R_load * R_load + (1.0 / (omega * C_couple)) * (1.0 / (omega * C_couple)) := by
    nlinarith
  have hsqrt_ge_R : R_load <= Float.sqrt (R_load * R_load + (1.0 / (omega * C_couple)) * (1.0 / (omega * C_couple))) := by
    nlinarith [Float.sqrt_le_sqrt hden_ge_R]
  exact (div_le_one ?_).mpr hsqrt_ge_R
  positivity
