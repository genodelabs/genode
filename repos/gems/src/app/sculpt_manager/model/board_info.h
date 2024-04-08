/*
 * \brief  Board discovery information
 * \author Norman Feske
 * \date   2018-06-01
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__BOARD_INFO_H_
#define _MODEL__BOARD_INFO_H_

#include <model/boot_fb.h>

namespace Sculpt { struct Board_info; }

struct Sculpt::Board_info
{
	/**
	 * Runtime-detected features
	 */
	struct Detected
	{
		bool wifi, nic, intel_gfx, boot_fb, vga, nvme, ahci, usb, ps2;

		void print(Output &out) const
		{
			Genode::print(out, "wifi=",      wifi,      " nic=",       nic,
			                  " intel_gfx=", intel_gfx, " boot_fb=",   boot_fb,
			                  " vga=",       vga,       " nvme=",      nvme,
			                  " ahci=",      ahci,      " usb=",       usb);
		}

		static inline Detected from_xml(Xml_node const &devices, Xml_node const &platform);

	} detected;

	/**
	 * Statically-known or configured features
	 */
	struct Soc
	{
		bool fb, touch, wifi, usb, mmc, modem;

		bool operator != (Soc const &other) const
		{
			return (fb   != other.fb)   || (touch != other.touch)
			    || (wifi != other.wifi) || (usb   != other.usb)
			    || (mmc  != other.mmc)  || (modem != other.modem);
		}
	} soc;

	/**
	 * Features that can be toggled at runtime
	 */
	struct Options
	{
		bool display, usb_net, nic, wifi;

		struct Suppress {

			bool ps2, intel_gpu;

			bool operator != (Suppress const &other) const
			{
				return ps2 != other.ps2 || intel_gpu != other.intel_gpu;
			}

		} suppress;

		bool operator != (Options const &other) const
		{
			return display  != other.display  || usb_net != other.usb_net
			    || nic      != other.nic      || wifi    != other.wifi
			    || suppress != other.suppress;
		}

	} options;

	bool usb_avail()  const { return detected.usb  || soc.usb; }
	bool wifi_avail() const { return detected.wifi || soc.wifi; }
};


Sculpt::Board_info::Detected
Sculpt::Board_info::Detected::from_xml(Xml_node const &devices, Xml_node const &platform)
{
	Detected detected { };

	Boot_fb::with_mode(platform, [&] (Boot_fb::Mode mode) {
		detected.boot_fb = mode.valid(); });

	devices.for_each_sub_node("device", [&] (Xml_node const &device) {

		if (device.attribute_value("name", String<16>()) == "ps2")
			detected.ps2 = true;

		device.with_optional_sub_node("pci-config", [&] (Xml_node const &pci) {

			enum class Pci_class : unsigned {
				WIFI = 0x28000,
				NIC  = 0x20000,
				VGA  = 0x30000,
				AHCI = 0x10601,
				NVME = 0x10802,
				UHCI = 0xc0300, OHCI = 0xc0310, EHCI = 0xc0320, XHCI = 0xc0330,
			};

			enum class Pci_vendor : unsigned { INTEL = 0x8086U, };

			auto matches_class = [&] (Pci_class value)
			{
				return pci.attribute_value("class", 0U) == unsigned(value);
			};

			auto matches_vendor = [&] (Pci_vendor value)
			{
				return pci.attribute_value("vendor_id", 0U) == unsigned(value);
			};

			if (matches_class(Pci_class::WIFI)) detected.wifi = true;
			if (matches_class(Pci_class::NIC))  detected.nic  = true;
			if (matches_class(Pci_class::NVME)) detected.nvme = true;

			if (matches_class(Pci_class::UHCI) || matches_class(Pci_class::OHCI)
			 || matches_class(Pci_class::EHCI) || matches_class(Pci_class::XHCI))
				detected.usb = true;

			if (matches_class(Pci_class::AHCI) && matches_vendor(Pci_vendor::INTEL))
				detected.ahci = true;

			if (matches_class(Pci_class::VGA)) {
				detected.vga = true;
				if (matches_vendor(Pci_vendor::INTEL))
					detected.intel_gfx = true;
			}
		});
	});

	return detected;
}

#endif /* _MODEL__BOARD_INFO_H_ */
