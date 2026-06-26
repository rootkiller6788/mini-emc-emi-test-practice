/**
 * surge_defs.h - Core Type Definitions for Surge/EFT/Burst Immunity
 *
 * L1 Definitions: Surge waveform types, EFT burst parameters,
 * protection device classifications, test levels per IEC 61000-4-4/-5.
 *
 * Key Standards:
 *   IEC 61000-4-4: Electrical Fast Transient / Burst Immunity
 *   IEC 61000-4-5: Surge Immunity
 *   IEEE C62.41:   Surge Voltages in Low-Voltage AC Power Circuits
 *   UL 1449:       Surge Protective Devices
 *
 * Courses: Stanford EE292 (EMC), ETH 227-0455 (EM Waves/EMC),
 *   TU Munich High-Frequency Engineering
 * Reference: Paul (2006) Introduction to Electromagnetic Compatibility
 */

#ifndef SURGE_DEFS_H
#define SURGE_DEFS_H

#include <stddef.h>
#include <stdint.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================================================================
 * L1: Surge Waveform Type Classification
 * Per IEC 61000-4-5 and IEEE C62.41
 * ================================================================== */

/** EMC surge waveform types, standardized across international test norms.
 *  Each waveform is defined by its open-circuit voltage and short-circuit
 *  current shapes, characterized by front time / duration.
 *
 *  1.2/50us:  Combination wave open-circuit voltage (IEC 61000-4-5)
 *  8/20us:    Combination wave short-circuit current
 *  10/700us:  Telecom surge waveform (ITU-T K.44)
 *  10/350us:  Direct lightning strike waveform
 *  10/1000us: Long-duration telecom surge
 *  5/50ns:    EFT/Burst per IEC 61000-4-4
 *  0.5us/100kHz: Ring wave per IEEE C62.41
 *  1MHz damped: Industrial switching transient */
typedef enum {
    SURGE_WAVE_1_2_50_US   = 0,  /* 1.2us rise / 50us duration, open-circuit V */
    SURGE_WAVE_8_20_US     = 1,  /* 8us rise / 20us duration, short-circuit I */
    SURGE_WAVE_10_700_US   = 2,  /* 10us rise / 700us duration, telecom line */
    SURGE_WAVE_10_350_US   = 3,  /* 10us rise / 350us duration, direct lightning */
    SURGE_WAVE_10_1000_US  = 4,  /* 10us rise / 1000us duration, telecom long */
    SURGE_WAVE_5_50_NS     = 5,  /* 5ns rise / 50ns duration, EFT pulse */
    SURGE_WAVE_RING_0_5_US = 6,  /* 0.5us ring wave @ 100kHz, oscillatory */
    SURGE_WAVE_1MHZ_DAMPED = 7   /* 1MHz damped sinusoid, switching transient */
} surge_waveform_type_t;

/** Coupling mode for surge injection per IEC 61000-4-5 */
typedef enum {
    SURGE_COUPLE_LINE_TO_LINE     = 0,  /* Differential mode, across L-N or L-L */
    SURGE_COUPLE_LINE_TO_GROUND   = 1,  /* Common mode, line to protective earth */
    SURGE_COUPLE_LINE_TO_LINE_PE  = 2,  /* Simultaneous L-L and L-PE */
    SURGE_COUPLE_TELECOM_LONG     = 3,  /* Longitudinal on telecom pairs */
    SURGE_COUPLE_EXTERNAL         = 4   /* External port (shield to ground) */
} surge_coupling_mode_t;

/** Surge generator source impedance types */
typedef enum {
    SURGE_Z_2_OHM       = 0,  /* 2ohm for low-impedance lines (IEC combination) */
    SURGE_Z_12_OHM      = 1,  /* 12ohm for AC/DC power ports */
    SURGE_Z_40_OHM      = 2,  /* 40ohm for telecom/data lines (ITU-T) */
    SURGE_Z_1_OHM       = 3,  /* 1ohm for direct lightning (10/350us) */
    SURGE_Z_25_OHM      = 4,  /* 25ohm for ring wave */
    SURGE_Z_50_OHM      = 5   /* 50ohm for EFT (capacitive coupling) */
} surge_source_impedance_t;

