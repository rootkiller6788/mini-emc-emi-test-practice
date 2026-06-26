# Mini EMC/EMI 测试实践（迷你电磁兼容与电磁干扰测试实践）

**从零开始、零依赖的 C 语言实现**，涵盖电磁兼容（EMC）与电磁干扰（EMI）测试实践，包括传导/辐射发射、抗扰度测试、近场诊断、屏蔽设计和预兼容方法。每个模块对应大学级 EMC 课程及国际标准（IEC、CISPR、FCC、MIL-STD），将合规测试流程转化为可运行的 C 代码，实现理论与实践的桥接。

## 子模块总览

| 子模块 | 主题 | 参考课程 |
|-----------|--------|-------------|
| [mini-common-mode-differential-mode-filter](mini-common-mode-differential-mode-filter/) | 共模扼流圈磁学、差模滤波器设计、插入损耗、S 参数、混合模网络分析、滤波器优化 | Stanford EE292, TU Munich HF Engineering |
| [mini-conducted-emission-lisn-test](mini-conducted-emission-lisn-test/) | LISN 阻抗、校准、共模/差模噪声分离、CISPR 11/22/25/32 限值、FCC Part 15、MIL-STD-461 | Stanford EE292, Missouri S&T EE 527 |
| [mini-esd-gun-air-contact-test](mini-esd-gun-air-contact-test/) | 静电放电枪 RLC 模型、IEC 61000-4-2 波形、TLP 测量、ESD 保护器件（TVS、压敏电阻）、合规验证 | Stanford EE292, ETH 227-0455 |
| [mini-near-field-probe-diagnostic](mini-near-field-probe-diagnostic/) | 近场电磁理论、H/E 场探头、探头校准、扫描与成像、信号处理、EMC 诊断算法 | Stanford EE292, ETH 227-0455 |
| [mini-pre-compliance-low-cost-methods](mini-pre-compliance-low-cost-methods/) | 低成本探头与天线、自制 LISN、SDR 频谱分析、TEM 小室、与全合规相关性分析、成本-精度权衡 | Missouri S&T EE 527, Stanford EE292 |
| [mini-radiated-emission-antenna-chamber](mini-radiated-emission-antenna-chamber/) | 双锥/对数周期/喇叭天线、电波暗室 NSA/SVSWR、场地衰减、场均匀性、CISPR 16-1-4、FCC 限值 | Stanford EE292, TU Munich HF Engineering |
| [mini-shielding-gasket-enclosure-design](mini-shielding-gasket-enclosure-design/) | 屏蔽效能、材料特性、导电衬垫设计、开孔泄漏、机箱谐振、RoHS/REACH 合规 | Stanford EE292, Missouri S&T EE 527 |
| [mini-surge-eft-burst-immunity](mini-surge-eft-burst-immunity/) | 浪涌波形（Heidler、双指数）、耦合网络、保护器件、IEC 61000-4-4/-5 测试等级 | Stanford EE292, ETH 227-0455 |

## 设计理念

- **零外部依赖** — 纯 C（C99/C11），仅使用 `libc` 和 `libm`
- **模块自包含** — 每个目录自带 `Makefile`、`include/`、`src/`、`examples/`、`demos/`、`tests/`
- **标准驱动设计** — 每个模块均参考真实的 IEC/CISPR/FCC/MIL-STD 测试流程与限值
- **实用测试工作流** — LISN 阻抗特性分析、ESD 波形生成、近场扫描、屏蔽效能计算等

## 构建方式

每个模块相互独立。进入模块目录后运行：

```bash
cd mini-common-mode-differential-mode-filter
make all    # 构建全部
make test   # 运行测试
```

需要 **GCC** 和 **GNU Make**。

## 项目结构

```
mini-emc-emi-test-practice/
├── mini-common-mode-differential-mode-filter/  # 共模扼流圈磁学、差模/共模滤波设计、插入损耗
├── mini-conducted-emission-lisn-test/          # LISN 阻抗、噪声分离、CISPR/FCC 限值
├── mini-esd-gun-air-contact-test/              # ESD 枪模型、IEC 61000-4-2、TLP、保护器件
├── mini-near-field-probe-diagnostic/           # 近场探头、校准、扫描与成像
├── mini-pre-compliance-low-cost-methods/       # 低成本预兼容、自制 LISN、SDR 分析
├── mini-radiated-emission-antenna-chamber/     # 辐射发射天线、电波暗室理论、CISPR 16-1-4
├── mini-shielding-gasket-enclosure-design/     # 屏蔽效能、导电衬垫、机箱设计
└── mini-surge-eft-burst-immunity/              # 浪涌/EFT 波形、耦合网络、IEC 61000-4-4/-5
```

## 许可证

MIT
