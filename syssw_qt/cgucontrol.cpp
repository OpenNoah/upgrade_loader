#include <stdlib.h>
#include "cgucontrol.h"
#include "regs.h"

#define	MHZ		1000000
#define EXT_CLK		(12 * MHZ)

#define MAX_PLL_CLK	(500 * MHZ)
#define MAX_MCLK	(133 * MHZ)
#define MAX_LCD_CLK	(150 * MHZ)
#define USB_CLK		(48 * MHZ)
#define MSC_CLK		(25 * MHZ)
#define SDRAM_REFRESH	(8192 * 1000 / 64)	// 8192 refresh cycles / 64ms

#define CGU_BASE	0x10000000
#define LCD_BASE	0x13050000
#define SDRAM_BASE	0x13010000

#define DELTA		0.0001

#define _I	volatile
#define _O	volatile
#define _IO	volatile

#pragma packed(push, 1)
struct cgu_t {
	_IO uint32_t CPCCR;
	uint32_t _RESERVED0[3];
	_IO uint32_t CPPCR;
	uint32_t _RESERVED1[19];
	_IO uint32_t I2SCDR;
	_IO uint32_t LPCDR;
	_IO uint32_t MSCCDR;
	_IO uint32_t UHCCDR;
	uint32_t _RESERVED2[1];
	_IO uint32_t SSICDR;
};

struct lcd_t {
	uint32_t LCDCFG;
	uint32_t LCDVSYNC;
	uint32_t LCDHSYNC;
	uint32_t LCDVAT;
	uint32_t LCDDAH;
	uint32_t LCDDAV;
	// TODO ...more
};

struct sdram_t {
	uint32_t DMCR;
	uint16_t RTCSR;
	uint16_t _RESERVED0[1];
	uint16_t RTCNT;
	uint16_t _RESERVED1[1];
	uint16_t RTCOR;
	uint16_t _RESERVED2[1];
	uint32_t DMAR;
	uint8_t  SDMR;
	uint8_t  _RESERVED3[3];
};
#pragma packed(pop)

const int CGUControl::info_t::clk_div_lut[16] = {1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 0};

CGUControl::CGUControl(QWidget *parent) : QWidget(parent)
{
	info.ext_clk = EXT_CLK;
	updateValues();

	QGridLayout *glay = new QGridLayout(this, 5, 6, 0, 4);

	QHBoxLayout *play = new QHBoxLayout(4);
	glay->addMultiCellLayout(play, 0, 0, 0, 5);
	play->addWidget(disp.pll_lbl = new QLabel("PLL", this));
	play->addWidget(disp.pll_clk = new QLabel(this));
	play->addWidget(disp.pll_step = new QSlider(0, 15, 1, 1, Qt::Horizontal, this));
	disp.pll_step->setTickmarks(QSlider::Below);
	connect(disp.pll_step, SIGNAL(valueChanged(int)), this, SLOT(pllStepChanged(int)));

	glay->addWidget(disp.c_lbl = new QLabel("CPU", this), 1, 0);
	glay->addWidget(disp.c_clk = new QLabel(this), 1, 1);
	glay->addWidget(disp.c_div = new QSlider(-9, 0, 1, 1, Qt::Horizontal, this), 1, 2);
	disp.c_div->setTickmarks(QSlider::Below);
	connect(disp.c_div, SIGNAL(valueChanged(int)), this, SLOT(cDivChanged(int)));

	glay->addWidget(disp.h_lbl = new QLabel("HCLK", this), 1, 3);
	glay->addWidget(disp.h_clk = new QLabel(this), 1, 4);
	glay->addWidget(disp.h_div = new QSlider(-9, 0, 1, 1, Qt::Horizontal, this), 1, 5);
	disp.h_div->setTickmarks(QSlider::Below);
	connect(disp.h_div, SIGNAL(valueChanged(int)), this, SLOT(hDivChanged(int)));

	glay->addWidget(disp.p_lbl = new QLabel("PCLK", this), 2, 0);
	glay->addWidget(disp.p_clk = new QLabel(this), 2, 1);
	glay->addWidget(disp.p_div = new QSlider(-9, 0, 1, 1, Qt::Horizontal, this), 2, 2);
	disp.p_div->setTickmarks(QSlider::Below);
	connect(disp.p_div, SIGNAL(valueChanged(int)), this, SLOT(pDivChanged(int)));

	glay->addWidget(disp.m_lbl = new QLabel("MCLK", this), 2, 3);
	glay->addWidget(disp.m_clk = new QLabel(this), 2, 4);
	glay->addWidget(disp.m_div = new QSlider(-9, 0, 1, 1, Qt::Horizontal, this), 2, 5);
	disp.m_div->setTickmarks(QSlider::Below);
	connect(disp.m_div, SIGNAL(valueChanged(int)), this, SLOT(mDivChanged(int)));

	glay->addMultiCellWidget(disp.warning = new QListBox(this), 3, 3, 0, 5);

	QHBoxLayout *blay = new QHBoxLayout;
	glay->addMultiCellLayout(blay, 4, 4, 0, 5);
	blay->addWidget(disp.test = new QPushButton(tr("性能测试"), this));
	connect(disp.test, SIGNAL(clicked()), this, SLOT(test()));
	blay->addWidget(disp.apply = new QPushButton(tr("应用"), this));
	connect(disp.apply, SIGNAL(clicked()), this, SLOT(apply()));

	glay->setColStretch(2, 1);

	updateDisplay();
	updateControls();
}

