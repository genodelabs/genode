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
#include <base/printf.h>
#include <base/sleep.h>
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

		size_t _width;
		size_t _height;
		Driver::Format            _format;
		size_t                    _size;
		Dataspace_capability      _ds;
		addr_t                    _phys_base;
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

	public:

		Session_component(Driver &driver, size_t width, size_t height,
			Driver::Output output)
		:
			_width(width),
			_height(height),
			_format(Driver::FORMAT_RGB565),
			_size(driver.buffer_size(width, height, _format)),
			_ds(env()->ram_session()->alloc(_size, false)),
			_phys_base(Dataspace_client(_ds).phys_addr())
		{
			if (!driver.init(width, height, _format, output, _phys_base)) {
				PERR("Could not initialize display");
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
			return Mode(_width,
			            _height,
			            _convert_format(_format));
		}

		void mode_sigh(Genode::Signal_context_capability) override { }

		void sync_sigh(Genode::Signal_context_capability sigh) override
		{
			_sync_sigh = sigh;
		}

		void refresh(int, int, int, int) override
		{
			if (_sync_sigh.valid())
				Signal_transmitter(_sync_sigh).submit();
		}
};


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
		PDBG("using default configuration: HDMI@%dx%d", width, height);
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
	static Session_component fb_session(driver, width, height, output);
	static Static_root<Framebuffer::Session> fb_root(ep.manage(&fb_session));

	/*
	 * Announce service
	 */
	env()->parent()->announce(ep.manage(&fb_root));

	sleep_forever();
	return 0;
}

