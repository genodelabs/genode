/*
 * \brief  USB driver main program
 * \author Norman Feske
 * \author Sebastian Sumpf <sebastian.sumpf@genode-labs.com>
 * \date   2012-01-29
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */


/* Genode */
#include <base/printf.h>
#include <base/sleep.h>
#include <os/server.h>
#include <nic_session/nic_session.h>

/* Local */
#include <platform.h>
#include <routine.h>
#include <signal.h>

extern "C" {
#include <dde_kit/timer.h>
}

using namespace Genode;

extern "C" int subsys_usb_init();
extern "C" void subsys_input_init();
extern "C" void module_evdev_init();
extern "C" void module_hid_init();
extern "C" void module_hid_init_core();
extern "C" void module_hid_generic_init();
extern "C" void module_usb_stor_init();
extern "C" void module_ch_driver_init();

extern "C" void start_input_service(void *ep);

Routine *Routine::_current    = 0;
Routine *Routine::_dead       = 0;
Routine *Routine::_main       = 0;
bool     Routine::_all        = false;

void breakpoint() { PDBG("BREAK"); }


static void init(Services *services)
{
	/* start jiffies */
	dde_kit_timer_init(0, 0);

	/* USB */
	subsys_usb_init();

	/* input + HID */
	if (services->hid) {
		subsys_input_init();
		module_evdev_init();

		/* HID */
		module_hid_init_core();
		module_hid_init();
		module_hid_generic_init();
		module_ch_driver_init();
	}

	/* host controller */
	platform_hcd_init(services);

	/* storage */
	if (services->stor)
		module_usb_stor_init();
}


void start_usb_driver(Server::Entrypoint &ep)
{
	Services services;

	if (services.hid)
		start_input_service(&ep.rpc_ep());

	Timer::init(ep);
	Irq::init(ep);
	Event::init(ep);
	Storage::init(ep);
	Nic::init(ep);

	Routine::add(0, 0, "Main", true);
	Routine::current_use_first();
	init(&services);

	Routine::main();
}
