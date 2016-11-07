/*
 * \brief  Frame-buffer driver for the OMAP4430 display-subsystem (HDMI)
 * \author Norman Feske
 * \date   2012-06-21
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <framebuffer_session/framebuffer_session.h>
#include <cap_session/connection.h>
#include <dataspace/client.h>
#include <base/log.h>
#include <base/sleep.h>
#include <blit/blit.h>
#include <os/config.h>
#include <os/static_root.h>

/* local includes */
#include <driver.h>


namespace Framebuffer {
	using namespace Genode;
	class Session_component;
};


class Framebuffer::Session_component : public Genode::Rpc_object<Framebuffer::Session>
{
	private:

		size_t         _width;
		size_t         _height;
		bool           _buffered;
		Mode           _mode;
		Driver::Format _format;
		size_t         _size;

		/* dataspace uses a back buffer (if '_buffered' is true) */
		Genode::Dataspace_capability _bb_ds;
		void                        *_bb_addr;

		/* dataspace of physical frame buffer */
		Genode::Dataspace_capability _fb_ds;
		void                        *_fb_addr;

		Signal_context_capability _sync_sigh;

		/**
		 * Convert Driver::Format to Framebuffer::Mode::Format
		 */
		static Mode::Format _convert_format(Driver::Format driver_format)
		{
			switch (driver_format) {
			case Driver::FORMAT_RGB565: return Mode::RGB565;
			}
			return Mode::INVALID;
		}

		void _refresh_buffered(int x, int y, int w, int h)
		{
			Mode _mode = mode();

			/* clip specified coordinates against screen boundaries */
			int x2 = min(x + w - 1, (int)_mode.width()  - 1),
				y2 = min(y + h - 1, (int)_mode.height() - 1);
			int x1 = max(x, 0),
				y1 = max(y, 0);
			if (x1 > x2 || y1 > y2) return;

			int bypp = _mode.bytes_per_pixel();

			/* copy pixels from back buffer to physical frame buffer */
			char *src = (char *)_bb_addr + bypp*(_mode.width()*y1 + x1),
			     *dst = (char *)_fb_addr + bypp*(_mode.width()*y1 + x1);

			blit(src, bypp*_mode.width(), dst, bypp*_mode.width(),
				 bypp*(x2 - x1 + 1), y2 - y1 + 1);
		}

	public:

		Session_component(Driver &driver, size_t width, size_t height,
		                  Driver::Output output, bool buffered)
		: _width(width),
		  _height(height),
		  _buffered(buffered),
		  _format(Driver::FORMAT_RGB565),
		  _size(driver.buffer_size(width, height, _format)),
		  _bb_ds(buffered ? Genode::env()->ram_session()->alloc(_size)
		                  : Genode::Ram_dataspace_capability()),
		  _bb_addr(buffered ? (void*)Genode::env()->rm_session()->attach(_bb_ds) : 0),
		  _fb_ds(Genode::env()->ram_session()->alloc(_size, WRITE_COMBINED)),
		  _fb_addr((void*)Genode::env()->rm_session()->attach(_fb_ds))
		{
			if (!driver.init(width, height, _format, output,
				Dataspace_client(_fb_ds).phys_addr())) {
				error("Could not initialize display");
				struct Could_not_initialize_display : Exception { };
				throw Could_not_initialize_display();
			}
		}

		/************************************
		 ** Framebuffer::Session interface **
		 ************************************/

		Dataspace_capability dataspace() override
		{
			return _buffered ? _bb_ds : _fb_ds;
		}

		Mode mode() const override
		{
			return Mode(_width,
			            _height,
			            _convert_format(_format));
		}

		void mode_sigh(Genode::Signal_context_capability) override { }

		void sync_sigh(Genode::Signal_context_capability sigh) override
		{
			_sync_sigh = sigh;
		}

		void refresh(int x, int y, int w, int h) override
		{
			if (_buffered)
				_refresh_buffered(x, y, w, h);

			if (_sync_sigh.valid())
				Signal_transmitter(_sync_sigh).submit();
		}
};


static bool config_attribute(const char *attr_name)
{
	return Genode::config()->xml_node().attribute_value(attr_name, false);
}


int main(int, char **)
{
	using namespace Framebuffer;

	size_t width = 1024;
	size_t height = 768;
	Driver::Output output = Driver::OUTPUT_HDMI;
	try {
		char out[5] = {}; 
		Genode::Xml_node config_node = Genode::config()->xml_node();
		config_node.attribute("width").value(&width);
		config_node.attribute("height").value(&height);
		config_node.attribute("output").value(out, sizeof(out));
		if (!Genode::strcmp(out, "LCD")) {
			output = Driver::OUTPUT_LCD;
		}
	}
	catch (...) {
		log("using default configuration: HDMI@", width, "x", height);
	}

	static Driver driver;

	/*
	 * Initialize server entry point
	 */
	enum { STACK_SIZE = 4096 };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "fb_ep");

	/*
	 * Let the entry point serve the framebuffer session and root interfaces
	 */
	static Session_component fb_session(driver, width, height, output,
	                                    config_attribute("buffered"));
	static Static_root<Framebuffer::Session> fb_root(ep.manage(&fb_session));

	/*
	 * Announce service
	 */
	env()->parent()->announce(ep.manage(&fb_root));

	sleep_forever();
	return 0;
}