/* ==================================================================
 * L1: Surge Test Levels - IEC 61000-4-5 and IEC 61000-4-4
 * ================================================================== */

/** IEC 61000-4-5 Surge test severity levels.
 *  Level 1: Protected environment (well-protected)
 *  Level 2: Protected environment with partial lightning exposure
 *  Level 3: Typical industrial environment
 *  Level 4: Severe industrial / outdoor environment */
typedef enum {
    SURGE_LEVEL_1 = 0,  /* 0.5 kV line-to-line, 0.5 kV line-to-ground */
    SURGE_LEVEL_2 = 1,  /* 1.0 kV line-to-line, 1.0 kV line-to-ground */
    SURGE_LEVEL_3 = 2,  /* 2.0 kV line-to-line, 2.0 kV line-to-ground */
    SURGE_LEVEL_4 = 3,  /* 4.0 kV line-to-line, 4.0 kV line-to-ground */
    SURGE_LEVEL_X = 4   /* Special: user-defined voltage level */
} surge_test_level_t;

/** IEC 61000-4-4 EFT/Burst test severity levels.
 *  Level 1: 0.5 kV peak, 5 kHz repetition
 *  Level 2: 1.0 kV peak, 5 kHz repetition
 *  Level 3: 2.0 kV peak, 5 kHz repetition
 *  Level 4: 4.0 kV peak, 2.5 kHz repetition */
typedef enum {
    EFT_LEVEL_1 = 0,  /* 0.5 kV / 5 kHz */
    EFT_LEVEL_2 = 1,  /* 1.0 kV / 5 kHz */
    EFT_LEVEL_3 = 2,  /* 2.0 kV / 5 kHz */
    EFT_LEVEL_4 = 3,  /* 4.0 kV / 2.5 kHz */
    EFT_LEVEL_X = 4   /* Custom */
} eft_test_level_t;

/* ==================================================================
 * L1: Protection Device Types
 * ================================================================== */

/** Surge protection device (SPD) types.
 *  MOV:   Metal Oxide Varistor - voltage-dependent resistor, high energy
 *  TVS_U: Transient Voltage Suppressor, unidirectional (avalanche diode)
 *  TVS_B: TVS bidirectional (back-to-back avalanche diodes)
 *  GDT:   Gas Discharge Tube - arc gap, very high surge current capacity
 *  TSS:   Thyristor Surge Suppressor - crowbar device, low capacitance
 *  SGAP:  Spark Gap - basic air gap, very high energy
 *  DIODE: Steering diode - low capacitance, used in multi-stage
 *  HYBRID: Hybrid protection (e.g., MOV + TVS combo) */
typedef enum {
    PROT_MOV      = 0,  /* Metal Oxide Varistor, ZnO-based */
    PROT_TVS_U    = 1,  /* TVS unidirectional (avalanche silicon diode) */
    PROT_TVS_B    = 2,  /* TVS bidirectional */
    PROT_GDT      = 3,  /* Gas Discharge Tube, noble gas filled */
    PROT_TSS      = 4,  /* Thyristor Surge Suppressor, semiconductor crowbar */
    PROT_SGAP     = 5,  /* Spark Gap, air or surface discharge */
    PROT_DIODE    = 6,  /* Steering/rectifier diode for ESD/EFT */
    PROT_HYBRID   = 7   /* Multi-technology hybrid protector */
} protection_device_type_t;

/** Protection stage in a cascaded protection scheme.
 *  Stage 1 (COARSE):  Highest energy, e.g., GDT, MOV
 *  Stage 2 (FINE):    Fast response, e.g., TVS
 *  Stage 3 (ONBOARD): Lowest residual, e.g., diode array
 *  Stage 4 (ISO):     Galvanic isolation (transformer, optocoupler)
 *  Stage 5 (FILTER):  Passive filter (LC, pi-filter) for EFT suppression */
