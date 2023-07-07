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
#include "frontend.h"
#include "access_firmware.h"

using namespace Genode;


static Msg_buffer      _wifi_msg_buffer;
static Wifi::Frontend *_wifi_frontend = nullptr;


/**
 * Notify front end about command processing
 *
 * Called by the CTRL interface after wpa_supplicant has processed
 * the command.
 */
void wifi_block_for_processing(void)
{
	if (!_wifi_frontend) {
		warning("frontend not available, dropping notification");
		return;
	}

	/*
	 * Next time we block as long as the front end has not finished
	 * handling our previous request
	 */
	_wifi_frontend->block_for_processing();

	/* XXX hack to trick poll() into returning faster */
	wpa_ctrl_set_fd();
}


void wifi_notify_cmd_result(void)
{
	if (!_wifi_frontend) {
		warning("frontend not available, dropping notification");
		return;
	}

	Signal_transmitter(_wifi_frontend->result_sigh()).submit();
}


/**
 * Notify front end about triggered event
 *
 * Called by the CTRL interface whenever wpa_supplicant has triggered
 * a event.
 */
void wifi_notify_event(void)
{
	if (!_wifi_frontend) {
		Genode::warning("frontend not available, dropping notification");
		return;
	}

	Signal_transmitter(_wifi_frontend->event_sigh()).submit();
}


/* exported by wifi.lib.so */
extern void wifi_init(Genode::Env&, Genode::Blockade&);
extern void wifi_set_rfkill_sigh(Genode::Signal_context_capability);


static Genode::Blockade _wpa_startup_blockade { };

struct Main
{
	Env  &env;

	Constructible<Wpa_thread>     _wpa;
	Constructible<Wifi::Frontend> _frontend;

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
		_frontend.construct(env, _wifi_msg_buffer);
		_wifi_frontend = &*_frontend;

		Wifi::rfkill_establish_handler(*_wifi_frontend);
		Wifi::firmware_establish_handler(_request_handler);

		_wpa.construct(env, _wpa_startup_blockade);
	}
};


/**
 * Return shared-memory message buffer
 *
 * It is used by the wpa_supplicant CTRL interface.
 */
void *wifi_get_buffer(void)
{
	return &_wifi_msg_buffer;
}


/*
 * Since the wireless LAN driver incorporates the 'wpa_supplicant',
 * which itself is a libc-using application, we have to initialize
 * the libc environment. Normally this initialization is performed
 * by the libc (see 'src/lib/libc/component.cc') but as the various
 * initcalls of the Linux kernel are registered as ctor we have to
 * initialize the Lx_kit::Env before the static ctors are executed.
 * As those are called prior to calling 'Libc::Component::construct',
 * which is implemented by us, we pose as regular component and
 * call the libc 'Component::construct' explicitly after we have
 * finished our initialization (Lx_kit::Env include).
 */

void Component::construct(Genode::Env &env)
{
	try {
		Genode::Heap shared_obj_heap(env.ram(), env.rm());

		Shared_object shared_obj(env, shared_obj_heap, "libc.lib.so",
		                         Shared_object::BIND_LAZY,
		                         Shared_object::DONT_KEEP);

		typedef void (*Construct_fn)(Genode::Env &);

		Construct_fn const construct_fn =
			shared_obj.lookup<Construct_fn>("_ZN9Component9constructERN6Genode3EnvE");

		/* prepare Lx_kit::Env */
		wifi_init(env, _wpa_startup_blockade);

		construct_fn(env);
	} catch (... /* intentional catch-all */) {
		Genode::error("could not perform multi-staged construction");
		throw;
	}
}


void Libc::Component::construct(Libc::Env &env)
{
	static Main server(env);
}
