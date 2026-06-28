#include "LMX2592.h"

#if SOFT_SPI
#include "gpio.h"
#endif
#include "spi.h"

uint16_t REG46 = 0x1424;
uint16_t REG47 = 0x08D4;
uint16_t REG48 = 0x03FD;

#define LMX2592_PFD_MAX_MHZ	200.0
#define LMX2592_PLL_N_PRE		2.0
#define LMX2592_PLL_DEN			20000000UL
#define LMX2592_VCO_MIN_MHZ		3550U

typedef struct {
	uint8_t valid;
	double fpd_mhz;
	uint16_t reg1;
	uint16_t reg9;
	uint16_t reg10;
	uint16_t reg11;
	uint16_t reg12;
	uint16_t fcal_adj;
} LMX2592_RefConfig;

static uint16_t lmx2592_active_fcal_adj;
static uint8_t lmx2592_ref_initialized;
static uint8_t lmx2592_active_osc_2x;
static double lmx2592_active_fpd_mhz;
static uint8_t lmx2592_ref_path_changed;
static uint8_t lmx2592_vco_cal_valid;
static uint16_t lmx2592_last_vco_cal_band;

void Wirte_Data(uint8_t ADDR, uint16_t Data) {
#if SOFT_SPI
	PLL_CS_SET();
	PLL_SCK_RESET();
	PLL_CS_RESET();

	for(int j = 0; j < 8; j++) { // 8-bit address
		PLL_SCK_RESET();
		HAL_GPIO_WritePin(LMX2592_SDI_GPIO_Port, LMX2592_SDI_Pin, (ADDR >> (7 - j)) & 0x01);
		PLL_SCK_SET();
	}

	for(int j = 0; j < 16; j++) { // 16-bit register data
		PLL_SCK_RESET();
		HAL_GPIO_WritePin(LMX2592_SDI_GPIO_Port, LMX2592_SDI_Pin, (Data >> (15 - j)) & 0x01);
		PLL_SCK_SET();
	}

	PLL_SCK_SET();
	PLL_CS_SET();
#else
	uint32_t spi_send_dat = (ADDR << 16) | Data;
	HAL_SPI_Transmit(&hspi2, (uint8_t *)&spi_send_dat, 1, 0xFFFF);
#endif
}

#if !LMX2592_USE_MANUAL_REF_REGS
static LMX2592_RefConfig lmx2592_ref_cfg_cache[2];

static uint8_t LMX2592_NormalizeOsc2x(uint8_t osc_2x_en) {
	return (osc_2x_en && (LMX2592_REF_CLK <= 200.0)) ? 1 : 0;
}

static uint16_t LMX2592_CalClkDiv(double ref_mhz) {
	uint16_t div = 0;
	double cal_clk = ref_mhz;

	while((cal_clk > 200.0) && (div < 7)) {
		div++;
		cal_clk = ref_mhz / (double)(1U << div);
	}

	return div;
}

static uint16_t LMX2592_FcalAdjust(double fpd_mhz) {
	if(fpd_mhz > 200.0) {
		return 0x0180;
	} else if(fpd_mhz > 150.0) {
		return 0x0100;
	} else if(fpd_mhz > 100.0) {
		return 0x0080;
	} else if(fpd_mhz >= 10.0) {
		return 0x0000;
	} else if(fpd_mhz >= 5.0) {
		return 0x0020;
	} else if(fpd_mhz >= 2.5) {
		return 0x0040;
	}

	return 0x0060;
}
#endif

