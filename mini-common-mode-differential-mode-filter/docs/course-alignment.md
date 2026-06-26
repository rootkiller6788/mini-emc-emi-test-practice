# Course Alignment — CM/DM EMI Filter

## MIT
| Course | Topic | Coverage |
|--------|-------|----------|
| 6.002 Circuits & Electronics | RLC circuits, resonance, damping | `filter_resonance_frequency()`, `filter_quality_factor()` |
| 6.003 Signal Processing | Transfer functions, Bode plots, s-domain analysis | `filter_stage_transfer()`, `filter_stage_bode()` |
| 6.630 EM Waves | Faraday's law, magnetic circuits, core physics | `cm_choke_create()`, core geometry/loss models |

## Stanford
| Course | Topic | Coverage |
|--------|-------|----------|
| EE101B Circuits II | Two-port networks, S-parameters | `z_to_s_matrix()`, `abcd_cascade_n()` |
| EE142 EM Fields | Magnetic materials, ferrite physics | `core_material_lookup()`, `complex_permeability_calc()` |
| EE292 EMC | EMI filter design, CISPR standards | EMC limits database, `filter_design_emi()` |

## Berkeley
| Course | Topic | Coverage |
|--------|-------|----------|
| EE105 Analog Circuits | Impedance, parasitics modeling | `filter_element_impedance()`, parasitics |
| EE117 EM Fields | Maxwell, Faraday, inductance calculation | `cm_choke_create()`, AL value |
| EE123 DSP | Digital filter analog — z-domain mapping | (documented, not implemented) |

## Illinois
| Course | Topic | Coverage |
|--------|-------|----------|
| ECE 310 DSP | Frequency response, Bode plots | Bode plot generation |
| ECE 451 EM | Magnetic circuits, core design | Core material database |
| ECE 459 Communications | Noise, SNR, interference | EMC limit analysis |

## Michigan
| Course | Topic | Coverage |
|--------|-------|----------|
| EECS 411 Microwave | S-parameters, network analysis | Mixed-mode S-parameters |
| EECS 418 Power Electronics | EMI in converters, filter design | AC-DC SMPS example |

## Georgia Tech
| Course | Topic | Coverage |
|--------|-------|----------|
| ECE 6350 EM Applications | EMC, shielding, filtering | System-level IL analysis |
| ECE 6601 Comm Systems | Signal integrity, eye diagrams | (peripheral coverage) |

## TU Munich
| Course | Topic | Coverage |
|--------|-------|----------|
| High-Frequency Engineering | Network parameters, filter synthesis | ABCD-to-S conversion |
| EMC of ICs and Systems | Integrated EMI filtering | Active EMI filter |

## ETH Zurich
| Course | Topic | Coverage |
|--------|-------|----------|
| 227-0455 EM: Theory & Applications | Magnetic materials, core loss | Steinmetz/GSE/iGSE models |
| 227-0436 Comm Electronics | EMI in communication systems | USB/Ethernet CM choke presets |

## Tsinghua (清华)
| Course | Topic | Coverage |
|--------|-------|----------|
| 电磁场 (EM Fields) | Faraday, inductance, core | `cm_choke_create()` |
| 通信原理 (Communication Principles) | Noise, interference, EMC | EMC standard limits |
| 电力电子 (Power Electronics) | Converter EMI, filter design | Motor drive/SMPS examples |