void CGUControl::updateValues()
{
	cgu_t *cgu = (cgu_t *)(*regs)(CGU_BASE);

	// Calculate current PLL frequency
	float pll_clk = info.ext_clk;
	info.pll_on = ((cgu->CPPCR & (1 << 9) /* PLLBP */) == 0) && ((cgu->CPPCR & (1 << 8) /* PLLEN */) != 0);
	if (info.pll_on) {
		// PLL enabled
		pll_clk *= ((cgu->CPPCR >> 23) & 0x1ff /* PLLM */) + 2;
		pll_clk /= ((cgu->CPPCR >> 18) & 0x1f /* PLLN */) + 2;
		const int pll_od_lut[4] = {1, 2, 2, 4};
		pll_clk /= pll_od_lut[(cgu->CPPCR >> 16) & 0x3 /* PLLOD */];
	}
	info.pll_clk = pll_clk;
	// PLL clock frequency is limited to multiples of 48M, required by UHC USB
	info.pll_step = int(round(pll_clk / USB_CLK)) - 1;

	// Main clocks
	info.c_div = cgu->CPCCR & 0xf /* CDIV */;
	info.h_div = (cgu->CPCCR >> 4) & 0xf /* HDIV */;
	info.p_div = (cgu->CPCCR >> 8) & 0xf /* PDIV */;
	info.m_div = (cgu->CPCCR >> 12) & 0xf /* MDIV */;

	// Peripheral clocks
	info.uhc_div = cgu->UHCCDR & 0xf /* UHCCDR */;

	// Update current backup values
	bak = info;
}