#if LMX2592_USE_MANUAL_REF_REGS
static double LMX2592_ConfigRefPath(uint8_t osc_2x_en) {
	(void)osc_2x_en;

	Wirte_Data(1, LMX2592_REF_REG1);
	Wirte_Data(9, LMX2592_REF_REG9);
	Wirte_Data(10, LMX2592_REF_REG10);
	Wirte_Data(12, LMX2592_REF_REG12);
	Wirte_Data(11, LMX2592_REF_REG11);
	lmx2592_active_fcal_adj = LMX2592_REF_FCAL_ADJ;

	return LMX2592_REF_FPD_MHZ;
}
#else
static LMX2592_RefConfig *LMX2592_GetRefConfig(uint8_t osc_2x_en) {
	uint16_t pll_r_pre = 1;
	uint16_t pll_r = 1;
	uint16_t mult = 1;
	uint8_t use_osc_2x = LMX2592_NormalizeOsc2x(osc_2x_en);
	LMX2592_RefConfig *cfg = &lmx2592_ref_cfg_cache[use_osc_2x];
	double ref_path_mhz = LMX2592_REF_CLK * (use_osc_2x ? 2.0 : 1.0);
	double fpd_mhz = ref_path_mhz;

	if(cfg->valid) {
		return cfg;
	}

	while((fpd_mhz > LMX2592_PFD_MAX_MHZ) && (pll_r_pre < 4095)) {
		pll_r_pre++;
		fpd_mhz = ref_path_mhz / (double)pll_r_pre;
	}

	cfg->reg1 = 0x0808 | LMX2592_CalClkDiv(LMX2592_REF_CLK);
	cfg->reg9 = 0x0302 | (use_osc_2x ? 0x0800 : 0x0000);
	cfg->reg10 = 0x1058 | ((mult & 0x1F) << 7);
	cfg->reg12 = 0x7000 | (pll_r_pre & 0x0FFF);
	cfg->reg11 = 0x0008 | ((pll_r & 0x00FF) << 4);
	cfg->fpd_mhz = fpd_mhz;
	cfg->fcal_adj = LMX2592_FcalAdjust(fpd_mhz);
	cfg->valid = 1;

	return cfg;
}

static double LMX2592_ConfigRefPath(uint8_t osc_2x_en) {
	LMX2592_RefConfig *cfg = LMX2592_GetRefConfig(osc_2x_en);

	Wirte_Data(1, cfg->reg1);
	Wirte_Data(9, cfg->reg9);
	Wirte_Data(10, cfg->reg10);
	Wirte_Data(12, cfg->reg12);
	Wirte_Data(11, cfg->reg11);
	lmx2592_active_fcal_adj = cfg->fcal_adj;

	return cfg->fpd_mhz;
}
#endif

static double LMX2592_ApplyRefPath(uint8_t osc_2x_en, uint8_t force) {
	lmx2592_ref_path_changed = 0;

#if LMX2592_USE_MANUAL_REF_REGS
	uint8_t use_osc_2x = osc_2x_en ? 1 : 0;

	if(force || !lmx2592_ref_initialized || (use_osc_2x != lmx2592_active_osc_2x)) {
		Wirte_Data(0, 0x2206);
		lmx2592_active_fpd_mhz = LMX2592_ConfigRefPath(osc_2x_en);
		lmx2592_active_osc_2x = use_osc_2x;
		lmx2592_ref_initialized = 1;
		lmx2592_ref_path_changed = 1;
		lmx2592_vco_cal_valid = 0;
	}
#else
	uint8_t use_osc_2x = LMX2592_NormalizeOsc2x(osc_2x_en);

	if(force || !lmx2592_ref_initialized || (use_osc_2x != lmx2592_active_osc_2x)) {
		Wirte_Data(0, 0x2206);
		lmx2592_active_fpd_mhz = LMX2592_ConfigRefPath(use_osc_2x);
		lmx2592_active_osc_2x = use_osc_2x;
		lmx2592_ref_initialized = 1;
		lmx2592_ref_path_changed = 1;
		lmx2592_vco_cal_valid = 0;
	}
#endif

	return lmx2592_active_fpd_mhz;
}

static uint16_t LMX2592_GetVcoCalBand(double vco_mhz) {
	uint32_t vco_mhz_i = (uint32_t)(vco_mhz + 0.5);

	if(vco_mhz_i <= LMX2592_VCO_MIN_MHZ) {
		return 0;
	}

	return (uint16_t)((vco_mhz_i - LMX2592_VCO_MIN_MHZ) / LMX2592_VCO_CAL_BAND_MHZ);
}

static uint8_t LMX2592_NeedVcoCalibration(uint16_t vco_band) {
	if(!lmx2592_vco_cal_valid) {
		return 1;
	} else if(lmx2592_ref_path_changed) {
		return 1;
	}

	return (vco_band != lmx2592_last_vco_cal_band);
}

