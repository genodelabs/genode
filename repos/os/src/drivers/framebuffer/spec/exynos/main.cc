/*
 * \brief  Framebuffer driver for Exynos5 HDMI
 * \author Martin Stein
 * \date   2013-08-09
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <framebuffer_session/framebuffer_session.h>
#include <timer_session/connection.h>
#include <dataspace/client.h>
#include <base/log.h>
#include <os/static_root.h>
#include <util/string.h>

/* local includes */
#include <driver.h>

namespace Framebuffer
{
	using namespace Genode;

	/**
	 * Framebuffer session backend
	 */
	class Session_component;
	struct Main;
};

class Framebuffer::Session_component
:
	public Genode::Rpc_object<Framebuffer::Session>
{
	private:

		Genode::Env &_env;

		unsigned                  _width;
		unsigned                  _height;
		Driver::Format            _format;
		size_t                    _size;
		Dataspace_capability      _ds;
		addr_t                    _phys_base;
		Timer::Connection         _timer { _env };

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

	public:

		/**
		 * Constructor
		 *
		 * \param driver  driver backend that communicates with hardware
		 * \param width   width of framebuffer in pixel
		 * \param height  height of framebuffer in pixel
		 * \param output  targeted output device
		 */
		Session_component(Genode::Env &env, Driver &driver,
		                  unsigned width, unsigned height)
		:
			_env(env),
			_width(width),
			_height(height),
			_format(Driver::FORMAT_RGB565),
			_size(driver.buffer_size(width, height, _format)),
			_ds(_env.ram().alloc(_size, WRITE_COMBINED)),
			_phys_base(Dataspace_client(_ds).phys_addr())
		{
			if (driver.init(width, height, _format, _phys_base)) {
				error("could not initialize display");
				struct Could_not_initialize_display : Exception { };
				throw Could_not_initialize_display();
			}
		}

		/************************************
		 ** Framebuffer::Session interface **
		 ************************************/

		Dataspace_capability dataspace() override { return _ds; }

		Mode mode() const override
		{
			return Mode(_width, _height, _convert_format(_format));
		}

		void mode_sigh(Genode::Signal_context_capability) override { }

		void sync_sigh(Genode::Signal_context_capability sigh) override
		{
			_timer.sigh(sigh);
			_timer.trigger_periodic(10*1000);
		}

		void refresh(int, int, int, int) override { }
};


static unsigned config_dimension(Genode::Xml_node node, char const *attr,
                                 unsigned default_value)
{
	return node.attribute_value(attr, default_value);
}


struct Main
{
	Genode::Env        &_env;
	Genode::Entrypoint &_ep;

	Genode::Attached_rom_dataspace _config { _env, "config" };

	Framebuffer::Driver _driver { _env };

	Framebuffer::Session_component _fb_session { _env, _driver,
		config_dimension(_config.xml(), "width", 1920),
		config_dimension(_config.xml(), "height", 1080)
	};

	Genode::Static_root<Framebuffer::Session> _fb_root { _ep.manage(_fb_session) };

	Main(Genode::Env &env) : _env(env), _ep(_env.ep())
	{
		/* announce service and relax */
		_env.parent().announce(_ep.manage(_fb_root));
	}
};


void Component::construct(Genode::Env &env) { static Main main(env); }
