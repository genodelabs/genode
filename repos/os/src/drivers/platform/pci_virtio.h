/*
 * \brief  Platform driver - PCI Virtio utilities
 * \author Stefan Kalkowski
 * \date   2022-09-19
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <util/mmio.h>
#include <pci/config.h>
#include <device.h>

namespace Driver {
	void pci_virtio_info(Device const &, Device::Pci_config const &, Env &,
	                     Xml_generator &);
}


void Driver::pci_virtio_info(Device             const & dev,
                             Device::Pci_config const & cfg,
                             Env                      & env,
                             Xml_generator            & xml)
{
	enum { VENDOR_RED_HAT = 0x1af4 };

	if (cfg.vendor_id != VENDOR_RED_HAT)
		return;

	struct Virtio : Pci::Config
	{
		struct Capability : Pci::Config::Pci_capability
		{
			enum { COMMON = 1, NOTIFY = 2, ISR = 3, DEVICE = 4 };

			struct Type          : Register<0x3, 8>  {};
			struct Bar           : Register<0x4, 8>  {};
			struct Offset        : Register<0x8, 32> {};
			struct Length        : Register<0xc, 32> {};
			struct Offset_factor : Register<0x10, 32> {};

			using Pci::Config::Pci_capability::Pci_capability;

			bool valid()
			{
				switch (read<Type>()) {
					case COMMON:
					case NOTIFY:
					case ISR:
					case DEVICE:
						return true;
				};
				return false;
			}

			String<16> name()
			{
				switch (read<Type>()) {
					case COMMON: return "common";
					case NOTIFY: return "notify";
					case ISR:    return "irq_status";
					case DEVICE: return "device";
				};
				return "unknown";
			}
		};

		using Pci::Config::Config;

		void capability(Capability           & cap,
		                Driver::Device const & dev,
		                Xml_generator        & xml)
		{
			unsigned idx = ~0U;
			dev.for_each_io_mem([&] (unsigned i,
			                         Driver::Device::Io_mem::Range,
			                         Driver::Device::Pci_bar bar, bool) {
				if (bar.number == cap.read<Capability::Bar>()) idx = i; });

			xml.node("virtio_range", [&] () {
				xml.attribute("type",   cap.name());
				xml.attribute("index",  idx);
				xml.attribute("offset", cap.read<Capability::Offset>());
				xml.attribute("size",   cap.read<Capability::Length>());
				if (cap.read<Capability::Type>() == Capability::NOTIFY)
					xml.attribute("factor",
					              cap.read<Capability::Offset_factor>());
			});
		}

		void for_each_capability(Driver::Device const & dev,
		                         Xml_generator        & xml)
		{
			if (!read<Status::Capabilities>())
				return;

			uint16_t off = read<Capability_pointer>();
			while (off) {
				Capability cap(base() + off);
				if (cap.read<Pci_capability::Id>() ==
				    Pci_capability::Id::VENDOR &&
				    cap.valid())
					capability(cap, dev, xml);
				off = cap.read<Pci_capability::Pointer>();
			}
		}
	};

	Attached_io_mem_dataspace io_mem(env, cfg.addr, 0x1000);
	Virtio                    config((addr_t)io_mem.local_addr<void>());
	config.for_each_capability(dev, xml);
}