static void LMX2592_WriteNDivider(double vco_mhz, double fpd_mhz) {
	double n_div = vco_mhz / (fpd_mhz * LMX2592_PLL_N_PRE);
	uint16_t n_integer = (uint16_t)n_div;
	double n_fraction = n_div - (double)n_integer;
	uint32_t n_num = (uint32_t)(n_fraction * (double)LMX2592_PLL_DEN + 0.5);

	if(n_num >= LMX2592_PLL_DEN) {
		n_integer++;
		n_num = 0;
	}

#if LMX2592_DEBUG
	printf("Fpd = %f MHz; N_integer = %d; N_fraction = %f\n", fpd_mhz, n_integer, n_fraction);
#endif

	Wirte_Data(37, 0x4000);
	Wirte_Data(38, (n_integer << 1) & 0x1FFE);
	Wirte_Data(44, (uint16_t)(n_num >> 16));
	Wirte_Data(45, (uint16_t)n_num);
	Wirte_Data(40, (uint16_t)(LMX2592_PLL_DEN >> 16));
	Wirte_Data(41, (uint16_t)LMX2592_PLL_DEN);

#if LMX2592_DEBUG
	printf("N numerator = 0x%04hx%04hx\n", (uint16_t)(n_num >> 16), (uint16_t)n_num);
#endif
}

static void LMX2592_StartWork(uint8_t acal_en) {
	if(acal_en) {
		Wirte_Data(0, 0x2214 | lmx2592_active_fcal_adj);
		HAL_Delay(1);
		Wirte_Data(0, 0x221C | lmx2592_active_fcal_adj);
		HAL_Delay(10);
	} else {
		Wirte_Data(0, 0x220C | lmx2592_active_fcal_adj);
	}
}

static void LMX2592_WriteFreqCommon(double freq, uint8_t osc_2x_en, uint8_t test_outputs, uint8_t acal_en) {
	uint16_t chdiv_num;
	uint16_t vco_doubler;
	uint16_t vco_level;
	uint16_t vco_band;
	uint8_t run_acal;
	double fpd_mhz;
	double vco_mhz;

	LMX2592_FIND_CHANNEL_DIV(freq, &chdiv_num, &vco_doubler, &vco_level);

	if((chdiv_num == 1) && (vco_doubler == 1)) {
		vco_mhz = freq / 2.0;
	} else if(chdiv_num == 1) {
		vco_mhz = freq;
	} else {
		vco_mhz = freq * (double)chdiv_num;
	}

	fpd_mhz = LMX2592_ApplyRefPath(osc_2x_en, 0);
	vco_band = LMX2592_GetVcoCalBand(vco_mhz);
	run_acal = acal_en || LMX2592_NeedVcoCalibration(vco_band);

	Wirte_Data(14, 0x0841);

	if(chdiv_num == 1) {
		if(vco_doubler == 1) {
			Wirte_Data(30, 0x0135);
		} else if(vco_level) {
			Wirte_Data(30, 0x01F4);
		} else {
			Wirte_Data(30, 0x0134);
		}

		Wirte_Data(35, 0x0019);
		Wirte_Data(34, 0xC3CA);
		Wirte_Data(31, 0x0081);
		Wirte_Data(36, 0x0000);
		Wirte_Data(46, REG46);
		REG47 = (REG47 & 0xF7FF) | 0x0800;
		Wirte_Data(47, REG47);
		REG48 = (REG48 & 0xFFFE) | 0x0001;
		Wirte_Data(48, REG48);
	} else {
		Wirte_Data(30, 0x0034);
		Wirte_Data(31, 0x0601);
		Wirte_Data(34, 0xC3EA);
		Wirte_Data(46, REG46);
		REG47 = (REG47 & 0xF7FF);
		Wirte_Data(47, REG47);
		REG48 = (REG48 & 0xFFFE);
		Wirte_Data(48, REG48);
		LMX2592_Wchannel_div(chdiv_num);
	}

	LMX2592_WriteNDivider(vco_mhz, fpd_mhz);

	if(test_outputs) { // Use fixed output state for test helper functions.
		REG46 = 0x0F20;
		REG47 = 0x00CF;
		REG48 = 0x03FC;
		Wirte_Data(46, REG46);
		Wirte_Data(47, REG47);
		Wirte_Data(48, REG48);
		Wirte_Data(59, 0x0020);
		Wirte_Data(39, 0x8104);
	}

	LMX2592_StartWork(run_acal);

	if(run_acal) {
		lmx2592_last_vco_cal_band = vco_band;
		lmx2592_vco_cal_valid = 1;
	}

#if LMX2592_DEBUG
	printf("chdiv_num = %d; vco_doubler = %d; vco_level = %d; acal = %d\n",
		chdiv_num,
		vco_doubler,
		vco_level,
		run_acal);
#endif
}

