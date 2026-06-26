# Course Alignment — Radiated Emission Antenna Chamber

## Nine-School Curriculum Mapping

### MIT
- **6.630 EM Waves**: Maxwell equations, near/far field, wave impedance
  - Mapped: `fp_hertzian_dipole_field()`, `fp_wave_impedance_magnitude()`
- **6.661 Antennas**: Radiation patterns, gain, impedance
  - Mapped: `at_biconical_gain()`, `at_horn_gain()`, `at_lpda_gain()`
- **6.442 EMC**: Emission measurement, chamber theory
  - Mapped: `re_emission_scan_perform()`, `ct_nsa_theoretical()`

### Stanford
- **EE252 Antenna Theory**: Balanis-based curriculum, Friis equation
  - Mapped: `at_free_space_path_loss()`, `fp_field_from_power()`
- **EE359 Wireless**: Propagation, path loss, fading
  - Mapped: Two-ray ground reflection model
- **EE347 EMC**: Radiated emission, CISPR standards
  - Mapped: `el_init_cispr32_classb_3m()`, all standard limits

### UC Berkeley
- **EE117 EM Waves**: Near-field/far-field, Poynting vector
  - Mapped: `fp_poynting_from_efield()`, `fp_classify_region()`
- **EE123 DSP**: Signal processing for EMI receivers
  - Mapped: QP detector, RBW filtering concepts
- **EE105 Analog**: Receiver design, preamplifier
  - Mapped: `re_preamp_gain_needed()`

### UIUC
- **ECE 451 EM**: Antenna radiation, coupling
  - Mapped: `at_estimate_antenna_factor()`
- **ECE 459 Comm Systems**: RF measurement
  - Mapped: `re_field_strength_compute()`

### Michigan
- **EECS 411 Microwave**: Antenna measurement
  - Mapped: Site attenuation theory
- **EECS 455 Comm**: Wireless channels
  - Mapped: Free-space path loss, reflection

### Georgia Tech
- **ECE 6350 EM**: Computational EM for chambers
  - Mapped: NSA theoretical computation, SVSWR
- **ECE 6601 Comm**: Measurement uncertainty
  - Mapped: `re_measurement_uncertainty()`, `re_compliance_with_uncertainty()`

### TU Munich
- **High-Frequency Engineering**: Antenna design for EMC
  - Mapped: Broadband antenna types, balun theory
- **EMC Measurement Technology**: Chamber qualification
  - Mapped: NSA/SVSWR validation procedures

### ETH Zurich
- **227-0455 EM**: Precision EM measurements
  - Mapped: Antenna factor calibration, uncertainty
- **227-0436 Comm**: RF system testing
  - Mapped: Emission scan algorithms

### Tsinghua
- **Signal and Systems**: Fourier analysis for EMI
  - Mapped: Frequency-domain measurement concepts
- **EM Field**: Antenna fundamentals
  - Mapped: Wavelength, wavenumber, impedance
- **Comm Principles**: RF measurement methodology
  - Mapped: Standard compliance testing