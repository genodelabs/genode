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
		Genode::warning("frontend not available, dropping notification");
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
		Genode::warning("frontend not available, dropping notification");
		return;
	}

	Genode::Signal_transmitter(_wifi_frontend->result_sigh()).submit();
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

	Genode::Signal_transmitter(_wifi_frontend->event_sigh()).submit();
}


/**
 * Return shared-memory message buffer
 *
 * It is used by the wpa_supplicant CTRL interface.
 */
void *wifi_get_buffer(void)
{
	return _wifi_frontend ? &_wifi_frontend->msg_buffer() : nullptr;
}


/* exported by wifi.lib.so */
extern void wifi_init(Genode::Env&, Genode::Lock&, bool, Genode::Signal_context_capability);


struct Main
{
	Genode::Env  &env;

	Genode::Constructible<Wpa_thread>     _wpa;
	Genode::Constructible<Wifi::Frontend> _frontend;

	Genode::Lock _wpa_startup_lock { Genode::Lock::LOCKED };

	Main(Genode::Env &env) : env(env)
	{
		_frontend.construct(env);
		_wifi_frontend = &*_frontend;

		_wpa.construct(env, _wpa_startup_lock);

		/*
		 * Forcefully disable 11n but for convenience the attribute is used the
		 * other way araound.
		 */
		bool const disable_11n = !_frontend->use_11n();
		wifi_init(env, _wpa_startup_lock, disable_11n,
		          _frontend->rfkill_sigh());
	}
};


void Libc::Component::construct(Libc::Env &env)
{
	Libc::with_libc([&] () { static Main server(env); });
}
