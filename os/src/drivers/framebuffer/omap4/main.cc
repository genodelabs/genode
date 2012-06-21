/*
 * \brief  Frame-buffer driver for the OMAP4430 display-subsystem (HDMI)
 * \author Norman Feske
 * \date   2012-06-21
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
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
#include <root/component.h>

/* local includes */
#include <driver.h>


/**
 * Root interface that hands out a statically created session
 */
template <typename SESSION_TYPE>
class Static_root : public Genode::Rpc_object<Genode::Typed_root<SESSION_TYPE> >
{
	private:

		typedef Genode::Capability<SESSION_TYPE> Session_capability;

		Session_capability _session;

	public:

		/**
		 * Constructor
		 *
		 * \param session  session to be provided to the client
		 */
		Static_root(Session_capability session) : _session(session) { }


		/********************
		 ** Root interface **
		 ********************/

		Genode::Session_capability session(Genode::Root::Session_args const &args) { return _session; }
		void upgrade(Genode::Session_capability, Genode::Root::Upgrade_args const &) { }
		void close(Genode::Session_capability) { }
};


namespace Framebuffer {
	using namespace Genode;
	class Session_component;
};


class Framebuffer::Session_component : public Genode::Rpc_object<Framebuffer::Session>
{
	private:

		Driver::Mode         _mode;
		Driver::Format       _format;
		size_t               _size;
		Dataspace_capability _ds;
		addr_t               _phys_base;

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

		Session_component(Driver &driver)
		:
			_mode(Driver::MODE_1024_768),
			_format(Driver::FORMAT_RGB565),
			_size(Driver::buffer_size(_mode, _format)),
			_ds(env()->ram_session()->alloc(_size, false)),
			_phys_base(Dataspace_client(_ds).phys_addr())
		{
			if (!driver.init(_mode, _format, _phys_base)) {
				PERR("Could not initialize display");
				struct Could_not_initialize_display : Exception { };
				throw Could_not_initialize_display();
			}
		}

		/************************************
		 ** Framebuffer::Session interface **
		 ************************************/

		Dataspace_capability dataspace() { return _ds; }

		void release() { }

		Mode mode() const
		{
			return Mode(Driver::width(_mode),
			            Driver::height(_mode),
			            _convert_format(_format));
		}

		void mode_sigh(Genode::Signal_context_capability) { }

		void refresh(int, int, int, int) { }
};


int main(int, char **)
{
	using namespace Framebuffer;

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
	static Session_component                 fb_session(driver);
	static Static_root<Framebuffer::Session> fb_root(ep.manage(&fb_session));

	/*
	 * Announce service
	 */
	env()->parent()->announce(ep.manage(&fb_root));

	sleep_forever();
	return 0;
}

