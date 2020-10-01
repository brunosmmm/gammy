/**
 * Copyright (C) 2019 Francesco Fusco. All rights reserved.
 * License: https://github.com/Fushko/gammy#license
 */

#include "x11.h"
#include <iostream>
#include <cstring>
#include <xcb/randr.h>
#include "utils.h"
#include "defs.h"
#include <algorithm>
#include <cmath>

X11::X11()
{

  LOGD << "Initializing display...";
  dsp = xcb_connect(NULL, &scr_num);

  scr = screenOfDisplay(scr_num);
  if (scr) {
    root = scr->root;
  } else {
    LOGE << "Could not determine screen";
    exit(EXIT_FAILURE);
  }

	LOGD << "display initialized on screen " << scr_num;

	w = uint32_t(scr->width_in_pixels);
	h = uint32_t(scr->height_in_pixels);

  xcb_randr_get_screen_resources_cookie_t scrResCookie =
    xcb_randr_get_screen_resources(dsp, root);
  xcb_randr_get_screen_resources_reply_t *scrResReply =
    xcb_randr_get_screen_resources_reply(dsp , scrResCookie, NULL);

  if (!scrResReply) {
    LOGE << "Failed to get screen information";
    exit(EXIT_FAILURE);
  }

  xcb_randr_crtc_t *firstCrtc =
    xcb_randr_get_screen_resources_crtcs(scrResReply);
  xcb_randr_output_t *firstOutput =
    xcb_randr_get_screen_resources_outputs(scrResReply);

  crtc_num = *firstCrtc;
  output_num = *firstOutput;
  free(scrResReply);

  // Get initial gamma ramp and size
  xcb_randr_get_crtc_gamma_reply_t* gammaReply =
    xcb_randr_get_crtc_gamma_reply(dsp, xcb_randr_get_crtc_gamma(dsp, crtc_num), NULL);
  if (!gammaReply) {
    LOGE << "Failed to get gamma information";
    exit(EXIT_FAILURE);
  }
  // get ramp size
  ramp_sz = gammaReply->size;
  {
    if (ramp_sz == 0) {
      LOGE << "Invalid gamma ramp size";
      free(gammaReply);
      exit(EXIT_FAILURE);
		}

		init_ramp.resize(3 * size_t(ramp_sz) * sizeof(uint16_t));

		uint16_t *d = init_ramp.data(), *r, *g, *b;

    r = xcb_randr_get_crtc_gamma_red(gammaReply);
    g = xcb_randr_get_crtc_gamma_green(gammaReply);
    b = xcb_randr_get_crtc_gamma_blue(gammaReply);
    free(gammaReply);
    if (!r or !g or !b) {
      LOGE << "Failed to get initial gamma ramp";
      initial_ramp_exists = false;
    } else {
      d[0*ramp_sz] = *r;
      d[1*ramp_sz] = *g;
      d[2*ramp_sz] = *b;
    }
	}

  // get backlight information
  xcb_intern_atom_cookie_t backlightCookie;
  xcb_intern_atom_reply_t* backlightReply;

  backlightCookie = xcb_intern_atom(dsp, 1, strlen("Backlight"), "Backlight");
  // backlightCookie[1] = xcb_intern_atom(dsp, 1, strlen("BACKLIGHT"), "BACKLIGHT");

  backlightReply = xcb_intern_atom_reply(dsp, backlightCookie, NULL);
  if (!backlightReply)
    {
      LOGI << "No backlight information available";
      initial_backlight_exists = false;
    }
  else {
    backlight = backlightReply->atom;
    free(backlightReply);
    init_backlight = getBacklight();
    LOGD << "Initial backlight value = " << init_backlight;
  }
}

void X11::getX11Snapshot(std::vector<uint8_t> &buf) noexcept
{
	// const auto img = XGetImage(dsp, root, 0, 0, w, h, AllPlanes, ZPixmap);
  const auto reply =
    xcb_get_image_reply(dsp,
                        xcb_get_image(dsp,
                                      XCB_IMAGE_FORMAT_Z_PIXMAP,
                                      root,
                                      0,
                                      0,
                                      w,
                                      h,
                                      static_cast<uint32_t>(~0)
                                      ),
                        NULL);
	memcpy(buf.data(), xcb_get_image_data(reply), buf.size());
	free(reply);
}

