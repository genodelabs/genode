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

struct Services
{
	/* USB profiles */
	bool hid;
	bool stor;
	bool nic;

	/* Controller types */
	bool uhci; /* 1.0 */
	bool ehci; /* 2.0 */
	bool xhci; /* 3.0 */

	Services()
	: hid(false),  stor(false), nic(false),
	  uhci(false), ehci(false), xhci(false)
	{
		using namespace Genode;

		try {
			config()->xml_node().sub_node("hid");
			hid = true;
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

#endif /* _PLATFORM_H_ */