void LMX2592_Init(uint8_t osc_2x_en) {
#if SOFT_SPI
	PLL_CS_SET();
	PLL_SCK_RESET();
	PLL_SDI_RESET();
#endif
	LMX2592_REF_INIT(osc_2x_en);
	LMX2592_SETOUT(0, 0); // Disable output A.
	LMX2592_SETOUT(1, 0); // Disable output B.
	LMX2592_SETPOWER(0, 0);
	LMX2592_SETPOWER(1, 0);
}

void LMX2592_REF_INIT(uint8_t osc_2x_en) {
	(void)LMX2592_ApplyRefPath(osc_2x_en, 1);
}

void LMX2592_SET_OSC2X(uint8_t osc_2x_en) {
	(void)LMX2592_ApplyRefPath(osc_2x_en, 0);
}

void LMX2592_WRITE_FREQ(double freq, uint8_t acal_en) {
	LMX2592_WriteFreqCommon(freq, lmx2592_active_osc_2x, 0, acal_en);
}

void LMX2592_WRITE_FREQ_OSC2X(double freq, uint8_t osc_2x_en, uint8_t acal_en) {
	LMX2592_WriteFreqCommon(freq, osc_2x_en, 0, acal_en);
}

void LMX2592_FIND_CHANNEL_DIV(float freq, uint16_t *chdiv_num, uint16_t *vco_doubler, uint16_t *vco_level) {
	if(freq > 9800) {
#if LMX2592_DEBUG
		printf("FREQ is too high!\n");
#endif
	} else if(freq > 7100) {
		*chdiv_num = 1;
		*vco_doubler = 1;
		*vco_level = 0;
	} else if(freq > 6500) {
		*chdiv_num = 1;
		*vco_doubler = 0;
		*vco_level = 1;
	} else if(freq > 3550) {
		*chdiv_num = 1;
		*vco_doubler = 0;
		*vco_level = 0;
	} else if(freq > 1775) {
		*chdiv_num = 2;
		*vco_doubler = 0;
		*vco_level = 0;
	} else if(freq > 1184) {
		*chdiv_num = 3;
		*vco_doubler = 0;
		*vco_level = 0;
	} else if(freq > 888) {
		*chdiv_num = 4;
		*vco_doubler = 0;
		*vco_level = 0;
	} else if(freq > 592) {
		*chdiv_num = 6;
		*vco_doubler = 0;
		*vco_level = 0;
	} else if(freq > 444) {
		*chdiv_num = 8;
		*vco_doubler = 0;
		*vco_level = 0;
	} else if(freq > 296) {
		*chdiv_num = 12;
		*vco_doubler = 0;
		*vco_level = 0;
	} else if(freq > 222) {
		*chdiv_num = 16;
		*vco_doubler = 0;
		*vco_level = 0;
	} else if(freq > 148) {
		*chdiv_num = 24;
		*vco_doubler = 0;
		*vco_level = 0;
	} else if(freq > 111) {
		*chdiv_num = 32;
		*vco_doubler = 0;
		*vco_level = 0;
	} else if(freq > 99) {
		*chdiv_num = 36;
		*vco_doubler = 0;
		*vco_level = 0;
	} else if(freq > 74) {
		*chdiv_num = 48;
		*vco_doubler = 0;
		*vco_level = 0;
	} else if(freq > 56) {
		*chdiv_num = 64;
		*vco_doubler = 0;
		*vco_level = 0;
	} else if(freq > 37) {
		*chdiv_num = 96;
		*vco_doubler = 0;
		*vco_level = 0;
	} else if(freq > 28) {
		*chdiv_num = 128;
		*vco_doubler = 0;
		*vco_level = 0;
	} else if(freq >= 20) {
		*chdiv_num = 192;
		*vco_doubler = 0;
		*vco_level = 0;
	} else {
#if LMX2592_DEBUG
		printf("FREQ is too low!\n");
#endif
	}
}

