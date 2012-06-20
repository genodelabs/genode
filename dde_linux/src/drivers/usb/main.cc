/*
 * \brief  USB driver main program
 * \author Norman Feske
 * \author Sebastian Sumpf <sebastian.sumpf@genode-labs.com>
 * \date   2012-01-29
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */


/* Genode */
#include <base/rpc_server.h>
#include <base/printf.h>
#include <base/sleep.h>
#include <cap_session/connection.h>
#include <os/config.h>
#include <util/xml_node.h>

/* Local */
#include "storage/component.h"
#include "routine.h"
#include "signal.h"

extern "C" {
#include <dde_kit/timer.h>
}

using namespace Genode;

extern "C" int subsys_usb_init();
extern "C" void subsys_input_init();
extern "C" void module_evdev_init();
extern "C" void module_hid_init();
extern "C" void module_hid_init_core();
extern "C" void module_usb_mouse_init();
extern "C" void module_usb_kbd_init();
extern "C" void module_usb_stor_init();

extern "C" void start_input_service(void *ep);

Routine *Routine::_current    = 0;
Routine *Routine::_dead       = 0;
bool     Routine::_all        = false;

void breakpoint() { PDBG("BREAK"); }


static void init(bool hid, bool stor)
{
	/* start jiffies */
	dde_kit_timer_init(0, 0);

	/* USB */
	subsys_usb_init();

	/* input + HID */
	if (hid) {
		subsys_input_init();
		module_evdev_init();

		/* HID */
		module_hid_init();
		module_usb_mouse_init();
		module_usb_kbd_init();
	}

	/*
	 * Host controller.
	 *
	 */
	platform_hcd_init();

	/* storage */
	if (stor)
		module_usb_stor_init();
}


int main(int, char **)
{
	/*
	 * Initialize server entry point
	 */
	enum { STACK_SIZE = 4096 };
	static Cap_connection cap;
	static Rpc_entrypoint ep_hid(&cap, STACK_SIZE, "usb_hid_ep");
	static Signal_receiver recv;

	bool hid = false;
	bool stor = false;

	try {
		config()->xml_node().sub_node("hid");
		start_input_service(&ep_hid);
		hid = true;
	} catch (Config::Invalid) {
		PDBG("No <config> node found - not starting any USB services");
		return 0;
	} catch (Xml_node::Nonexistent_sub_node) {
		PDBG("No <hid> config node found - not starting the USB HID (Input) service");
	}

	try {
		config()->xml_node().sub_node("storage");
		stor = true;
	} catch (Xml_node::Nonexistent_sub_node) {
		PDBG("No <storage> config node found - not starting the USB Storage (Block) service");
	}

	Timer::init(&recv);
	Irq::init(&recv);
	Event::init(&recv);
	Service_handler::s()->receiver(&recv);
	Storage::init(&recv);

	Routine::add(0, 0, "Main", true);
	Routine::current_use_first();
	init(hid, stor);

	Routine::remove();

	/* will never be reached */
	sleep_forever();

	return 0;
}
