/*
 * \brief  Startup Wifi driver
 * \author Josef Soentgen
 * \date   2014-03-03
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <base/sleep.h>
#include <os/server.h>

/* local includes */
#include "wpa.h"


extern void wifi_init(Server::Entrypoint &, Genode::Lock &);

static Genode::Lock &wpa_startup_lock()
{
	static Genode::Lock _l(Genode::Lock::LOCKED);
	return _l;
}

struct Main
{
	Server::Entrypoint &_ep;
	Wpa_thread         *_wpa;

	Main(Server::Entrypoint &ep)
	:
		_ep(ep)
	{
		_wpa = new (Genode::env()->heap()) Wpa_thread(wpa_startup_lock());

		_wpa->start();

		wifi_init(ep, wpa_startup_lock());
	}
};


namespace Server {
	char const *name()             { return "wifi_drv_ep";   }
	size_t      stack_size()       { return 32 * 1024 * sizeof(long); }
	void construct(Entrypoint &ep) { static Main server(ep); }
}
