/**
 * \brief  Platform specific definitions
 * \author Sebastian Sumpf
 * \date   2012-07-06
 *
 * These functions have to be implemented on all supported platforms.
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#include <base/printf.h>
#include <os/config.h>
#include <util/xml_node.h>
#include <os/signal_rpc_dispatcher.h>
#include <irq_session/capability.h>

struct Services
{
	/* USB profiles */
	bool hid  = false;
	bool stor = false;
	bool nic  = false;
	bool raw  = false;

	/* Controller types */
	bool uhci = false; /* 1.0 */
	bool ehci = false; /* 2.0 */
	bool xhci = false; /* 3.0 */

	/*
	 * Screen resolution used by touch devices to convert touchscreen
	 * absolute coordinates to screen absolute coordinates
	 */
	bool          multitouch    = false;
	unsigned long screen_width  = 0;
	unsigned long screen_height = 0;

	Services()
	{
		using namespace Genode;

		try {
			Genode::Xml_node node_hid = config()->xml_node().sub_node("hid");
			hid = true;

			try {
				Genode::Xml_node node_screen = node_hid.sub_node("touchscreen");
				node_screen.attribute("width").value(&screen_width);
				node_screen.attribute("height").value(&screen_height);
				multitouch = node_screen.attribute("multitouch").has_value("yes");
			} catch (...) {
				screen_width = screen_height = 0;
				PDBG("Could not read screen resolution in config node");
			}
		} catch (Xml_node::Nonexistent_sub_node) {
			PDBG("No <hid> config node found - not starting the USB HID (Input) service");
		}
	
		try {
			config()->xml_node().sub_node("storage");
			stor = true;
		} catch (Xml_node::Nonexistent_sub_node) {
			PDBG("No <storage> config node found - not starting the USB Storage (Block) service");
		}
	
		try {
			config()->xml_node().sub_node("nic");
			nic = true;
		} catch (Xml_node::Nonexistent_sub_node) {
			PDBG("No <nic> config node found - not starting the USB Nic (Network) service");
		}

		try {
			config()->xml_node().sub_node("raw");
			raw = true;
		} catch (Xml_node::Nonexistent_sub_node) {
			PDBG("No <raw> config node found - not starting external USB service");
		}

		try {
			if (!config()->xml_node().attribute("uhci").has_value("yes"))
				throw -1;
			uhci = true;
			PINF("Enabled UHCI (USB 1.0/1.1) support");
		} catch (...) { }

		try {
			if (!config()->xml_node().attribute("ehci").has_value("yes"))
				throw -1;
			ehci = true;
			PINF("Enabled EHCI (USB 2.0) support");
		} catch (...) { }

		try {
			if (!config()->xml_node().attribute("xhci").has_value("yes"))
				throw -1;
			xhci = true;
			PINF("Enabled XHCI (USB 3.0) support");
		} catch (...) { }

		if (!(uhci | ehci | xhci))
			PWRN("Warning: No USB controllers enabled.\n"
			     "Use <config (u/e/x)hci=\"yes\"> in your 'usb_drv' configuration");
	}
};

void platform_hcd_init(Services *services);
Genode::Irq_session_capability platform_irq_activate(int irq);

#endif /* _PLATFORM_H_ */
