/*
 * \brief  Non PCI devices, e.g. PS2
 * \author Alexander Boettcher
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <irq_session/connection.h>
#include "pci_session_component.h"


namespace Nonpci { class Ps2; }

class Nonpci::Ps2 : public Pci::Device_component
{
	private:

		enum {
			IRQ_KEYBOARD     = 1,
			IRQ_MOUSE        = 12,
		};

		Genode::Irq_connection      _irq_mouse;

	public:

		Ps2(Genode::Rpc_entrypoint * ep, Pci::Session_component * session)
		:
			Pci::Device_component(ep, session, IRQ_KEYBOARD),
			_irq_mouse(IRQ_MOUSE)
		{ }

		Genode::Irq_session_capability irq(Genode::uint8_t virt_irq) override
		{
			switch (virt_irq) {
				case 0:
					return Device_component::irq(virt_irq);
				case 1:
					return _irq_mouse.cap();
				default:
					return Genode::Irq_session_capability();
			}
		}
};


/**
 * PCI session component devices which are non PCI devices, e.g. PS2
 */
Pci::Device_capability Pci::Session_component::device(String const &name) {

	if (!name.is_valid_string())
		return Device_capability();

	using namespace Genode;

	char const * device_name = name.string();
	const char * devices []  = { "PS2" };
	unsigned      devices_i  = 0;

	for (; devices_i < sizeof(devices) / sizeof(devices[0]); devices_i++)
		if (!strcmp(device_name, devices[devices_i]))
			break;

	if (devices_i >= sizeof(devices) / sizeof(devices[0])) {
		PERR("unknown '%s' device name", device_name);
		return Device_capability();
	}

	if (!permit_device(devices[devices_i])) {
		PERR("Denied access to device '%s' for session '%s'", device_name,
		     _label.string());
		return Device_capability();
	}

	try {
		Device_component * dev = nullptr;

		switch(devices_i) {
			case 0:
				dev = new (_md_alloc) Nonpci::Ps2(_ep, this);
				break;
			default:
				return Device_capability();
		}

		_device_list.insert(dev);
		return _ep->manage(dev);
	} catch (Genode::Allocator::Out_of_memory) {
		throw Pci::Device::Quota_exceeded();
	} catch (Genode::Parent::Service_denied) {
		return Device_capability();
	}
}
