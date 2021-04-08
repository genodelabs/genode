/*
 * \brief  Non PCI devices, e.g. PS2
 * \author Alexander Boettcher
 * \date   2015-04-17
 */

/*
 * Copyright (C) 2015-2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include "pci_session_component.h"
#include "irq.h"

namespace Platform { namespace Nonpci { class Ps2; class Pit; } }


class Platform::Nonpci::Ps2 : public Device_component
{
	private:

		enum {
			IRQ_KEYBOARD     = 1,
			IRQ_MOUSE        = 12,

			ACCESS_WIDTH     = 1,
			REG_DATA         = 0x60,
			REG_STATUS       = 0x64,
		};

		Rpc_entrypoint                 &_ep;
		Platform::Irq_session_component _irq_mouse;
		Io_port_connection              _data;
		Io_port_connection              _status;

	public:

		Ps2(Env                       &env,
		    Attached_io_mem_dataspace &pciconf,
		    Session_component         &session,
		    Allocator                 &heap_for_irq,
		    Pci::Config::Delayer      &delayer,
		    Device_bars_pool          &devices_bars)
		:
			Device_component(env, pciconf, session, IRQ_KEYBOARD,
			                 heap_for_irq, delayer, devices_bars),
			_ep(env.ep().rpc_ep()),
			_irq_mouse(IRQ_MOUSE, ~0UL, env, heap_for_irq),
			_data(env, REG_DATA, ACCESS_WIDTH),
			_status(env, REG_STATUS, ACCESS_WIDTH)
		{
			_ep.manage(&_irq_mouse);
		}

		~Ps2() { _ep.dissolve(&_irq_mouse); }

		Irq_session_capability irq(uint8_t virt_irq) override
		{
			switch (virt_irq) {
				case 0:
					log("PS2 uses IRQ, vector ", Hex(IRQ_KEYBOARD));
					return Device_component::irq(virt_irq);
				case 1:
					log("PS2 uses IRQ, vector ", Hex(IRQ_MOUSE));
					return _irq_mouse.cap();
				default:
					return Irq_session_capability();
			}
		}

		Io_port_session_capability io_port(uint8_t io_port) override
		{
			if (io_port == 0)
				return _data.cap();
			if (io_port == 1)
				return _status.cap();

			return Io_port_session_capability();
		}

		Io_mem_session_capability io_mem(uint8_t, Cache, addr_t, size_t) override
		{
			return Io_mem_session_capability();
		}

		String<5> name() const override { return "PS2"; }
};


class Platform::Nonpci::Pit : public Device_component
{
	private:

		enum {
			IRQ_PIT     = 0,

			PIT_PORT    = 0x40,
			PORTS_WIDTH = 4
		};

		Io_port_connection _ports;

	public:

		Pit(Env                       &env,
		    Attached_io_mem_dataspace &pciconf,
		    Session_component         &session,
		    Allocator                 &heap_for_irq,
		    Pci::Config::Delayer      &delayer,
		    Device_bars_pool          &devices_bars)
		:
			Device_component(env, pciconf, session, IRQ_PIT,
			                 heap_for_irq, delayer, devices_bars),
			_ports(env, PIT_PORT, PORTS_WIDTH)
		{ }

		Io_port_session_capability io_port(uint8_t io_port) override
		{
			if (io_port == 0)
				return _ports.cap();

			return Io_port_session_capability();
		}

		String<5> name() const override { return "PIT"; }
};


/**
 * Platform session component devices which are non PCI devices, e.g. PS2
 */
Platform::Device_capability Platform::Session_component::device(Device_name const &name)
{
	if (!name.valid_string())
		return Device_capability();

	char const * device_name = name.string();
	const char * devices []  = { "PS2", "PIT" };
	unsigned      devices_i  = 0;

	for (; devices_i < sizeof(devices) / sizeof(devices[0]); devices_i++)
		if (!strcmp(device_name, devices[devices_i]))
			break;

	if (devices_i >= sizeof(devices) / sizeof(devices[0])) {
		error("unknown '", device_name, " device name");
		return Device_capability();
	}

	if (!permit_device(devices[devices_i])) {
		error("denied access to device '", device_name, "' for "
		      "session '", _label, "'");
		return Device_capability();
	}

	try {
		Device_component * dev = nullptr;

		switch(devices_i) {
			case 0:
				dev = new (_md_alloc) Nonpci::Ps2(_env, _pciconf, *this,
				                                  _global_heap, _delayer,
				                                  _devices_bars);
				break;
			case 1:
				dev = new (_md_alloc) Nonpci::Pit(_env, _pciconf, *this,
				                                  _global_heap, _delayer,
				                                  _devices_bars);
				break;
			default:
				return Device_capability();
		}

		_device_list.insert(dev);
		return _env.ep().rpc_ep().manage(dev);
	}
	catch (Out_of_ram)     { throw; }
	catch (Service_denied) { return Device_capability(); }
}
