/*
 * \brief  Platform driver - PCI UHCI utilities
 * \author Stefan Kalkowski
 * \date   2022-06-02
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <pci/config.h>
#include <device.h>

namespace Driver {
	static void pci_uhci_quirks(Device::Pci_config cfg, addr_t base);
}


void Driver::pci_uhci_quirks(Device::Pci_config cfg, addr_t base)
{
	enum { UHCI_CLASS_CODE = 0xc0300 };

	if (cfg.class_code != UHCI_CLASS_CODE)
		return;

	/* PCI configuration register for UHCI */
	struct Uhci : Mmio
	{
		struct Usb_legacy_support : Register<0xc0, 16>
		{
			struct Trap_by_60h_read_status            : Bitfield<8, 1> {};
			struct Trap_by_60h_write_status           : Bitfield<9, 1> {};
			struct Trap_by_64h_read_status            : Bitfield<10,1> {};
			struct Trap_by_64h_write_status           : Bitfield<11,1> {};
			struct Usb_pirq_enable                    : Bitfield<13,1> {};
			struct End_of_a20gate_pass_through_status : Bitfield<15,1> {};
		};

		struct Usb_resume_intel : Register<0xc4, 16> {};

		using Mmio::Mmio;
	};

	Uhci config(base);

	using Reg = Uhci::Usb_legacy_support;

	/* BIOS handover */
	Reg::access_t reg = 0;
	Reg::Trap_by_60h_read_status::set(reg,1);
	Reg::Trap_by_60h_write_status::set(reg,1);
	Reg::Trap_by_64h_read_status::set(reg,1);
	Reg::Trap_by_64h_write_status::set(reg,1);
	Reg::End_of_a20gate_pass_through_status::set(reg,1);
	Reg::Usb_pirq_enable::set(reg,1);
	config.write<Reg>(reg);

	if (cfg.vendor_id == 0x8086)
		config.write<Uhci::Usb_resume_intel>(0);
}
