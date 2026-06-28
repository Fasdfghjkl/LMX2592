# LMX2592 Driver

This directory contains an STM32 HAL based driver for the TI LMX2592 frequency synthesizer. The driver supports hardware SPI or software SPI, selects the output channel divider from the target output frequency, calculates the PLL N divider, and configures the PFD from the reference clock.

## Features

- Reference clock is configured by `LMX2592_REF_CLK` in MHz.
- `OSC_2X` is selected at initialization or frequency-write time by function arguments.
- Reference-path registers are recalculated only during initialization, forced reconfiguration, or an `OSC_2X` state change.
- Optional manual reference-register mode removes runtime reference-path calculation.
- VCO calibration is automatically triggered when the computed VCO frequency crosses a configured band.
- Output enable, output power, and MASH order helper functions are provided.

## Basic Configuration

Define these macros before including `LMX2592.h`, or add them to the project-level compiler definitions:

```c
#define LMX2592_REF_CLK 100.0
#define LMX2592_VCO_CAL_BAND_MHZ 100
```

Enable software SPI when needed:

```c
#define SOFT_SPI 1
```

Enable debug printing when needed:

```c
#define LMX2592_DEBUG 1
```

## Initialization

```c
LMX2592_Init(0); // Initialize with OSC_2X disabled.
LMX2592_Init(1); // Initialize with OSC_2X enabled.
```

`osc_2x_en` is only valid when the OSCin frequency is not greater than 200 MHz. In automatic reference mode, the driver ignores `OSC_2X` when the configured reference clock is above that limit.

## Frequency Programming

```c
LMX2592_WRITE_FREQ(2400.0, 0);
```

Parameters:

- `freq`: target output frequency in MHz.
- `acal_en`: forces automatic calibration when set to `1`. When set to `0`, the driver still runs calibration when it is required.

To switch `OSC_2X` while programming a frequency:

```c
LMX2592_WRITE_FREQ_OSC2X(2400.0, 1, 0);
```

When the `OSC_2X` state changes, the driver reconfigures the reference path and invalidates the saved VCO calibration state. The next frequency write will run calibration.

## VCO Calibration Strategy

The driver maps the computed VCO frequency into software-defined bands:

```c
#define LMX2592_VCO_CAL_BAND_MHZ 100
```

Calibration is run on the first frequency write, when the reference path changes, or when the new VCO frequency crosses a band boundary. Frequency changes inside the same band skip calibration to reduce delay.

If some boundary frequencies do not lock reliably, use a smaller band:

```c
#define LMX2592_VCO_CAL_BAND_MHZ 50
```

A larger value can reduce calibration frequency, but may reduce locking robustness.

## Manual Reference-Register Mode

To skip runtime reference-path calculation completely, enable:

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

When this mode is enabled, the user must ensure that the supplied register values match the actual reference clock. The `OSC_2X` argument is still used for internal state tracking, but the reference-path registers are fixed by macros.

## Common APIs

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

Channel IDs:

- `0`: output A
- `1`: output B

## Notes

- All frequency arguments are in MHz.
- The output-frequency range is limited by the divider table in `LMX2592_FIND_CHANNEL_DIV()`.
- For large frequency jumps, first validate locking with `acal_en = 1`, then tune `LMX2592_VCO_CAL_BAND_MHZ` for the application.
- The function name `Wirte_Data` is kept for compatibility with the existing project.
