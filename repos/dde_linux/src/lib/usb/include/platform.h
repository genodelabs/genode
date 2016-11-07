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

#include <base/log.h>
#include <util/xml_node.h>
#include <os/signal_rpc_dispatcher.h>
#include <irq_session/capability.h>

#include <lx_kit/env.h>

struct Services
{

	Genode::Env &env;

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

	/* report generation */
	bool raw_report_device_list = false;

	Services(Genode::Env &env) : env(env)
	{
		using namespace Genode;

		Genode::Xml_node config_node = Lx_kit::env().config_rom().xml();
		try {
			Genode::Xml_node node_hid = config_node.sub_node("hid");
			hid = true;

			try {
				Genode::Xml_node node_screen = node_hid.sub_node("touchscreen");
				node_screen.attribute("width").value(&screen_width);
				node_screen.attribute("height").value(&screen_height);
				multitouch = node_screen.attribute_value("multitouch", false);
			} catch (...) {
				screen_width = screen_height = 0;
				log("Could not read screen resolution in config node");
			}

			log("Configured HID screen with ", screen_width, "x", screen_height,
			    " (multitouch=", multitouch ? "true" : "false", ")");
		} catch (Xml_node::Nonexistent_sub_node) {
			log("No <hid> config node found - not starting the USB HID (Input) service");
		}
	
		try {
			config_node.sub_node("storage");
			stor = true;
		} catch (Xml_node::Nonexistent_sub_node) {
			log("No <storage> config node found - not starting the USB Storage (Block) service");
		}
	
		try {
			config_node.sub_node("nic");
			nic = true;
		} catch (Xml_node::Nonexistent_sub_node) {
			log("No <nic> config node found - not starting the USB Nic (Network) service");
		}

		try {
			Genode::Xml_node node_raw = config_node.sub_node("raw");
			raw = true;

			try {
				Genode::Xml_node node_report = node_raw.sub_node("report");
				raw_report_device_list = node_report.attribute_value("devices", false);
			} catch (...) { }
		} catch (Xml_node::Nonexistent_sub_node) {
			log("No <raw> config node found - not starting external USB service");
		}

		if (config_node.attribute_value("uhci", false)) {
			uhci = true;
			log("Enabled UHCI (USB 1.0/1.1) support");
		}

		if (config_node.attribute_value("ehci", false)) {
			ehci = true;
			log("Enabled EHCI (USB 2.0) support");
		}

		if (config_node.attribute_value("xhci", false)) {
			xhci = true;
			log("Enabled XHCI (USB 3.0) support");
		}

		if (!(uhci | ehci | xhci))
			warning("Warning: No USB controllers enabled.\n"
			        "Use <config (u/e/x)hci=\"yes\"> in your 'usb_drv' configuration");
	}
};

void platform_hcd_init(Services *services);
Genode::Irq_session_capability platform_irq_activate(int irq);

#endif /* _PLATFORM_H_ */
