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
#include <libc/component.h>
#include <base/attached_rom_dataspace.h>
#include <base/log.h>
#include <base/sleep.h>
#include <os/reporter.h>
#include <timer_session/connection.h>
#include <util/xml_node.h>

/* local includes */
#include <util.h>
#include <wpa.h>
#include <frontend.h>


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


struct Main
{
	Env  &env;

	Constructible<Wpa_thread>     _wpa;
	Constructible<Wifi::Frontend> _frontend;

	Blockade _wpa_startup_blockade { };

	Main(Genode::Env &env) : env(env)
	{
		_frontend.construct(env, _wifi_msg_buffer);
		_wifi_frontend = &*_frontend;
		wifi_set_rfkill_sigh(_wifi_frontend->rfkill_sigh());

		_wpa.construct(env, _wpa_startup_blockade);

		wifi_init(env, _wpa_startup_blockade);
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


void Libc::Component::construct(Libc::Env &env)
{
	static Main server(env);
}
