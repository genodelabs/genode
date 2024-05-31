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

#include <io_port_session/connection.h>
#include <pci/config.h>
#include <device.h>

namespace Driver {
	static void pci_uhci_quirks(Env &, Device const &,
	                            Device::Pci_config const &, Pci::Config const &);
}


void Driver::pci_uhci_quirks(Env                      & env,
                             Device             const & dev,
                             Device::Pci_config const & cfg,
                             Pci::Config        const & pci_config)
{
	enum { UHCI_CLASS_CODE = 0xc0300 };

	if (cfg.class_code != UHCI_CLASS_CODE)
		return;

	/* PCI configuration register for UHCI */
	struct Uhci : Mmio<0xc6>
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

	/* find uhci controller i/o ports */
	Device::Io_port_range::Range range { 0, 0 };
	dev.for_each_io_port_range([&] (unsigned, Device::Io_port_range::Range r,
		                            Device::Pci_bar) {
		if (!range.size) range = r; });

	Io_port_connection io_ports(env, range.addr, range.size);
	Uhci config(pci_config.range());

	bool have_to_reset = false;
	uint16_t UHCI_CMD  = range.addr;
	uint16_t UHCI_INTR = range.addr + 4;

	struct Uhci_command : Register<16>
	{
		struct Enable         : Bitfield<0,1> {};
		struct Reset          : Bitfield<1,1> {};
		struct Global_suspend : Bitfield<3,1> {};
		struct Config         : Bitfield<6,1> {};
	};

	struct Uhci_irq_status : Register<16>
	{
		struct Resume : Bitfield<1,1> {};
	};

	using Reg = Uhci::Usb_legacy_support;


	/***************************
	 ** BIOS handover + reset **
	 ***************************/

	Reg::access_t reg = 0;
	Reg::Trap_by_60h_read_status::set(reg,1);
	Reg::Trap_by_60h_write_status::set(reg,1);
	Reg::Trap_by_64h_read_status::set(reg,1);
	Reg::Trap_by_64h_write_status::set(reg,1);
	Reg::End_of_a20gate_pass_through_status::set(reg,1);
	if (config.read<Reg>() & ~reg)
		have_to_reset = true;

	if (!have_to_reset) {
		Uhci_command::access_t cmd = io_ports.inw(UHCI_CMD);
		if (Uhci_command::Enable::get(cmd) ||
		    !Uhci_command::Config::get(cmd) ||
		    !Uhci_command::Global_suspend::get(cmd))
			have_to_reset = true;
	}

	if (!have_to_reset) {
		Uhci_irq_status::access_t intr = io_ports.inw(UHCI_INTR);
		if (intr & ~Uhci_irq_status::Resume::mask())
			have_to_reset = true;
	}

	if (!have_to_reset)
		return;

	config.write<Reg>(reg);

	io_ports.outw(UHCI_CMD,  Uhci_command::Reset::bits(1));
	io_ports.outw(UHCI_INTR, 0);
	io_ports.outw(UHCI_CMD,  0);

	if (cfg.vendor_id == 0x8086)
		config.write<Uhci::Usb_resume_intel>(0);
}
