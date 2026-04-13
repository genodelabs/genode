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

#ifndef _BOARD_INFO_H_
#define _BOARD_INFO_H_

#include <boot_fb.h>

namespace Pc_discover { struct Board_info; }

struct Pc_discover::Board_info
{
	enum class Pci_class : unsigned {
		VGA  = 0x30000,
		AHCI = 0x10601,
		NVME = 0x10802,
		UHCI = 0xc0300, OHCI = 0xc0310, EHCI = 0xc0320, XHCI = 0xc0330,
	};

	enum class Pci_vendor : unsigned { INTEL = 0x8086U, };

	static bool _matches_class(Node const &pci, Pci_class value)
	{
		return pci.attribute_value("class", 0U) == unsigned(value);
	};

	static bool _matches_vendor(Node const &pci, Pci_vendor value)
	{
		return pci.attribute_value("vendor_id", 0U) == unsigned(value);
	};

	static bool _matches_usb(Node const &pci)
	{
		return _matches_class(pci, Pci_class::UHCI) || _matches_class(pci, Pci_class::OHCI)
			|| _matches_class(pci, Pci_class::EHCI) || _matches_class(pci, Pci_class::XHCI);
	}

	static bool _matches_ahci(Node const &pci)
	{
		return _matches_class(pci, Pci_class::AHCI);
	}

	/**
	 * Runtime-detected features
	 */
	struct Detected
	{
		bool intel_gfx, boot_fb, vga, nvme, ahci, usb, ps2;

		void print(Output &out) const
		{
			Genode::print(out, "intel_gfx=", intel_gfx, " boot_fb=",   boot_fb,
			                  " vga=",       vga,       " nvme=",      nvme,
			                  " ahci=",      ahci,      " usb=",       usb);
		}

		static inline Detected from_node(Node const &devices, Node const &platform);

	} detected;
};


Pc_discover::Board_info::Detected
Pc_discover::Board_info::Detected::from_node(Node const &devices, Node const &platform)
{
	Detected detected { };

	Boot_fb::with_mode(platform, [&] (Boot_fb::Mode mode) {
		detected.boot_fb = mode.valid(); });

	devices.for_each_sub_node("device", [&] (Node const &device) {

		if (device.attribute_value("name", String<16>()) == "ps2")
			detected.ps2 = true;

		device.with_optional_sub_node("pci-config", [&] (Node const &pci) {

			if (_matches_class(pci, Pci_class::NVME)) detected.nvme = true;
			if (_matches_usb(pci))                    detected.usb  = true;
			if (_matches_ahci(pci))                   detected.ahci = true;

			if (_matches_class(pci, Pci_class::VGA)) {
				detected.vga = true;
				if (_matches_vendor(pci, Pci_vendor::INTEL))
					detected.intel_gfx = true;
			}
		});
	});

	return detected;
}

#endif /* _BOARD_INFO_H_ */