typedef enum {
    PROT_STAGE_COARSE  = 0, /* Primary / high-energy stage */
    PROT_STAGE_FINE    = 1, /* Secondary / fast stage */
    PROT_STAGE_ONBOARD = 2, /* On-PCB / chip-level protection */
    PROT_STAGE_ISO     = 3, /* Galvanic isolation barrier */
    PROT_STAGE_FILTER  = 4  /* Passive EMI filter stage */
} protection_stage_t;

/* ==================================================================
 * L1: Key Parameter Structures
 * ================================================================== */

/** Surge waveform parameters: define a standardized surge pulse.
 *
 *  The IEC combination wave (1.2/50us - 8/20us) is defined by the
 *  double-exponential function:
 *    v(t) = Vp * k * (1 - exp(-t/tau1)) * exp(-t/tau2)
 *  where tau1 ~ 0.4074 us, tau2 ~ 68.22 us for 1.2/50us voltage wave,
 *  and tau1 ~ 3.00 us, tau2 ~ 27.15 us for 8/20us current wave.
 *
 *  The EFT pulse (5/50ns) follows the same form:
 *    v(t) = Vp * k * (1 - exp(-t/tau1)) * exp(-t/tau2)
 *  with tau1 ~ 1.70 ns, tau2 ~ 68.0 ns. */
typedef struct {
    surge_waveform_type_t type;      /* Waveform standard type */
    double v_peak;                   /* Peak voltage (kV) */
    double i_peak;                   /* Peak current (kA) for current waves */
    double t_front;                  /* Front/rise time (us or ns) */
    double t_duration;               /* Duration to half-value (us or ns) */
    double tau1;                     /* Exponential rise time constant (us) */
    double tau2;                     /* Exponential decay time constant (us) */
    double source_impedance;         /* Generator source impedance (ohm) */
    double energy_per_pulse;         /* Energy per pulse (J) */
    double repetition_rate;          /* Repetition rate for burst (Hz, 0=once) */
    char   description[128];         /* Human-readable waveform description */
} surge_waveform_params_t;

/** EFT burst characteristics per IEC 61000-4-4.
 *
 *  A burst consists of a train of individual pulses:
 *    - Individual pulse: 5ns rise / 50ns duration
 *    - Burst duration: 15ms @ 5kHz (75 pulses) or 0.75ms @ 100kHz
 *    - Burst period: 300ms (repeating every 300ms)
 *    - Test duration: >= 1 minute per coupling mode */
typedef struct {
    eft_test_level_t level;          /* Test severity level */
    double v_peak;                   /* Peak voltage per pulse (kV) */
    double pulse_rise_ns;            /* Pulse rise time (ns), typically 5 */
    double pulse_width_ns;           /* Pulse width at 50% (ns), typically 50 */
    double burst_duration_ms;        /* Burst envelope duration (ms) */
    double burst_period_ms;          /* Repetition period of burst (ms) */
    double repetition_khz;           /* Individual pulse rep rate (kHz) */
    int    pulses_per_burst;         /* Number of pulses per burst */
    double coupling_cap_pf;          /* Coupling capacitance (pF, typically 33nF) */
    double test_duration_sec;        /* Total test duration per port (s) */
} eft_burst_params_t;

/** Protection device parameters.
 *
 *  Each SPD technology has distinct characteristics:
 *  MOV:    V1mA ref voltage, alpha non-linearity exponent (~20-50)
 *  TVS:    V_BR breakdown voltage (typ 6.8V-440V), C_j junction cap
 *  GDT:    V_arc arc voltage (~15-25V once triggered), V_spark spark-over
 *  TSS:    V_BO breakover voltage, I_H holding current, low capacitance */
