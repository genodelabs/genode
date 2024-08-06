/*
 * \brief  USB webcam app using libuvc
 * \author Josef Soentgen
 * \author Sebastian Sumpf
 * \date   2021-01-25
 *
 * The test component is based on the original 'example.c'.
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/log.h>
#include <gui_session/connection.h>
#include <libc/component.h>
#include <os/pixel_rgb888.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"

/* libuv */
#include <libyuv/convert_argb.h>

/* libuvc stuff */
#include "libuvc/libuvc.h"
#include <stdio.h>
#include <unistd.h>

#pragma GCC diagnostic pop  /* restore -Wconversion warnings */

using namespace Genode;


class Viewer
{
	private:

		Viewer(const Viewer &) = delete;
		const Viewer& operator=(const Viewer&) = delete;

		using PT = Genode::Pixel_rgb888;

		Genode::Env               &_env;
		Gui::Connection            _gui  { _env, "webcam" };
		Gui::Session::View_handle  _view { _gui.create_view() };
		Framebuffer::Mode const    _mode;

		Constructible<Genode::Attached_dataspace> _fb_ds { };
		uint8_t *_framebuffer { nullptr };

	public:

		Viewer(Genode::Env &env, Framebuffer::Mode mode)
		:
			_env    { env },
			_mode   { mode }
		{
			_gui.buffer(mode, false);

			_fb_ds.construct(_env.rm(), _gui.framebuffer.dataspace());
			_framebuffer = _fb_ds->local_addr<uint8_t>();

			using Command = Gui::Session::Command;
			using namespace Gui;

			_gui.enqueue<Command::Geometry>(_view, Gui::Rect(Gui::Point(0, 0), _mode.area));
			_gui.enqueue<Command::To_front>(_view, Gui::Session::View_handle());
			_gui.enqueue<Command::Title>(_view, "webcam");
			_gui.execute();
		}

		uint8_t *framebuffer() { return _framebuffer; }

		void refresh() {
			_gui.framebuffer.refresh(0, 0, _mode.area.w, _mode.area.h);
		}

		Framebuffer::Mode const &mode() { return _mode; }
};


static void cb(uvc_frame_t *frame, void *ptr)
{
	Viewer *viewer = ptr ? reinterpret_cast<Viewer*>(ptr) : nullptr;
	if (!viewer) return;

	int err = 0;
	int width  = viewer->mode().area.w;
	int height = viewer->mode().area.h;

	switch (frame->frame_format) {
		case UVC_COLOR_FORMAT_MJPEG:
			err = libyuv::MJPGToARGB((uint8_t const *)frame->data,
			                         frame->data_bytes,
			                         viewer->framebuffer(),
			                         width * 4,
			                         width, height,
			                         width, height);
			if (err) {
				error("MJPGToARGB returned:", err);
				return;
			}
			break;

		case UVC_COLOR_FORMAT_YUYV:

			/* skip incomplete frames */
			if (frame->data_bytes < width * height * 2ul)
				break;

			err = libyuv::YUY2ToARGB((uint8_t const *)frame->data,
			                         width * 2,
			                         viewer->framebuffer(),
			                         width * 4,
			                         width, height);
			if (err) {
				error("YUY2ToARGB returned:", err);
				return;
			}
			break;
		default:
			return;
	};

	viewer->refresh();
}


class Webcam
{
	private:

		Webcam(const Webcam &) = delete;
		const Webcam& operator=(const Webcam&) = delete;

		Env &_env;
		uvc_context_t       *_context { nullptr };
		uvc_device_t        *_device  { nullptr };
		uvc_device_handle_t *_handle  { nullptr };
		Viewer               _viewer;

		void _cleanup()
		{
			Libc::with_libc([&] () {
				if (_handle)  uvc_stop_streaming(_handle);
				if (_device)  uvc_unref_device(_device);
				if (_context) uvc_exit(_context);
			});
		}

	public:

		Webcam(Env &env, Framebuffer::Mode mode, uvc_frame_format format, unsigned fps)
		:
		  _env(env), _viewer(env, mode)
		{
			int result = Libc::with_libc([&] () {

				uvc_error_t res = uvc_init(&_context, NULL);

				if (res < 0) {
					uvc_perror(res, "uvc_init failed");
					return -1;
				}

				res = uvc_find_device(_context, &_device, 0, 0, NULL);
				if (res < 0) {
					uvc_perror(res, "uvc_find_device failed");
					return -2;
				}

				res = uvc_open(_device, &_handle);
				if (res < 0) {
					uvc_perror(res, "uvc_open failed");
					return -3;
				}

				uvc_stream_ctrl_t control;
				res = uvc_get_stream_ctrl_format_size(_handle, &control, format,
				                                      mode.area.w, mode.area.h,
				                                      fps);
				if (res < 0) {
					error("Unsupported mode: ", mode, " format: ", (unsigned)format, " fps: ", fps);
					log("Supported modes: ");
					uvc_print_diag(_handle, stderr);
					return -4;
				}

				res = uvc_start_streaming(_handle, &control, cb, &_viewer, 0);
				if (res < 0) {
					uvc_perror(res, "Start streaming failed");
					return -5;
				}

				/**
 				 *  Turn on auto exposure if not already set.
 				 *  There are three auto exposure modes (0x2, 0x4, 0x8), which we
 				 *  must first check whether supported by the device.
 				 */
				uint8_t aemode = 0;
				if (uvc_get_ae_mode(_handle, &aemode, UVC_GET_CUR) != UVC_SUCCESS) {
					error("uvc_get_ae_mode() failed");
					return 0;
				}

				enum {
					MANUAL    = 0x1,
					FULL_AUTO = 0x2,
					IRIS_AUTO = 0x4,
					TIME_AUTO = 0x8
				};

				/* manual focus mode active? */
				if (aemode == MANUAL) {
					uint8_t aemodes_avail = 0;
					/* get available modes */
					if (uvc_get_ae_mode(_handle, &aemodes_avail, UVC_GET_RES) != UVC_SUCCESS) {
						error("uvc_get_ae_mode(UVC_GET_RES) failed");
						return 0;
					}

					if (aemodes_avail & FULL_AUTO)
						uvc_set_ae_mode(_handle, FULL_AUTO);
					else if (aemodes_avail & IRIS_AUTO)
						uvc_set_ae_mode(_handle, IRIS_AUTO);
					else if (aemodes_avail & TIME_AUTO)
						uvc_set_ae_mode(_handle, TIME_AUTO);
				}

				return 0;
			});

			if (result < 0) {
				_cleanup();
				throw -1;
			}
		}

		~Webcam()
		{
			_cleanup();
		}
};


class Main
{
	private:

		Env                   &_env;
		Attached_rom_dataspace _config_rom { _env, "config" };
		Constructible<Webcam>  _webcam { };
		Signal_handler<Main>   _config_sigh { _env.ep(), *this, &Main::_config_update };

		void _config_update()
		{
			_config_rom.update();

			if (_config_rom.valid() == false) return;

			Xml_node config = _config_rom.xml();
			bool      enabled = config.attribute_value("enabled", false);
			unsigned  width   = config.attribute_value("width", 640u);
			unsigned  height  = config.attribute_value("height", 480u);
			unsigned  fps     = config.attribute_value("fps", 15u);
			String<8> format  { config.attribute_value("format", String<8>("yuv")) };

			uvc_frame_format frame_format;
			if (format == "yuv")
				frame_format = UVC_FRAME_FORMAT_YUYV;
			else if (format == "mjpeg")
				frame_format = UVC_FRAME_FORMAT_MJPEG;
			else {
				warning("Unkown format '", format, "' trying 'yuv'");
				frame_format = UVC_FRAME_FORMAT_YUYV;
			}

			log("config: enabled: ", enabled, " ", width, "x", height, " format: ", (unsigned)frame_format);
			if (_webcam.constructed())
				_webcam.destruct();

			if (enabled) {
				try {
					_webcam.construct(_env, Framebuffer::Mode {
						.area = { width, height } }, frame_format, fps);
				}  catch (...) { }
			}
		}

	public:

		Main(Env &env)
		:
		  _env(env)
		{
			_config_rom.sigh(_config_sigh);
			_config_update();
		}
};


void Libc::Component::construct(Libc::Env &env)
{
	static Main main(env);
}