void CGUControl::apply()
{
	cgu_t *cgu = (cgu_t *)(*regs)(CGU_BASE);
	lcd_t *lcd = (lcd_t *)(*regs)(LCD_BASE);
	sdram_t *sdram = (sdram_t *)(*regs)(SDRAM_BASE);

	// Calculate various clock configs
	cgu_t vcgu;

	unsigned int pll_clk = USB_CLK * (info.pll_step + 1);
	unsigned int msc_div = (pll_clk + MSC_CLK - 1) / MSC_CLK - 1;

	// Calculate LCD clock rate from LCD size and type
	unsigned int lcd_ht = (lcd->LCDVAT >> 16) & 0x7ff /* HT */;
	unsigned int lcd_vt = lcd->LCDVAT & 0x7ff /* VT */;
	unsigned int lcd_mode = lcd->LCDCFG & 0xf /* MDOE */;
	unsigned int lcd_tpp = 2;	// Transfers per pixel
	switch (lcd_mode) {
	case 0xc:	// 8-bit Serial TFT
		lcd_tpp = 3; break;
	case 0x0:	// Generic 16-bit/18-bit Parallel TFT Panel
	case 0x1:	// Special TFT Panel Mode1
	case 0x2:	// Special TFT Panel Mode2
	case 0x3:	// Special TFT Panel Mode3
		lcd_tpp = 2; break;
	case 0x4:	// Non-Interlaced CCIR656
	case 0x6:	// Interlaced CCIR656
	case 0x8:	// Single-Color STN Panel
	case 0x9:	// Single-Monochrome STN Panel
	case 0xa:	// Dual-Color STN Panel
	case 0xb:	// Dual-Monochrome STN Panel
	default:	// Reserved
		QMessageBox::warning(this, tr("频率计算"), tr("未知 LCD 类型 %1").arg(lcd_mode));
		lcd_tpp = 2; break;	// Select medium value, probably fine
	}

	unsigned int lcd_pixclk	= 60 * lcd_ht * lcd_vt * lcd_tpp;	// Assuming 60 fps
	unsigned int lcd_div	= pll_clk / lcd_pixclk - 1;
	unsigned int l_div	= (pll_clk + MAX_LCD_CLK - 1) / MAX_LCD_CLK - 1;

	// Update main clock dividers
	// I2S using external clock
	// CKO output enabled
	// USB clock using external clock
	// Delay clock divider change
	// No PLL clock divider for peripheral clocks
	vcgu.CPCCR = 0x40200000 | (l_div << 16) | (info.m_div << 12) | (info.p_div << 8) | (info.h_div << 4) | info.c_div;
	// No change on I2S using external clock
	vcgu.I2SCDR = cgu->I2SCDR;
	// Update LCD pixel clock
	// If using external clock source, don't change
	vcgu.LPCDR = (cgu->LPCDR & (1 << 31) /* LCS */) == 0 ? lcd_div : cgu->LPCDR;
	// Update MSC clock
	vcgu.MSCCDR = msc_div;
	// Update UHC USB clock
	vcgu.UHCCDR = info.pll_step;
	// SSI using external clock, no change
	vcgu.SSICDR = cgu->SSICDR;
	// Update PLL multiplier and dividers
	// PLLN = 2 - 2
	// PLLM = USB_CLK * (info.pll_step + 1) / (EXT_CLK / 2) - 2
	// PLL not bypassed
	// PLL enabled
	// Keep PLL stabilise time unchanged
	vcgu.CPPCR = (0 << 18 /* PLLN */) | ((pll_clk / (EXT_CLK / 2) - 2) << 23 /* PLLM */) |
			(1 << 8 /* PLLEN */) | (cgu->CPPCR & 0xff);

	// Apply values
	// Update divider and PLL first
	cgu->CPCCR  = vcgu.CPCCR;
	cgu->CPPCR  = vcgu.CPPCR;
	cgu->MSCCDR = vcgu.MSCCDR;
	cgu->UHCCDR = vcgu.UHCCDR;
	cgu->LPCDR  = vcgu.LPCDR;
	cgu->I2SCDR = vcgu.I2SCDR;
	cgu->SSICDR = vcgu.SSICDR;
	// Wait for PLL to become stable
	while ((cgu->CPPCR & (1 << 10) /* PLLS */) == 0);
	// Update divider value now
	cgu->CPCCR  = vcgu.CPCCR | (1 << 22 /* CE */);

	// TODO Set MSC clock rate register (MSC_CLKRT.CLK_RATE) to 0
	// TODO SDRAM registers won't update?

	// Finished, refresh values
	updateValues();
	updateDisplay();
	updateControls();

	// Status report
	float lcd_fps = float(pll_clk) / (lcd_div + 1) / lcd_ht / lcd_vt / lcd_tpp;
	float msc_clk = float(pll_clk) / (msc_div + 1);
	QMessageBox::information(this, tr("频率设定"), tr("频率设定已应用\nCPU 频率: %1 MHz\n"
		"储存卡时钟: %2 MHz\n屏幕刷新率: %3 Hz")
		.arg(info.c_clk() / MHZ, 0, 'f', 2).arg(msc_clk / MHZ, 0, 'f', 2)
		.arg(lcd_fps, 0, 'f', 2));
}

void CGUControl::test()
{
	disp.test->setEnabled(false);
	disp.test->setText(tr("性能测试中..."));
	qApp->processEvents(0);

	QTime t;
	t.start();

	// 16MiB test pattern
	const unsigned int pat_size = 16 * 1024 * 1024;
	uint16_t *pattern = new uint16_t[pat_size / 2];
	if (!pattern) {
		QMessageBox::critical(this, tr("性能测试"), tr("内存分配失败"));
		disp.test->setText(tr("性能测试"));
		disp.test->setEnabled(true);
		return;
	}
	srand(12345);
	for (unsigned int i = 0; i < pat_size / 2; i++)
		pattern[i] = rand();

	// CRC calculation
	const unsigned int crc_init = 0xffff;
	const unsigned int crc_poly = 0x1021;

	union {
		uint16_t crc;
		struct PACKED {
			uint8_t low;
			uint8_t high;
		} f;
	} crc;

	// crc16_init()
	crc.crc = crc_init;

	uint8_t *p = (uint8_t *)pattern;
	for (unsigned int i = 0; i < pat_size; i++) {
		// crc16_update(uint8_t v)
		uint8_t v = p[i];
		crc.f.high ^= v;
		for (uint8_t i = 0; i < 8; i++) {
			uint8_t x = crc.f.high & 0x80;
			crc.crc <<= 1;
			if (x)
				crc.crc ^= crc_poly;
		}
	}

	// crc16_finish()
	int elapsed = t.elapsed();
	delete [] pattern;
	if (crc.crc == 0x474f)
		QMessageBox::information(this, tr("性能测试"), tr("测试完成\n耗时 %1 毫秒").arg(elapsed));
	else
		QMessageBox::warning(this, tr("性能测试"), tr("计算错误\n耗时 %1 毫秒").arg(elapsed));
	disp.test->setText(tr("性能测试"));
	disp.test->setEnabled(true);
}

