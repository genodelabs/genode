/*
 * \brief  Driver for Raspberry Pi specific platform devices
 * \author Norman Feske
 * \date   2013-09-16
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <base/sleep.h>
#include <base/rpc_server.h>
#include <cap_session/connection.h>
#include <root/component.h>

/* platform includes */
#include <platform_session/platform_session.h>
#include <platform/property_message.h>

/* local includes */
#include <mbox.h>
#include <property_command.h>
#include <framebuffer_message.h>


namespace Platform {
	class Session_component;
	class Root;
}


class Platform::Session_component : public Genode::Rpc_object<Platform::Session>
{
	private:

		Mbox &_mbox;

	public:

		/**
		 * Constructor
		 */
		Session_component(Mbox &mbox) : _mbox(mbox) { }


		/**********************************
		 **  Platform session interface  **
		 **********************************/

		void setup_framebuffer(Framebuffer_info &info)
		{
			auto const &msg = _mbox.message<Framebuffer_message>(info);
			_mbox.call<Framebuffer_message>();
			info = msg;
		}

		bool power_state(Power id)
		{
			auto &msg = _mbox.message<Property_message>();
			auto const &res = msg.append<Property_command::Get_power_state>(id);
			_mbox.call<Property_message>();
			return res.state;
		}

		void power_state(Power id, bool enable)
		{
			auto &msg = _mbox.message<Property_message>();
			msg.append_no_response<Property_command::Set_power_state>(id, enable, true);
			_mbox.call<Property_message>();
		}

		uint32_t clock_rate(Clock id)
		{
			auto &msg = _mbox.message<Property_message>();
			auto const &res = msg.append<Property_command::Get_clock_rate>(id);
			_mbox.call<Property_message>();
			return res.hz;
		}
};


class Platform::Root : public Genode::Root_component<Platform::Session_component>
{
	private:

		Mbox _mbox;

	protected:

		Session_component *_create_session(const char *args) {
			return new (md_alloc()) Session_component(_mbox); }

	public:

		Root(Rpc_entrypoint *session_ep, Allocator *md_alloc)
		: Root_component<Session_component>(session_ep, md_alloc) { }
};


int main(int, char **)
{
	using namespace Platform;

	Genode::log("--- Raspberry Pi platform driver ---");

	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, 4096, "rpi_plat_ep");
	static Platform::Root plat_root(&ep, env()->heap());
	env()->parent()->announce(ep.manage(&plat_root));

	sleep_forever();
	return 0;
}