void X11::fillRamp(std::vector<uint16_t> &ramp, const int brightness, const int temp_step)
{
	auto r = &ramp[0 * ramp_sz],
	     g = &ramp[1 * ramp_sz],
	     b = &ramp[2 * ramp_sz];

	std::array<double, 3> c{1.0, 1.0, 1.0};

	setColors(temp_step, c);

	/* This equals 32 when ramp_sz = 2048, 64 when 1024, etc.
	*  Assuming ramp_sz = 2048 and pure state (default brightness/temp)
	*  each color channel looks like:
	* { 0, 32, 64, 96, ... UINT16_MAX - 32 } */
	const int ramp_mult = (UINT16_MAX + 1) / ramp_sz;

	for (int32_t i = 0; i < ramp_sz; ++i)
	{
		const int val = std::clamp(int(normalize(0, brt_slider_steps, brightness) * ramp_mult * i), 0, UINT16_MAX);

		r[i] = uint16_t(val * c[0]);
		g[i] = uint16_t(val * c[1]);
		b[i] = uint16_t(val * c[2]);
	}
}

void X11::setGamma(int scr_br, int temp)
{
	std::vector<uint16_t> r (3 * ramp_sz * sizeof(uint16_t));

	fillRamp(r, scr_br, temp);

  xcb_randr_set_crtc_gamma(dsp, crtc_num, ramp_sz, &r[0*ramp_sz], &r[1*ramp_sz], &r[2*ramp_sz]);
}

void X11::setGamma(int temp) {
  // set at maximum brightness
  setGamma(brt_slider_steps, temp);
}

void X11::setInitialGamma(bool set_previous)
{
	if(set_previous && initial_ramp_exists)
	{
		LOGI << "Setting previous gamma";
    xcb_randr_set_crtc_gamma(dsp, crtc_num, ramp_sz, &init_ramp[0 * ramp_sz],
                             &init_ramp[1 * ramp_sz], &init_ramp[2 * ramp_sz]);
	}
	else
	{
		LOGI << "Setting pure gamma";
		X11::setGamma(brt_slider_steps, 0);
	}
}

void X11::setInitialBacklight(bool set_previous)
{
  if (set_previous and initial_backlight_exists)
    {
      setBacklight(init_backlight);
    }
  else
    {
      setBacklight(brt_slider_steps);
    }
}

void X11::setBacklight(uint32_t level)
{
  if (!dsp or !backlight) {
    return;
  }

  uint32_t _level = (uint32_t)((double)(level*255)/(double)brt_slider_steps);
  xcb_randr_change_output_property(dsp, output_num, backlight, XCB_ATOM_INTEGER, 32,
                                   XCB_PROP_MODE_REPLACE, 1, (uint8_t*)&_level);
}

uint32_t X11::getBacklight(void)
{
  uint32_t retval = 0;
  if (backlight != XCB_ATOM_NONE and dsp) {
    xcb_randr_get_output_property_cookie_t cookie;
    xcb_randr_get_output_property_reply_t* reply;

    cookie = xcb_randr_get_output_property(dsp, output_num, backlight, XCB_ATOM_NONE, 0, 4, 0, 0);
    reply = xcb_randr_get_output_property_reply(dsp, cookie, NULL);
    if (!reply)
      {
        LOGE << "Cannot get backlight information";
        return -1;
      }
    retval = (uint32_t)(*((int32_t*)xcb_randr_get_output_property_data(reply)));
    retval = (uint32_t)((double)retval*(double)brt_slider_steps/255.0);
    free(reply);
  }

  return retval;
}

uint32_t X11::getWidth()
{
	return w;
}

uint32_t X11::getHeight()
{
	return h;
}

xcb_screen_t* X11::screenOfDisplay(int screen) {
  if (!dsp) {
    return NULL;
  }

  xcb_screen_iterator_t iter;
  iter = xcb_setup_roots_iterator(xcb_get_setup(dsp));
  for (;iter.rem; --screen, xcb_screen_next(&iter))
    if (screen == 0) {
      return iter.data;
    }
  return NULL;
}

X11::~X11()
{
  if (dsp) {
    xcb_disconnect(dsp);
  }
}