typedef struct {
    protection_device_type_t type;   /* Device technology */
    double v_standoff;               /* Maximum continuous operating voltage (V) */
    double v_breakdown_min;          /* Minimum breakdown voltage (V) */
    double v_breakdown_max;          /* Maximum breakdown voltage (V) */
    double v_clamp_rated;            /* Rated clamping voltage @ rated I_pp (V) */
    double v_arc;                    /* Arc voltage for GDT (V), 0 for solid-state */
    double i_peak_rated;             /* Rated peak pulse current (A), 8/20us */
    double i_peak_max;               /* Absolute max peak pulse current (A) */
    double p_peak_pulse;             /* Peak pulse power rating (W), 10/1000us */
    double energy_rating;            /* Single-pulse energy rating (J) */
    double capacitance_typ;          /* Typical off-state capacitance (pF) */
    double clamping_ratio;           /* V_clamp / V_breakdown ratio */
    double response_time_ps;         /* Response time (ps) */
    double leakage_current_ua;       /* Leakage current at V_standoff (uA) */
    double alpha;                    /* MOV non-linearity exponent (20-50) */
    double thermal_resistance;       /* Junction-to-ambient thermal R (degC/W) */
    int    surge_life_cycles;        /* Rated number of surge events */
    char   part_number[64];          /* Typical representative part number */
} protection_device_params_t;

/** Multi-stage protection configuration.
 *
 *  Cascaded protection addresses different energy/time scales:
 *  Stage 1 (GDT):  handles kA-level surges, slow (us response)
 *  Stage 2 (MOV):  handles 100A-1kA, medium speed (ns-us)
 *  Stage 3 (TVS):  handles 1A-100A, fast (<1ns response)
 *  Decoupling impedance between stages ensures coordination.
 *
 *  Coordination condition: V_clamp(stage_N) < V_breakdown(stage_N+1) */
typedef struct {
    int            num_stages;        /* Number of protection stages (1-5) */
    protection_device_params_t devices[5];  /* Devices at each stage */
    protection_stage_t stage_types[5];      /* Functional stage type */
    double         decoupling_R[4];          /* Series decoupling resistance (ohm) */
    double         decoupling_L[4];          /* Series decoupling inductance (uH) */
    double         residual_v_at_load;       /* Let-through voltage at load (V) */
    double         total_energy_absorbed;    /* Total energy dissipated (J) */
    double         stage_energy_fraction[5]; /* Energy share per stage */
} multistage_protection_t;

/* ==================================================================
 * L1: Error and Status Codes
 * ================================================================== */

typedef enum {
    SURGE_OK                     = 0,
    SURGE_ERR_NULL_PTR           = -1,
    SURGE_ERR_INVALID_WAVEFORM   = -2,
    SURGE_ERR_INVALID_LEVEL      = -3,
    SURGE_ERR_INVALID_IMPEDANCE  = -4,
    SURGE_ERR_DIVERGENCE         = -5,
    SURGE_ERR_OVER_STRESS        = -6,
    SURGE_ERR_THERMAL_RUNAWAY    = -7,
    SURGE_ERR_PROTECTION_FAIL    = -8,
    SURGE_ERR_COORDINATION_FAIL  = -9,
    SURGE_ERR_NUMERICAL          = -10,
    SURGE_ERR_ALLOC              = -11
} surge_error_t;

/** Test result for a single surge/EFT test sequence */
typedef struct {
    surge_error_t result;              /* Overall test result */
    int           num_pulses_applied;  /* Number of pulses applied */
    int           num_failures;        /* Number of equipment failures observed */
    double        max_residual_v;      /* Maximum residual voltage measured (V) */
    double        max_residual_i;      /* Maximum residual current measured (A) */
    double        total_energy_dumped; /* Total energy into protection (J) */
    double        peak_temp_rise;      /* Peak temperature rise in protector (K) */
    char          failure_mode[256];   /* Description of failure if any */
} surge_test_result_t;

/* ==================================================================
 * L1: Physical Constants for Surge / EMC Engineering
 * ================================================================== */

/* Surge impedance environments */
#define SURGE_Z_POWER_LINE   2.0
#define SURGE_Z_TELECOM     40.0
#define SURGE_Z_DATA_LINE   42.0
#define SURGE_Z_AC_MAINS    12.0

