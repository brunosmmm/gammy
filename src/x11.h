/**
 * Copyright (C) 2019 Francesco Fusco. All rights reserved.
 * License: https://github.com/Fushko/gammy#license
 */

#ifndef X11_H
#define X11_H

#include <xcb/xcb.h>
#include <cstdint>
#include <vector>

class X11
{

  xcb_connection_t *dsp;
  xcb_screen_t *scr;
  xcb_window_t root;

	int ramp_sz;
	int scr_num;
  int crtc_num;

	std::vector<uint16_t> init_ramp;
	bool initial_ramp_exists = true;

	unsigned w, h;

	void fillRamp(std::vector<uint16_t> &ramp, const int brightness, const int temp);
  xcb_screen_t *screenOfDisplay(int screen);

	public:
	X11();

	uint32_t getWidth();
	uint32_t getHeight();

	void getX11Snapshot(std::vector<uint8_t> &buf) noexcept;
	void setGamma(int scrBr, int temp);
  void setGamma(int temp);
	void setInitialGamma(bool set_previous);

	~X11();
};

#endif // X11_H
