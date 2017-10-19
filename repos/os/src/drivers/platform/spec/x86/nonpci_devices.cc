/*
 * \brief  Non PCI devices, e.g. PS2
 * \author Alexander Boettcher
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include "pci_session_component.h"
#include "irq.h"


namespace Nonpci { class Ps2; class Pit; }

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

		Genode::Rpc_entrypoint         &_ep;
		Platform::Irq_session_component _irq_mouse;
		Genode::Io_port_connection      _data;
		Genode::Io_port_connection      _status;

	public:

		Ps2(Genode::Env &env,
		    Genode::Attached_io_mem_dataspace &pciconf,
		    Platform::Session_component &session,
		    Genode::Allocator &heap_for_irq)
		:
			Platform::Device_component(env, pciconf, session, IRQ_KEYBOARD, heap_for_irq),
			_ep(env.ep().rpc_ep()),
			_irq_mouse(IRQ_MOUSE, ~0UL, env, heap_for_irq),
			_data(env, REG_DATA, ACCESS_WIDTH),
			_status(env, REG_STATUS, ACCESS_WIDTH)
		{
			_ep.manage(&_irq_mouse);
		}

		~Ps2() { _ep.dissolve(&_irq_mouse); }

		Genode::Irq_session_capability irq(Genode::uint8_t virt_irq) override
		{
			switch (virt_irq) {
				case 0:
					Genode::log("PS2 uses IRQ, vector ", Genode::Hex(IRQ_KEYBOARD));
					return Device_component::irq(virt_irq);
				case 1:
					Genode::log("PS2 uses IRQ, vector ", Genode::Hex(IRQ_MOUSE));
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


class Nonpci::Pit : public Platform::Device_component
{
	private:

		enum {
			IRQ_PIT     = 0,

			PIT_PORT    = 0x40,
			PORTS_WIDTH = 4
		};

		Genode::Io_port_connection _ports;

	public:

		Pit(Genode::Env &env,
		    Genode::Attached_io_mem_dataspace &pciconf,
		    Platform::Session_component &session,
		    Genode::Allocator &heap_for_irq)
		:
			Platform::Device_component(env, pciconf, session, IRQ_PIT, heap_for_irq),
			_ports(env, PIT_PORT, PORTS_WIDTH)
		{ }

		Genode::Io_port_session_capability io_port(Genode::uint8_t io_port) override
		{
			if (io_port == 0)
				return _ports.cap();

			return Genode::Io_port_session_capability();
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
	const char * devices []  = { "PS2", "PIT" };
	unsigned      devices_i  = 0;

	for (; devices_i < sizeof(devices) / sizeof(devices[0]); devices_i++)
		if (!strcmp(device_name, devices[devices_i]))
			break;

	if (devices_i >= sizeof(devices) / sizeof(devices[0])) {
		Genode::error("unknown '", device_name, " device name");
		return Device_capability();
	}

	if (!permit_device(devices[devices_i])) {
		Genode::error("denied access to device '", device_name, "' for "
		              "session '", _label, "'");
		return Device_capability();
	}

	try {
		Device_component * dev = nullptr;

		switch(devices_i) {
			case 0:
				dev = new (_md_alloc) Nonpci::Ps2(_env, _pciconf, *this, _global_heap);
				break;
			case 1:
				dev = new (_md_alloc) Nonpci::Pit(_env, _pciconf, *this, _global_heap);
				break;
			default:
				return Device_capability();
		}

		_device_list.insert(dev);
		return _env.ep().rpc_ep().manage(dev);
	}
	catch (Genode::Out_of_ram)     { throw; }
	catch (Genode::Service_denied) { return Device_capability(); }
}
