/**
 * \brief  USB webcam model back end using capture session
 * \author Alexander Boettcher
 * \date   2021-04-08
 *
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <base/allocator.h>
#include <base/env.h>
#include <capture_session/connection.h>
#include <gui_session/gui_session.h>
#include <os/reporter.h>
#include <util/xml_node.h>

#include <libyuv/convert_from_argb.h>

extern "C" {
	#include "webcam-backend.h"

	void _type_init_usb_webcam_register_types();

}

using namespace Genode;

struct Capture_webcam
{
	Env                        &_env;
	Gui::Area            const  _area;
	bool                 const  _vflip;
	uint8_t              const  _fps;
	bool                        _force_update { false };

	Constructible<Capture::Connection> _capture;
	Constructible<Attached_dataspace>  _ds;

	Gui::Area _setup_area(Gui::Area const area_in, bool const auto_area)
	{
		Gui::Area area = area_in;

		if (auto_area) {
			Capture::Connection probe { _env, "webcam" };

			area = probe.screen_size();

			if (!area.valid())
				area = area_in;
		}

		return area;
	}


	bool update_yuv(void *frame)
	{
		if (!_area.valid())
			return false;

		bool changed = _force_update;
		_capture->capture_at(Capture::Point(0, 0)).for_each_rect([&](auto) {
			changed = true; });

		if (!changed)
			return false;

		int const src_stride_argb = _area.w() * 4;
		int const dst_stride_yuy2 = _area.w() * 2;

		libyuv::ARGBToYUY2(_ds->local_addr<uint8_t>(), src_stride_argb,
		                   reinterpret_cast<uint8_t*>(frame), dst_stride_yuy2,
		                   _area.w(), _area.h());

		if (_force_update)
			_force_update = false;

		return true;
	}

	bool update_bgr(void *frame)
	{
		if (!_area.valid())
			return false;

		bool changed = false;

		uint8_t            * const bgr  = reinterpret_cast<uint8_t *>(frame);
		Pixel_rgb888 const * const data = _ds->local_addr<Pixel_rgb888>();

		auto const &update_fn = ([&](auto &rect) {
			changed = true;
			for (int y = rect.y1(); y <= rect.y2(); y++) {
				unsigned const row      = _vflip ? y : _area.h() - 1 - y;
				unsigned const row_byte = (row * _area.w() * 3);

				for (int x = rect.x1(); x < rect.x2(); x++) {
					auto &pixel = data[y * _area.w() + x];
					bgr[row_byte + x * 3 + 0] = pixel.b();
					bgr[row_byte + x * 3 + 1] = pixel.g();
					bgr[row_byte + x * 3 + 2] = pixel.r();
				}
			}
		});

		if (_force_update) {
			/* update whole frame */
			_force_update = false;
			Rect const whole(Point(0,0), _area);
			_capture->capture_at(Capture::Point(0, 0));
			update_fn(whole);
		} else
			_capture->capture_at(Capture::Point(0, 0)).for_each_rect(update_fn);

		return changed;
	}

	void capture_state_changed(bool on)
	{
		/* next time update whole frame due to format changes or on/off */
		_force_update = true;

		/* construct/destruct capture connection and dataspace */
		if (on) {
			_capture.construct(_env, "webcam");
			_capture->buffer(_area);
			_ds.construct(_env.rm(), _capture->dataspace());
		} else {
			_ds.destruct();
			_capture.destruct();
		}
	}

	Capture_webcam(Env &env, Gui::Area area, bool auto_area, bool flip, uint8_t fps)
	:
		_env(env),
		_area(_setup_area(area, auto_area)),
		_vflip(flip),
		_fps(fps)
	{
		log("USB webcam ", _area, " fps=", _fps, " vertical_flip=",
		    _vflip ? "yes" : "no");
	}
};

static Genode::Constructible<Capture_webcam> capture;

extern "C" void capture_state_changed(bool on)
{
	capture->capture_state_changed(on);
}

extern "C" bool capture_bgr_frame(void * pixel)
{
	return capture->update_bgr(pixel);
}

extern "C" bool capture_yuv_frame(void * pixel)
{
	return capture->update_yuv(pixel);
}


extern "C" void webcam_backend_config(struct webcam_config *config)
{
	config->fps    = capture->_fps;
	config->width  = capture->_area.w();
	config->height = capture->_area.h();
}

/*
 * Do not use type_init macro because of name mangling
 */
extern "C" void _type_init_host_webcam_register_types(Env &env,
                                                      Xml_node const &webcam)
{
	/* initialize capture session */
	capture.construct(env, Gui::Area(webcam.attribute_value("width",  640u),
	                                 webcam.attribute_value("height", 480u)),
	                  webcam.attribute_value("screen_size", false),
	                  webcam.attribute_value("vertical_flip", false),
	                  webcam.attribute_value("fps", 15u));

	/* register webcam model, which will call webcam_backend_config() */
	_type_init_usb_webcam_register_types();
}
