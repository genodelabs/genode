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
	Capture::Connection         _capture { _env, "webcam" };
	Gui::Area            const  _area;
	bool                 const  _vflip;
	uint8_t              const  _fps;
	Attached_dataspace          _ds { _env.rm(), _capture.dataspace() };
	Constructible<Reporter>     _reporter { };


	Gui::Area setup_area(Gui::Area const area_in, bool const auto_area)
	{
		Gui::Area area = area_in;

		if (auto_area) {
			area = _capture.screen_size();

			if (!area.valid())
				area = area_in;
		}

		/* request setup of dataspace by server */
		_capture.buffer(area);
		return area;
	}

	void update_yuv(void *frame)
	{
		_capture.capture_at(Capture::Point(0, 0));

		int const src_stride_argb = _area.w() * 4;
		int const dst_stride_yuy2 = _area.w() * 2;

		libyuv::ARGBToYUY2(_ds.local_addr<uint8_t>(), src_stride_argb,
		                   reinterpret_cast<uint8_t*>(frame), dst_stride_yuy2,
		                   _area.w(), _area.h());
	}

	void update_bgr(void *frame)
	{
		_capture.capture_at(Capture::Point(0, 0));

		uint8_t * const bgr  = reinterpret_cast<uint8_t *>(frame);
		Pixel_rgb888 *  data = reinterpret_cast<Pixel_rgb888 *>(_ds.local_addr<void>());

		for (int y = 0; y < _area.h(); y++) {
			unsigned const row = _vflip ? y : _area.h() - 1 - y;
			unsigned const row_byte = (row * _area.w() * 3);
			for (int x = 0; x < _area.w(); x++) {
				bgr[row_byte + x * 3 + 0] = data->b();
				bgr[row_byte + x * 3 + 1] = data->g();
				bgr[row_byte + x * 3 + 2] = data->r();

				data++;
			}
		}
	}

	void capture_state_changed(bool on, char const * format)
	{
		if (!_reporter.constructed())
			return;

		Reporter::Xml_generator xml(*_reporter, [&] () {
			xml.attribute("enabled", on);
			xml.attribute("format", format);
		});
	}

	Capture_webcam (Env &env, Gui::Area area, bool auto_area, bool flip,
	                uint8_t fps, bool report)
	:
		_env(env),
		_area(setup_area(area, auto_area)),
		_vflip(flip),
		_fps(fps)
	{
		if (report) {
			_reporter.construct(_env, "capture");
			_reporter->enabled(true);
		}

		log("USB webcam ", _area, " fps=", _fps, " vertical_flip=",
		    _vflip ? "yes" : "no",
		    " report=", _reporter.constructed() ? "enabled" : "disabled");
	}
};

static Genode::Constructible<Capture_webcam> capture;

extern "C" void capture_state_changed(bool on, char const * format)
{
	capture->capture_state_changed(on, format);
}

extern "C" void capture_bgr_frame(void * pixel)
{
	capture->update_bgr(pixel);
}

extern "C" void capture_yuv_frame(void * pixel)
{
	capture->update_yuv(pixel);
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
	                  webcam.attribute_value("fps", 15u),
	                  webcam.attribute_value("report", false));

	/* register webcam model, which will call webcam_backend_config() */
	_type_init_usb_webcam_register_types();
}