void CGUControl::updateDisplay()
{
	disp.pll_clk->setText(QString("%1 MHz").arg(info.pll_clk / MHZ, 0, 'f', 1));
	disp.c_clk->setText(QString("%1").arg(info.c_clk() / MHZ, 0, 'f', 1));
	disp.h_clk->setText(QString("%1").arg(info.h_clk() / MHZ, 0, 'f', 1));
	disp.p_clk->setText(QString("%1").arg(info.p_clk() / MHZ, 0, 'f', 1));
	disp.m_clk->setText(QString("%1").arg(info.m_clk() / MHZ, 0, 'f', 1));
	validate();
}

void CGUControl::updateControls()
{
	disp.pll_step->setValue(info.pll_step);
	disp.c_div->setValue(-info.c_div);
	disp.h_div->setValue(-info.h_div);
	disp.p_div->setValue(-info.p_div);
	disp.m_div->setValue(-info.m_div);
}

void CGUControl::validate()
{
	info.warn = 0;
	info.err = 0;
	disp.pll_lbl->setBackgroundMode(QWidget::PaletteBackground);
	disp.c_lbl->setBackgroundMode(QWidget::PaletteBackground);
	disp.h_lbl->setBackgroundMode(QWidget::PaletteBackground);
	disp.p_lbl->setBackgroundMode(QWidget::PaletteBackground);
	disp.m_lbl->setBackgroundMode(QWidget::PaletteBackground);
	disp.warning->clear();
	QStringList str_err, str_warn;

	// Maximum PLL frequency
	if (info.pll_clk > MAX_PLL_CLK) {
		info.warn = 1;
		disp.pll_lbl->setBackgroundColor(Qt::yellow);
		str_warn << tr("PLL 频率上限 %1 MHz").arg(MAX_PLL_CLK / MHZ);
	}
	// Maximum MCLK frequency
	if (info.m_clk() > MAX_MCLK) {
		info.warn = 1;
		disp.m_lbl->setBackgroundColor(Qt::yellow);
		str_warn << tr("MCLK 频率上限 %1 MHz").arg(MAX_MCLK / MHZ);
	}
	// CCLK must be integral multiple of HCLK
	float ratio = info.c_clk() / info.h_clk();
	if (fabs(ratio - round(ratio)) >= DELTA) {
		info.err = 1;
		disp.c_lbl->setBackgroundColor(Qt::red);
		disp.h_lbl->setBackgroundColor(Qt::red);
		str_err << tr("CCLK 必须是 HCLK 的整数倍");
	}
	// Frequency ratio of CCLK and HCLK cannot be 24 or 32
	int iratio = int(round(ratio));
	if (iratio == 24 || iratio == 32) {
		info.err = 1;
		disp.c_lbl->setBackgroundColor(Qt::red);
		disp.h_lbl->setBackgroundColor(Qt::red);
		str_err << tr("CCLK 和 HCLK 的比例不能是 24 或 32");
	}
	// HCLK must be equal to or twice of MCLK
	ratio = info.h_clk() / info.m_clk();
	if (fabs(ratio - 1) >= DELTA && fabs(ratio - 2) >= DELTA) {
		info.err = 1;
		disp.h_lbl->setBackgroundColor(Qt::red);
		disp.m_lbl->setBackgroundColor(Qt::red);
		str_err << tr("HCLK 必须是 MCLK 的整数倍");
	}
	// HCLK must be integral multiple of PCLK
	ratio = info.h_clk() / info.p_clk();
	if (fabs(ratio - round(ratio)) >= DELTA) {
		info.err = 1;
		disp.h_lbl->setBackgroundColor(Qt::red);
		disp.p_lbl->setBackgroundColor(Qt::red);
		str_err << tr("HCLK 必须是 PCLK 的整数倍");
	}
	// MCLK must be integral multiple of PCLK
	ratio = info.m_clk() / info.p_clk();
	if (fabs(ratio - round(ratio)) >= DELTA) {
		info.err = 1;
		disp.m_lbl->setBackgroundColor(Qt::red);
		disp.p_lbl->setBackgroundColor(Qt::red);
		str_err << tr("MCLK 必须是 PCLK 的整数倍");
	}

	disp.warning->insertStringList(str_err);
	disp.warning->insertStringList(str_warn);
	disp.apply->setEnabled(!info.err);
}

void CGUControl::pllStepChanged(int v)
{
	info.pll_step = v;
	info.pll_clk = USB_CLK * (info.pll_step + 1);
	updateDisplay();
}

void CGUControl::cDivChanged(int v)
{
	info.c_div = -v;
	updateDisplay();
}

void CGUControl::pDivChanged(int v)
{
	info.p_div = -v;
	updateDisplay();
}

void CGUControl::hDivChanged(int v)
{
	info.h_div = -v;
	updateDisplay();
}

void CGUControl::mDivChanged(int v)
{
	info.m_div = -v;
	updateDisplay();
}
