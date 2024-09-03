/*
 * \brief  Startup Wifi driver
 * \author Josef Soentgen
 * \date   2014-03-03
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/component.h>
#include <base/shared_object.h>
#include <libc/component.h>
#include <base/attached_rom_dataspace.h>
#include <base/log.h>
#include <base/sleep.h>
#include <os/reporter.h>
#include <timer_session/connection.h>
#include <util/xml_node.h>

/* wifi library includes */
#include <wifi/firmware.h>

/* local includes */
#include "util.h"
#include "wpa.h"
#include "manager.h"
#include "access_firmware.h"

using namespace Genode;


/* exported by wifi.lib.so */
extern void wifi_init(Genode::Env&, Genode::Blockade&);
extern void wifi_set_rfkill_sigh(Genode::Signal_context_capability);


static Genode::Blockade _wpa_startup_blockade { };

struct Main
{
	Env  &env;

	Constructible<Wpa_thread>    _wpa;
	Constructible<Wifi::Manager> _manager;

	struct Request_handler : Wifi::Firmware_request_handler
	{
		Signal_handler<Request_handler> _handler;

		void _handle_request()
		{
			using Fw_path = Genode::String<128>;
			using namespace Wifi;

			Firmware_request *request_ptr = firmware_get_request();
			if (!request_ptr)
				return;

			Firmware_request &request = *request_ptr;

			request.success = false;

			switch (request.state) {
			case Firmware_request::State::PROBING:
				{
					Fw_path const path { "/firmware/", request.name };

					Stat_firmware_result const result = access_firmware(path.string());

					request.fw_len = result.success ? result.length : 0;
					request.success = result.success;

					request.submit_response();
					break;
				}
			case Firmware_request::State::REQUESTING:
				{
					Fw_path const path { "/firmware/", request.name };

					Read_firmware_result const result =
						read_firmware(path.string(), request.dst, request.dst_len);

					request.success = result.success;

					request.submit_response();
					break;
				}
			case Firmware_request::State::INVALID:
				break;
			case Firmware_request::State::PROBING_COMPLETE:
				break;
			case Firmware_request::State::REQUESTING_COMPLETE:
				break;
			}
		}

		Request_handler(Genode::Entrypoint &ep)
		:
			_handler { ep, *this, &Request_handler::_handle_request }
		{ }

		void submit_request() override
		{
			_handler.local_submit();
		}
	};

	Request_handler _request_handler { env.ep() };

	Main(Genode::Env &env) : env(env)
	{
		/* prepare Lx_kit::Env */
		wifi_init(env, _wpa_startup_blockade);

		_manager.construct(env);

		Wifi::rfkill_establish_handler(*_manager);
		Wifi::firmware_establish_handler(_request_handler);

		Wifi::ctrl_init(_manager->msg_buffer());
		_wpa.construct(env, _wpa_startup_blockade);
	}
};


void Libc::Component::construct(Libc::Env &env)
{
	static Main server(env);
}