/* Standard EFT parameters per IEC 61000-4-4 */
#define EFT_PULSE_RISE_NS     5.0
#define EFT_PULSE_WIDTH_NS   50.0
#define EFT_BURST_DURATION_MS 15.0
#define EFT_BURST_PERIOD_MS  300.0
#define EFT_REP_RATE_KHZ       5.0
#define EFT_COUPLING_CAP_PF 33000.0

/* Standard surge temporal constants (double-exponential model) */
#define SURGE_TAU1_1_2_50    0.4074
#define SURGE_TAU2_1_2_50   68.22
#define SURGE_TAU1_8_20      3.00
#define SURGE_TAU2_8_20     27.15
#define SURGE_TAU1_5_50_NS   0.0017
#define SURGE_TAU2_5_50_NS   0.068
#define SURGE_TAU1_10_700    2.93
#define SURGE_TAU2_10_700  953.0

/** Joule-energy constant: E = 0.5*C*V^2 (energy in a charged capacitor) */
#define SURGE_ENERGY_CONSTANT 0.5

/**
 * Get surge test level voltage per IEC 61000-4-5.
 * Returns voltage in kV for the specified level.
 */
static inline double surge_test_voltage_kv(surge_test_level_t level) {
    switch (level) {
        case SURGE_LEVEL_1: return 0.5;
        case SURGE_LEVEL_2: return 1.0;
        case SURGE_LEVEL_3: return 2.0;
        case SURGE_LEVEL_4: return 4.0;
        default:            return 0.0;
    }
}

/** Get EFT test voltage per IEC 61000-4-4 */
static inline double eft_test_voltage_kv(eft_test_level_t level) {
    switch (level) {
        case EFT_LEVEL_1: return 0.5;
        case EFT_LEVEL_2: return 1.0;
        case EFT_LEVEL_3: return 2.0;
        case EFT_LEVEL_4: return 4.0;
        default:          return 0.0;
    }
}

/** Get EFT pulse repetition frequency per level */
static inline double eft_rep_rate_khz(eft_test_level_t level) {
    switch (level) {
        case EFT_LEVEL_1: return 5.0;
        case EFT_LEVEL_2: return 5.0;
        case EFT_LEVEL_3: return 5.0;
        case EFT_LEVEL_4: return 2.5;
        default:          return 5.0;
    }
}

/** Waveform name lookup for reporting */
static inline const char* surge_waveform_name(surge_waveform_type_t wf) {
    switch (wf) {
        case SURGE_WAVE_1_2_50_US:    return "1.2/50us Voltage";
        case SURGE_WAVE_8_20_US:      return "8/20us Current";
        case SURGE_WAVE_10_700_US:    return "10/700us Telecom";
        case SURGE_WAVE_10_350_US:    return "10/350us Lightning";
        case SURGE_WAVE_10_1000_US:   return "10/1000us Telecom Long";
        case SURGE_WAVE_5_50_NS:      return "5/50ns EFT Pulse";
        case SURGE_WAVE_RING_0_5_US:  return "0.5us 100kHz Ring Wave";
        case SURGE_WAVE_1MHZ_DAMPED:  return "1MHz Damped Sinusoid";
        default:                      return "Unknown";
    }
}

/** Get standard source impedance for a waveform type */
static inline double surge_source_impedance(surge_waveform_type_t wf) {
    switch (wf) {
        case SURGE_WAVE_1_2_50_US:    return 2.0;
        case SURGE_WAVE_8_20_US:      return 2.0;
        case SURGE_WAVE_10_700_US:    return 40.0;
        case SURGE_WAVE_10_350_US:    return 1.0;
        case SURGE_WAVE_10_1000_US:   return 40.0;
        case SURGE_WAVE_5_50_NS:      return 50.0;
        case SURGE_WAVE_RING_0_5_US:  return 25.0;
        case SURGE_WAVE_1MHZ_DAMPED:  return 50.0;
        default:                      return 0.0;
    }
}

#ifdef __cplusplus
}
#endif

#endif /* SURGE_DEFS_H */