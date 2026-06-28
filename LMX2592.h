#ifndef __LMX2592_H_
#define __LMX2592_H_

#include "main.h"

#ifndef SOFT_SPI
#define SOFT_SPI	0
#endif

#ifndef LMX2592_DEBUG
#define LMX2592_DEBUG	0
#endif

/* Unit: MHz. Keep this value in the LMX2592 OSCin supported range. */
#ifndef LMX2592_REF_CLK
#define LMX2592_REF_CLK	100.0
#endif

/*
 * Unit: MHz. Smaller values trigger VCO calibration more often but are safer.
 * The driver calibrates when the computed VCO frequency crosses this band.
 */
#ifndef LMX2592_VCO_CAL_BAND_MHZ
#define LMX2592_VCO_CAL_BAND_MHZ	100
#endif

#if LMX2592_VCO_CAL_BAND_MHZ <= 0
#error "LMX2592_VCO_CAL_BAND_MHZ must be greater than 0."
#endif

/*
 * Set to 1 to provide precomputed reference-path register values and skip
 * runtime reference-path calculation.
 *
 * Required when enabled:
 * LMX2592_REF_REG1, LMX2592_REF_REG9, LMX2592_REF_REG10,
 * LMX2592_REF_REG11, LMX2592_REF_REG12, LMX2592_REF_FPD_MHZ,
 * LMX2592_REF_FCAL_ADJ.
 */
#ifndef LMX2592_USE_MANUAL_REF_REGS
#define LMX2592_USE_MANUAL_REF_REGS	0
#endif

#if LMX2592_USE_MANUAL_REF_REGS
#ifndef LMX2592_REF_REG1
#error "Define LMX2592_REF_REG1 when LMX2592_USE_MANUAL_REF_REGS is 1."
#endif

#ifndef LMX2592_REF_REG9
#error "Define LMX2592_REF_REG9 when LMX2592_USE_MANUAL_REF_REGS is 1."
#endif

#ifndef LMX2592_REF_REG10
#error "Define LMX2592_REF_REG10 when LMX2592_USE_MANUAL_REF_REGS is 1."
#endif

#ifndef LMX2592_REF_REG11
#error "Define LMX2592_REF_REG11 when LMX2592_USE_MANUAL_REF_REGS is 1."
#endif

#ifndef LMX2592_REF_REG12
#error "Define LMX2592_REF_REG12 when LMX2592_USE_MANUAL_REF_REGS is 1."
#endif

#ifndef LMX2592_REF_FPD_MHZ
#error "Define LMX2592_REF_FPD_MHZ when LMX2592_USE_MANUAL_REF_REGS is 1."
#endif

#ifndef LMX2592_REF_FCAL_ADJ
#error "Define LMX2592_REF_FCAL_ADJ when LMX2592_USE_MANUAL_REF_REGS is 1."
#endif
#endif

#define REG_0	((uint8_t)0x00)
#define REG_1	((uint8_t)0x01)
#define REG_2	((uint8_t)0x02)
#define REG_4	((uint8_t)0x04)
#define REG_7	((uint8_t)0x07)
#define REG_8	((uint8_t)0x08)
#define REG_9	((uint8_t)0x09)
#define REG_A	((uint8_t)0x0A)
#define REG_B	((uint8_t)0x0B)
#define REG_C	((uint8_t)0x0C)
#define REG_D	((uint8_t)0x0D)
#define REG_E	((uint8_t)0x0E)

#define REG_13	((uint8_t)0x13)
#define REG_14	((uint8_t)0x14)
#define REG_16	((uint8_t)0x15)
#define REG_17	((uint8_t)0x17)
#define REG_1E	((uint8_t)0x1E)
#define REG_1F	((uint8_t)0x1F)
#define REG_23	((uint8_t)0x23)
#define REG_24	((uint8_t)0x24)
#define REG_25	((uint8_t)0x25)
#define REG_26	((uint8_t)0x26)
#define REG_27	((uint8_t)0x27)
#define REG_28	((uint8_t)0x28)
#define REG_29	((uint8_t)0x29)
#define REG_2A	((uint8_t)0x2A)
#define REG_2B	((uint8_t)0x2B)
#define REG_2C	((uint8_t)0x2C)
#define REG_2D	((uint8_t)0x2D)

#if SOFT_SPI
#define PLL_SCK_SET()		HAL_GPIO_WritePin(LMX2592_SCK_GPIO_Port, LMX2592_SCK_Pin, GPIO_PIN_SET);
#define PLL_SCK_RESET()	HAL_GPIO_WritePin(LMX2592_SCK_GPIO_Port, LMX2592_SCK_Pin, GPIO_PIN_RESET);

#define PLL_CS_SET()		HAL_GPIO_WritePin(LMX2592_CSB_GPIO_Port, LMX2592_CSB_Pin, GPIO_PIN_SET);
#define PLL_CS_RESET()	HAL_GPIO_WritePin(LMX2592_CSB_GPIO_Port, LMX2592_CSB_Pin, GPIO_PIN_RESET);

#define PLL_SDI_SET()		HAL_GPIO_WritePin(LMX2592_SDI_GPIO_Port, LMX2592_SDI_Pin, GPIO_PIN_SET);
#define PLL_SDI_RESET()	HAL_GPIO_WritePin(LMX2592_SDI_GPIO_Port, LMX2592_SDI_Pin, GPIO_PIN_RESET);
#endif

void Wirte_Data(uint8_t ADDR, uint16_t Data);
void LMX2592_Init(uint8_t osc_2x_en);
void LMX2592_REF_INIT(uint8_t osc_2x_en);
void LMX2592_SET_OSC2X(uint8_t osc_2x_en);
void LMX2592_WRITE_FREQ(double freq, uint8_t acal_en);
void LMX2592_WRITE_FREQ_OSC2X(double freq, uint8_t osc_2x_en, uint8_t acal_en);
void LMX2592_FIND_CHANNEL_DIV(float freq, uint16_t *chdiv_num, uint16_t *vco_doubler, uint16_t *vco_level);
void LMX2592_Wchannel_div(uint16_t chdiv_num);
void LMX2592_SETPOWER(uint16_t channel, uint16_t power);
void LMX2592_SETOUT(uint16_t channel, uint16_t state);
void LMX2592_SETNMODE(uint16_t state);
void LMX2592_TEST100M(void);
void LMX2592_TEST2G(void);

#endif
