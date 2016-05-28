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

#include "pci_session_component.h"
#include "irq.h"


namespace Nonpci { class Ps2; }

class Nonpci::Ps2 : public Platform::Device_component
{
	private:

		enum {
			IRQ_KEYBOARD     = 1,
			IRQ_MOUSE        = 12,

			ACCESS_WIDTH     = 1,                                               
			REG_DATA         = 0x60,                                            
			REG_STATUS       = 0x64,  
		};

		Platform::Irq_session_component _irq_mouse;
		Genode::Io_port_connection      _data;
		Genode::Io_port_connection      _status;

	public:

		Ps2(Genode::Rpc_entrypoint * ep, Platform::Session_component * session)
		:
			Platform::Device_component(ep, session, IRQ_KEYBOARD),
			_irq_mouse(IRQ_MOUSE, ~0UL),
			_data(REG_DATA, ACCESS_WIDTH), _status(REG_STATUS, ACCESS_WIDTH)
		{
			ep->manage(&_irq_mouse);
		}

		~Ps2() {
			ep()->dissolve(&_irq_mouse);
		}

		Genode::Irq_session_capability irq(Genode::uint8_t virt_irq) override
		{
			switch (virt_irq) {
				case 0:
					PINF("PS2 uses IRQ, vector 0x%x", IRQ_KEYBOARD);
					return Device_component::irq(virt_irq);
				case 1:
					PINF("PS2 uses IRQ, vector 0x%x", IRQ_MOUSE);
					return _irq_mouse.cap();
				default:
					return Genode::Irq_session_capability();
			}
		}

		Genode::Io_port_session_capability io_port(Genode::uint8_t io_port) override
		{
			if (io_port == 0)
				return _data.cap();
			if (io_port == 1)
				return _status.cap();

			return Genode::Io_port_session_capability();
		}

		Genode::Io_mem_session_capability io_mem(Genode::uint8_t,
		                                         Genode::Cache_attribute,
		                                         Genode::addr_t,
		                                         Genode::size_t) override
		{
			return Genode::Io_mem_session_capability();
		}
};


/**
 * Platform session component devices which are non PCI devices, e.g. PS2
 */
Platform::Device_capability Platform::Session_component::device(String const &name) {

	if (!name.valid_string())
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
		throw Out_of_metadata();
	} catch (Genode::Parent::Service_denied) {
		return Device_capability();
	}
}
