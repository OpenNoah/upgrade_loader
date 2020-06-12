#pragma once

#include <qt.h>

class CGUControl: public QWidget
{
	Q_OBJECT
public:
	CGUControl(QWidget *parent = 0);

private slots:
	void pllStepChanged(int v);
	void cDivChanged(int v);
	void pDivChanged(int v);
	void hDivChanged(int v);
	void mDivChanged(int v);
	void apply();
	void test();

private:
	void updateValues();
	void updateDisplay();
	void updateControls();
	void validate();

	struct {
		QLabel *pll_lbl, *pll_clk;
		QSlider *pll_step;
		QLabel *c_lbl, *p_lbl, *h_lbl, *m_lbl;
		QLabel *c_clk, *p_clk, *h_clk, *m_clk;
		QSlider *c_div, *p_div, *h_div, *m_div;
		QListBox *warning;

		QPushButton *test, *apply;
	} disp;

	struct info_t {
		static const int clk_div_lut[16];
		bool pll_on;
		unsigned int pll_step;
		unsigned int pll_m, pll_n, pll_od;
		float ext_clk, pll_clk;
		unsigned int c_div, p_div, h_div, m_div; 
		float ld_clk, lp_clk, i2s_clk, msc_clk, usb_clk, ssi_clk;
		unsigned int uhc_div;

		int warn, err;

		float pll_step_clk() {return 48*1000000 * pll_step;}	// UHC USB clock must be 48MHz
		float c_clk() {return pll_clk / clk_div_lut[c_div];}
		float p_clk() {return pll_clk / clk_div_lut[p_div];}
		float h_clk() {return pll_clk / clk_div_lut[h_div];}
		float m_clk() {return pll_clk / clk_div_lut[m_div];}
	} info, bak;
};
