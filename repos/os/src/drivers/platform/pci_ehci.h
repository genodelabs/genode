/*
 * \brief  Platform driver - PCI EHCI utilities
 * \author Stefan Kalkowski
 * \date   2022-09-28
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
	static void pci_ehci_quirks(Env &, Device const &,
	                            Device::Pci_config const &, Pci::Config const &);
}


void Driver::pci_ehci_quirks(Env                      & env,
                             Device             const & dev,
                             Device::Pci_config const & cfg,
                             Pci::Config        const & pci_config)
{
	enum { EHCI_CLASS_CODE = 0xc0320 };

	if (cfg.class_code != EHCI_CLASS_CODE)
		return;

	/* EHCI host controller register definitions */
	struct Ehci : Mmio<0x44>
	{
		struct Capability_parameters : Register<0x8, 32>
		{
			struct Extended_cap_pointer : Bitfield<8,8> {};
		};
		struct Configure_flag : Register<0x40, 32> {};

		using Mmio::Mmio;
	};

	struct Ehci_pci : Mmio<0x64>
	{
		struct Port_wake : Register<0x62, 16> {};

		using Mmio::Mmio;
	};

	/* PCI extended capability for EHCI */
	struct Cap : Mmio<0x8>
	{
		struct Pointer : Register<0x0, 16>
		{
			struct Id   : Bitfield<0, 8> { enum { SYNC = 1 }; };
			struct Next : Bitfield<8, 8> { };
		};

		struct Bios_semaphore            : Register<0x2, 8> { };
		struct Os_semaphore              : Register<0x3, 8> { };
		struct Usb_legacy_control_status : Register<0x4, 32> {};

		using Mmio::Mmio;
	};

	/* find ehci controller registers behind PCI bar zero */
	dev.for_each_io_mem([&] (unsigned, Device::Io_mem::Range range,
	                         Device::Pci_bar bar, bool)
	{
		if (!bar.valid() || bar.number != 0)
			return;

		Ehci_pci pw(pci_config.range());

		static constexpr size_t IO_MEM_SIZE = 0x1000;

		Attached_io_mem_dataspace iomem(env, range.start, IO_MEM_SIZE);
		Ehci ehci({iomem.local_addr<char>(), IO_MEM_SIZE});
		addr_t offset =
			ehci.read<Ehci::Capability_parameters::Extended_cap_pointer>();

		/* iterate over EHCI extended capabilities */
		while (offset) {
			Cap cap(pci_config.range_at(offset));
			if (cap.read<Cap::Pointer::Id>() != Cap::Pointer::Id::SYNC)
				break;

			bool bios_owned = cap.read<Cap::Bios_semaphore>();

			if (bios_owned) cap.write<Cap::Os_semaphore>(1);

			enum { MAX_ROUNDS = 1000000 };
			unsigned rounds = MAX_ROUNDS;
			while (cap.read<Cap::Bios_semaphore>() && --rounds) ;

			if (!rounds) cap.write<Cap::Bios_semaphore>(0);

			cap.write<Cap::Usb_legacy_control_status>(0);

			if (bios_owned) ehci.write<Ehci::Configure_flag>(0);

			offset = cap.read<Cap::Pointer::Next>();
		}
	});
}
