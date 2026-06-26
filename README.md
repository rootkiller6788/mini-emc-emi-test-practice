# Mini EMC/EMI Test Practice

A collection of **from-scratch, zero-dependency C implementations** of electromagnetic compatibility (EMC) and electromagnetic interference (EMI) test practice, covering conducted/radiated emissions, immunity testing, near-field diagnostics, shielding design, and pre-compliance methods. Each module maps to university-level EMC courses and international standards (IEC, CISPR, FCC, MIL-STD), bridging theory and practice by translating compliance test procedures into runnable C code.

## Sub-Modules

| Sub-Module | Topics | Key Courses |
|-----------|--------|-------------|
| [mini-common-mode-differential-mode-filter](mini-common-mode-differential-mode-filter/) | CM choke magnetics, DM filter design, insertion loss, S-parameters, mixed-mode network analysis, filter optimization | Stanford EE292, TU Munich HF Engineering |
| [mini-conducted-emission-lisn-test](mini-conducted-emission-lisn-test/) | LISN impedance, calibration, CM/DM noise separation, CISPR 11/22/25/32 limits, FCC Part 15, MIL-STD-461 | Stanford EE292, Missouri S&T EE 527 |
| [mini-esd-gun-air-contact-test](mini-esd-gun-air-contact-test/) | ESD gun RLC model, IEC 61000-4-2 waveforms, TLP measurement, ESD protection devices (TVS, varistor), compliance verification | Stanford EE292, ETH 227-0455 |
| [mini-near-field-probe-diagnostic](mini-near-field-probe-diagnostic/) | Near-field EM theory, H/E-field probes, probe calibration, scanning & imaging, signal processing, EMC diagnostic algorithms | Stanford EE292, ETH 227-0455 |
| [mini-pre-compliance-low-cost-methods](mini-pre-compliance-low-cost-methods/) | Budget probes & antennas, DIY LISN, SDR spectrum analysis, TEM cell, correlation to full compliance, cost-accuracy trade-offs | Missouri S&T EE 527, Stanford EE292 |
| [mini-radiated-emission-antenna-chamber](mini-radiated-emission-antenna-chamber/) | Biconical/LPDA/horn antennas, chamber NSA/SVSWR, site attenuation, field uniformity, CISPR 16-1-4, FCC limits | Stanford EE292, TU Munich HF Engineering |
| [mini-shielding-gasket-enclosure-design](mini-shielding-gasket-enclosure-design/) | Shielding effectiveness, material properties, gasket design, aperture leakage, enclosure resonance, RoHS/REACH compliance | Stanford EE292, Missouri S&T EE 527 |
| [mini-surge-eft-burst-immunity](mini-surge-eft-burst-immunity/) | Surge waveforms (Heidler, double-exponential), coupling networks, protection devices, IEC 61000-4-4/-5 test levels | Stanford EE292, ETH 227-0455 |

## Design Philosophy

- **Zero external dependencies** — pure C (C99/C11), only `libc` and `libm`
- **Self-contained modules** — each directory has its own `Makefile`, `include/`, `src/`, `examples/`, `demos/`, `tests/`
- **Standards-driven design** — every module references real IEC/CISPR/FCC/MIL-STD test procedures and limits
- **Practical test workflows** — LISN impedance characterization, ESD waveform generation, near-field scanning, shielding effectiveness calculation, and more

## Building

Each module is standalone. Navigate to a module directory and run:

```bash
cd mini-common-mode-differential-mode-filter
make all    # build everything
make test   # run tests
```

Requires **GCC** and **GNU Make**.

## Project Structure

```
mini-emc-emi-test-practice/
├── mini-common-mode-differential-mode-filter/  # CM choke magnetics, DM/CM filter design, insertion loss
├── mini-conducted-emission-lisn-test/          # LISN impedance, noise separation, CISPR/FCC limits
├── mini-esd-gun-air-contact-test/              # ESD gun model, IEC 61000-4-2, TLP, protection devices
├── mini-near-field-probe-diagnostic/           # Near-field probes, calibration, scanning & imaging
├── mini-pre-compliance-low-cost-methods/       # Low-cost pre-compliance, DIY LISN, SDR analysis
├── mini-radiated-emission-antenna-chamber/     # Emission antennas, chamber theory, CISPR 16-1-4
├── mini-shielding-gasket-enclosure-design/     # Shielding effectiveness, gaskets, enclosure design
└── mini-surge-eft-burst-immunity/              # Surge/EFT waveforms, coupling, IEC 61000-4-4/-5
```

## License

MIT
