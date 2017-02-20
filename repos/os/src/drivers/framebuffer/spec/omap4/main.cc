/*
 * \brief  Frame-buffer driver for the OMAP4430 display-subsystem (HDMI)
 * \author Norman Feske
 * \date   2012-06-21
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/log.h>
#include <framebuffer_session/framebuffer_session.h>
#include <dataspace/client.h>
#include <blit/blit.h>
#include <os/static_root.h>
#include <timer_session/connection.h>

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

		Timer::Connection _timer;

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

		Session_component(Genode::Env &env, Driver &driver, size_t width, size_t height,
		                  Driver::Output output, bool buffered)
		: _width(width),
		  _height(height),
		  _buffered(buffered),
		  _format(Driver::FORMAT_RGB565),
		  _size(driver.buffer_size(width, height, _format)),
		  _bb_ds(buffered ? env.ram().alloc(_size)
		                  : Genode::Ram_dataspace_capability()),
		  _bb_addr(buffered ? (void*)env.rm().attach(_bb_ds) : 0),
		  _fb_ds(env.ram().alloc(_size, WRITE_COMBINED)),
		  _fb_addr((void*)env.rm().attach(_fb_ds)),
		  _timer(env)
		{
			if (!driver.init(width, height, _format, output,
				Dataspace_client(_fb_ds).phys_addr())) {
				error("Could not initialize display");
				struct Could_not_initialize_display : Exception { };
				throw Could_not_initialize_display();
			}

			Genode::log("using ", width, "x", height,
			            output == Driver::OUTPUT_HDMI ? " HDMI" : " LCD");
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

			_timer.sigh(_sync_sigh);
			_timer.trigger_periodic(10*1000);
		}

		void refresh(int x, int y, int w, int h) override
		{
			if (_buffered)
				_refresh_buffered(x, y, w, h);

			if (_sync_sigh.valid())
				Signal_transmitter(_sync_sigh).submit();
		}
};


template <typename T>
static T config_attribute(Genode::Xml_node node, char const *attr_name, T const &default_value)
{
	return node.attribute_value(attr_name, default_value);
}

static Framebuffer::Driver::Output config_output(Genode::Xml_node node,
                                                 Framebuffer::Driver::Output default_value)
{
	Framebuffer::Driver::Output value = default_value;

	try {
		Genode::String<8> output;
		node.attribute("output").value(&output);

		if (output == "LCD") { value = Framebuffer::Driver::OUTPUT_LCD; }
	} catch (...) { }

	return value;
}


struct Main
{
	Genode::Env        &_env;
	Genode::Entrypoint &_ep;

	Genode::Attached_rom_dataspace _config { _env, "config" };

	Framebuffer::Driver _driver { _env };

	Framebuffer::Session_component _fb_session { _env, _driver,
		config_attribute(_config.xml(), "width", 1024u),
		config_attribute(_config.xml(), "height", 768u),
		config_output(_config.xml(), Framebuffer::Driver::OUTPUT_HDMI),
		config_attribute(_config.xml(), "buffered", false),
	};

	Genode::Static_root<Framebuffer::Session> _fb_root { _ep.manage(_fb_session) };

	Main(Genode::Env &env) : _env(env), _ep(_env.ep())
	{
		/* announce service */
		_env.parent().announce(_ep.manage(_fb_root));
	}
};


void Component::construct(Genode::Env &env) { static Main main(env); }
