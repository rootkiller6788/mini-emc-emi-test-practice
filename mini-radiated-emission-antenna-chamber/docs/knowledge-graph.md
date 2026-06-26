# Knowledge Graph — Radiated Emission Antenna Chamber

## L1: Definitions (Complete)
- Antenna Factor (AF) [dB(1/m)] — `re_antenna_factor_point_t`, `re_antenna_factor_table_t`
- Electric Field Strength [V/m, dBuV/m] — `re_emission_point_t`
- Normalized Site Attenuation (NSA) [dB] — `nsa_measurement_t`
- Site VSWR (SVSWR) — `svswr_point_t`
- Field Uniformity (FU) — `field_uniformity_point_t`
- Measurement Distance (3m/10m/1m) — `re_measurement_config_t`
- Detector Types (Peak/QP/Avg/RMS) — `re_detector_type_t`
- Measurement Standards (CISPR/FCC/MIL/CISPR25) — `re_standard_t`
- EUT Configuration — `re_eut_descriptor_t`, `re_eut_config_t`
- Chamber Types (OATS/SAC/FAR/GTEM/Reverb) — `re_site_type_t`

## L2: Core Concepts (Complete)
- Far-field / Near-field transition — `fp_classify_region()`
- Antenna reciprocity in EMC measurements — `antenna_factor.c`
- Ground plane reflection (image theory) — `ct_ground_reflection_coeff()`
- Polarization matching (H/V optimization) — `re_polarization_t`
- Height scan for max emission — `ct_nsa_height_scan_opt()`
- Turntable rotation — `re_emission_scan_perform()`
- Pre-scan vs final measurement — `pre_scan()`, `re_emission_scan_perform()`
- Antenna factor interpolation — `re_af_interpolate()`
- Cable loss compensation — `re_field_strength_compute()`
- Chamber qualification concepts — `chamber_qualify.c`

## L3: Mathematical Structures (Complete)
- Friis transmission equation — `at_free_space_path_loss()`, `fp_field_from_power()`
- Free-space path loss formula — `ct_nsa_theoretical()`
- Two-ray ground reflection model — `ct_nsa_theoretical()`
- Complex wave impedance — `fp_wave_impedance_magnitude()`
- Poynting vector computation — `fp_poynting_from_efield()`
- dB conversions (dBm/dBuV/dBuV/m) — `re_dbm_to_dbuv()`, `dbuvm_to_vm()`
- Antenna factor from gain (Balanis eqn) — `re_antenna_factor_from_gain()`
- Wavenumber k = 2*pi/lambda — `fp_wavenumber()`
- Wavelength lambda = c/f — `at_wavelength()`
- Fresnel reflection coefficients — `ct_ground_reflection_coeff()`

## L4: Fundamental Laws (Complete)
- Friis Transmission Equation — `friisReceivedPower` (Lean), `at_free_space_path_loss()` (C)
- Reciprocity Theorem for antennas — `antenna_factor.c` derivation
- Image Theory (perfect ground plane) — `ct_nsa_theoretical()`, `groundReflectionFactor` (Lean)
- Schelkunoff Equivalence Principle — `antenna_types.c` biconical analysis
- Carson Reciprocity — applied in antenna factor calibration
- IEEE Std 145-2013 field regions — `fp_classify_region()`
- CISPR 16-1-4 NSA acceptance criterion — `nsa_measurement_t.pass`

## L5: Algorithms/Methods (Complete)
- Antenna factor interpolation (log-freq linear) — `re_af_interpolate()`
- Height scan optimization (golden section) — `ct_nsa_height_scan_opt()`
- Max-hold emission search — `height_scan_max_hold()`
- Pre-scan algorithm — `pre_scan()`
- NSA measurement procedure — `ct_nsa_measure_single()`
- SVSWR measurement procedure — `svswr_measure_at_frequency()`
- Field uniformity evaluation — `ct_field_uniformity_evaluate()`
- Limit query with distance extrapolation — `el_query_limit()`, `el_convert_distance()`
- Chamber quiet zone estimation — `ct_quiet_zone_estimate()`
- Measurement uncertainty budget — `re_measurement_uncertainty()`
- Biconical/LPDA/Horn gain estimation — `at_biconical_gain()`, `at_lpda_gain()`, `at_horn_gain()`

## L6: Canonical Problems (Complete)
- CISPR 32 Class B emission test (3m SAC) — `example_cispr32_test.c`
- Chamber NSA validation (CISPR 16-1-4) — `example_nsa_validation.c`
- Near-field/Far-field transition analysis — `example_near_far_field.c`
- Biconical antenna 30-300 MHz — `re_af_table_init_standard("BICONICAL")`
- LPDA antenna 200-1000 MHz — `re_af_table_init_standard("LPDA")`
- Horn antenna 1-18 GHz — `re_af_table_init_standard("HORN")`
- 3m vs 10m distance comparison — `fp_extrapolate_distance()`
- Half-wave dipole reference — `re_antenna_factor_from_gain(300e6, 1.64)`

## L7: Applications (Complete — 5 applications)
- CISPR 11 (ISM equipment) — `el_init_cispr11_classb_10m()`
- CISPR 22 (ITE) — `el_init_cispr22_classb_3m()`
- CISPR 32 (Multimedia Equipment) — `el_init_cispr32_classb_3m()`
- FCC Part 15 (Digital Devices) — `el_init_fcc15_classb_3m()`
- MIL-STD-461G RE102 (Military) — `el_init_mil461_re102_navy_1m()`
- CISPR 25 (Automotive) — `el_init_cispr25_class5_1m()`

## L8: Advanced Topics (Partial — 3/5)
- Time-domain EMI (TDEMI) measurement — documented in course-tree.md
- Reverberation chamber theory — `RE_SITE_REVERB` enum, chamber profile
- Full-vehicle chamber testing — chamber_qualify.c chamber_profile_t
- Stochastic emission modeling — (future)
- Near-field scanning diagnostics — `fp_loop_antenna_field()`

## L9: Research Frontiers (Partial — documented)
- AI-driven emission prediction — documented
- mmWave OTA chamber testing — documented
- Virtual chamber simulation — documented
- 6G EMC — documented
- Quantum EMI sensing — documented