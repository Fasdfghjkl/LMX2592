# LMX2592 驱动说明

本目录提供基于 STM32 HAL 的 TI LMX2592 频率合成器驱动。驱动支持硬件 SPI 或软件 SPI，能够根据目标输出频率自动选择输出分频器、计算 PLL N 分频值，并根据参考时钟配置 PFD。

## 主要特性

- 参考时钟由 `LMX2592_REF_CLK` 指定，单位为 MHz。
- `OSC_2X` 由初始化或写频函数参数手动控制，不再依赖固定宏。
- 参考链路寄存器只在初始化、强制重配或 `OSC_2X` 状态变化时重新计算。
- 支持手动预计算参考链路寄存器，以减少运行时计算。
- 根据 VCO 频段变化自动触发校准，减少不必要的 `ACAL` 等待。
- 支持输出通道开关、输出功率设置、MASH 阶数设置。

## 基本配置

在包含 `LMX2592.h` 前或工程全局宏中配置：

```c
#define LMX2592_REF_CLK 100.0
#define LMX2592_VCO_CAL_BAND_MHZ 100
```

如果使用软件 SPI：

```c
#define SOFT_SPI 1
```

如果需要调试打印：

```c
#define LMX2592_DEBUG 1
```

## 初始化

```c
LMX2592_Init(0); // 初始化，关闭 OSC_2X
LMX2592_Init(1); // 初始化，开启 OSC_2X
```

`osc_2x_en` 只在参考输入不高于 200 MHz 时有效。若参考输入超过 200 MHz，自动计算模式会忽略 `OSC_2X`。

## 设置频率

```c
LMX2592_WRITE_FREQ(2400.0, 0);
```

参数说明：

- `freq`：目标输出频率，单位 MHz。
- `acal_en`：是否强制触发自动校准。传入 `0` 时，驱动仍会在必要时自动校准。

如果需要在写频时切换 `OSC_2X`：

```c
LMX2592_WRITE_FREQ_OSC2X(2400.0, 1, 0);
```

当 `OSC_2X` 状态发生变化时，驱动会重新配置参考链路，并使 VCO 校准状态失效，下一次写频会触发校准。

## VCO 自动校准策略

驱动会把计算出的 VCO 频率划分为若干区间：

```c
#define LMX2592_VCO_CAL_BAND_MHZ 100
```

当新频率对应的 VCO 区间不同、参考链路变化、或首次写频时，驱动自动触发 `ACAL`。同一区间内写频不会重复校准，以减少延时。

如果边界频点偶尔无法锁定，可减小区间值：

```c
#define LMX2592_VCO_CAL_BAND_MHZ 50
```

如果更关注切频速度，可适当增大该值，但稳定性可能下降。

## 手动参考寄存器模式

若希望完全跳过运行时参考链路计算，可启用：

```c
#define LMX2592_USE_MANUAL_REF_REGS 1
#define LMX2592_REF_REG1      0x0808
#define LMX2592_REF_REG9      0x0302
#define LMX2592_REF_REG10     0x10D8
#define LMX2592_REF_REG11     0x0018
#define LMX2592_REF_REG12     0x7001
#define LMX2592_REF_FPD_MHZ   100.0
#define LMX2592_REF_FCAL_ADJ  0x0000
```

启用该模式后，用户必须保证寄存器值与实际参考时钟一致。此模式下 `OSC_2X` 参数只用于驱动内部状态比较，参考链路寄存器由宏固定。

## 常用 API

```c
void LMX2592_Init(uint8_t osc_2x_en);
void LMX2592_REF_INIT(uint8_t osc_2x_en);
void LMX2592_SET_OSC2X(uint8_t osc_2x_en);
void LMX2592_WRITE_FREQ(double freq, uint8_t acal_en);
void LMX2592_WRITE_FREQ_OSC2X(double freq, uint8_t osc_2x_en, uint8_t acal_en);
void LMX2592_SETPOWER(uint16_t channel, uint16_t power);
void LMX2592_SETOUT(uint16_t channel, uint16_t state);
void LMX2592_SETNMODE(uint16_t state);
```

通道编号：

- `0`：输出 A
- `1`：输出 B

## 注意事项

- 频率参数单位均为 MHz。
- 当前输出频率范围由 `LMX2592_FIND_CHANNEL_DIV()` 的分频表决定。
- 若跨较大频率范围切换，建议先使用 `acal_en = 1` 验证锁定，再根据应用调整 `LMX2592_VCO_CAL_BAND_MHZ`。
- 函数名 `Wirte_Data` 保留了原工程命名，未改为 `Write_Data`，以避免影响已有调用。