void LMX2592_Wchannel_div(uint16_t chdiv_num) {
	switch(chdiv_num) {
		case 2:
			Wirte_Data(35, 0x001B);
			Wirte_Data(36, 0x0C10);
			break;

		case 3:
			Wirte_Data(35, 0x001D); // TI datasheet has an error in this REG35 value.
			Wirte_Data(36, 0x0C10);
			break;

		case 4:
			Wirte_Data(35, 0x029B);
			Wirte_Data(36, 0x0C20);
			break;

		case 6:
			Wirte_Data(35, 0x029D);
			Wirte_Data(36, 0x0C20);
			break;

		case 8:
			Wirte_Data(35, 0x049B);
			Wirte_Data(36, 0x0C20);
			break;

		case 12:
			Wirte_Data(35, 0x049D);
			Wirte_Data(36, 0x0C20);
			break;

		case 16:
			Wirte_Data(35, 0x109B);
			Wirte_Data(36, 0x0C20);
			break;

		case 24:
			Wirte_Data(35, 0x109D);
			Wirte_Data(36, 0x0C20);
			break;

		case 32:
			Wirte_Data(35, 0x119B);
			Wirte_Data(36, 0x0C41);
			break;

		case 36:
			Wirte_Data(35, 0x099D);
			Wirte_Data(36, 0x0C41);
			break;

		case 48:
			Wirte_Data(35, 0x119D);
			Wirte_Data(36, 0x0C41);
			break;

		case 64:
			Wirte_Data(35, 0x119B);
			Wirte_Data(36, 0x0C42);
			break;

		case 96:
			Wirte_Data(35, 0x119B);
			Wirte_Data(36, 0x0C44);
			break;

		case 128:
			Wirte_Data(35, 0x119B);
			Wirte_Data(36, 0x0C48);
			break;

		case 192:
			Wirte_Data(35, 0x119D);
			Wirte_Data(36, 0x0C48);
			break;

		default:
			Wirte_Data(35, 0x119B);
			Wirte_Data(36, 0x0C48);
			break;
	}
}

void LMX2592_SETPOWER(uint16_t channel, uint16_t power) {
	if(channel == 0) { // Channel A
		if(power > 63) {
#if LMX2592_DEBUG
			printf("power limit\n");
#endif
		} else {
			power = (power << 8) & 0x3F00;
			REG46 = (REG46 & 0xC0FF) | power;
			Wirte_Data(46, REG46);
		}
	} else if(channel == 1) { // Channel B
		if(power > 63) {
#if LMX2592_DEBUG
			printf("power limit\n");
#endif
		} else {
			power = power & 0x003F;
			REG47 = (REG47 & 0xFFC0) | power;
			Wirte_Data(47, REG47);
		}
	}

#if LMX2592_DEBUG
	printf("REG46=0x%hx, REG47=0x%hx\n", REG46, REG47);
#endif
}

void LMX2592_SETOUT(uint16_t channel, uint16_t state) {
	if(channel == 0) { // Channel A
		if(state) {
			REG47 = (REG47 & 0xEFFF);
		} else {
			REG47 = (REG47 & 0xEFFF) | 0x1000;
		}

		Wirte_Data(47, REG47);
	} else if(channel == 1) { // Channel B
		if(state) {
			REG48 = (REG48 & 0xFFFD);
		} else {
			REG48 = (REG48 & 0xFFFD) | 0x0002;
		}

		Wirte_Data(48, REG48);
	}
}

void LMX2592_SETNMODE(uint16_t state) {
	switch(state) {
		case 0: // Integer mode
			REG46 = REG46 & 0xFFF8;
			Wirte_Data(46, REG46);
			Wirte_Data(39, 0x8104);
			break;

		case 1: // First-order MASH
			REG46 = REG46 & 0xFFF9;
			Wirte_Data(46, REG46);
			Wirte_Data(39, 0x8104);
			break;

		case 2: // Second-order MASH
			REG46 = REG46 & 0xFFFA;
			Wirte_Data(46, REG46);
			Wirte_Data(39, 0x8204);
			break;

		case 3: // Third-order MASH
			REG46 = REG46 & 0xFFFB;
			Wirte_Data(46, REG46);
			Wirte_Data(39, 0x8204);
			break;

		case 4: // Fourth-order MASH
			REG46 = REG46 & 0xFFFC;
			Wirte_Data(46, REG46);
			Wirte_Data(39, 0x8804);
			break;

		default:
			REG46 = REG46 & 0xFFF8;
			Wirte_Data(46, REG46);
			Wirte_Data(39, 0x8104);
			break;
	}
}

void LMX2592_TEST100M(void) {
	LMX2592_WriteFreqCommon(100.0, lmx2592_active_osc_2x, 1, 1);
}

void LMX2592_TEST2G(void) {
	LMX2592_WriteFreqCommon(2000.0, lmx2592_active_osc_2x, 1, 1);
}
