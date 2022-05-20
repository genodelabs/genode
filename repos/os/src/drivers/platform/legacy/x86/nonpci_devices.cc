/*
 * \brief  Non PCI devices, e.g. PS2
 * \author Alexander Boettcher
 * \author Christian Helmuth
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
#include "acpi_devices.h"

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

		Device_name_string name() const override { return "PS2"; }
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

		Device_name_string name() const override { return "PIT"; }
};


namespace Platform { namespace Nonpci {
	class Acpi;

	void acpi_device_registry(Platform::Acpi::Device_registry &);
} }


static Platform::Acpi::Device_registry *_acpi_device_registry;

void Platform::Nonpci::acpi_device_registry(Platform::Acpi::Device_registry &registry)
{
	_acpi_device_registry = &registry;
}


class Platform::Nonpci::Acpi : public Device_component
{
	private:

		Env &_env;

		Allocator &_session_heap;

		Platform::Acpi::Device const &_acpi_device;

		Irq_session_component *_irq0 = nullptr;

		/*
		 * Noncopyable
		 */
		Acpi(Acpi const &) = delete;
		Acpi &operator = (Acpi const &) = delete;

	public:

		Acpi(Platform::Acpi::Device const &acpi_device,
		     Env                       &env,
		     Attached_io_mem_dataspace &pciconf,
		     Session_component         &session,
		     Allocator                 &session_heap,
		     Allocator                 &global_heap,
		     Pci::Config::Delayer      &delayer,
		     Device_bars_pool          &devices_bars)
		:
			Device_component(env, pciconf, session, 0,
			                 global_heap, delayer, devices_bars),
			_env(env), _session_heap(session_heap), _acpi_device(acpi_device)
		{ }

		Device_name_string name() const override { return _acpi_device.hid(); }

		/* Platform::Device interface */

		void bus_address(unsigned char *bus, unsigned char *dev,
		                 unsigned char *fn) override
		{
			*bus = 0; *dev = 0; *fn  = 0;
		}

		unsigned short vendor_id() override { return ~0; }

		unsigned short device_id() override { return ~0; }

		unsigned class_code() override { return ~0; }

		Resource resource(int resource_id) override
		{
			using Acpi_device = Platform::Acpi::Device;

			return _acpi_device.resource(resource_id).convert<Resource>(
				[&] (Acpi_device::Resource r) {
					/* craft artificial BAR values from resource info */
					switch (r.type) {
					case Acpi_device::Resource::Type::IOMEM:
						return Resource((r.base & 0xfffffff0) | 0b0000, r.size);

					case Acpi_device::Resource::Type::IOPORT:
						return Resource((r.base & 0xfffffffc) | 0b01, r.size);

					case Acpi_device::Resource::Type::IRQ:
						return Resource();
					}
					return Resource();
				},
				[&] (Acpi_device::Invalid_resource) { return Resource(); });
		}

		unsigned config_read(unsigned char, Access_size) override
		{
			warning("ignore config_read from ACPI device ", _acpi_device.hid());
			return 0;
		}

		void config_write(unsigned char, unsigned, Access_size) override
		{
			warning("ignore config_write to ACPI device ", _acpi_device.hid());
		}

		Irq_session_capability irq(uint8_t v_id) override
		{
			using Acpi_device = Platform::Acpi::Device;

			/* TODO more than one IRQ */
			if (v_id != 0) {
				warning("ACPI device with more than one IRQ not supported (requested id ", v_id, ")");
				return Irq_session_capability();
			}
			if (_irq0) return _irq0->cap();

			/* TODO needs try see pci_device.cc ::iomem() */
			return _acpi_device.irq(v_id).convert<Irq_session_capability>(
				[&] (Acpi_device::Resource r) {
					Platform::Irq_session_component &irq =
						*new(_session_heap)
						Platform::Irq_session_component(r.base, ~0UL, _env, _session_heap,
						                                Irq_session::TRIGGER_LEVEL,
						                                Irq_session::POLARITY_LOW);
					_env.ep().manage(irq);
					_irq0 = &irq;
					return irq.cap();
				},
				[&] (Acpi_device::Invalid_resource) { return Irq_session_capability(); });
		}

		Io_port_session_capability io_port(uint8_t v_id) override
		{
			using Acpi_device = Platform::Acpi::Device;

			/* TODO needs try see pci_device.cc ::iomem() */

			return _acpi_device.ioport(v_id).convert<Io_port_session_capability>(
				[&] (Acpi_device::Resource r) {
					Io_port_connection &ioport =
						*new (_session_heap) Io_port_connection(_env, r.base, r.size);
					return ioport.cap();
				},
				[&] (Acpi_device::Invalid_resource) { return Io_port_session_capability(); });
		}

		Io_mem_session_capability io_mem(uint8_t v_id,
		                                 Cache  /* ignored */,
		                                 addr_t /* ignored */,
		                                 size_t /* ignored */) override
		{
			using Acpi_device = Platform::Acpi::Device;

			/* TODO needs try see pci_device.cc ::iomem() */

			return _acpi_device.iomem(v_id).convert<Io_mem_session_capability>(
				[&] (Acpi_device::Resource r) {
					Io_mem_connection &iomem =
						*new (_session_heap) Io_mem_connection(_env, r.base, r.size);
					return iomem.cap();
				},
				[&] (Acpi_device::Invalid_resource) { return Io_mem_session_capability(); });
		}
};


/**
 * Platform session component devices which are non PCI devices, e.g. PS2
 */
Platform::Device_capability Platform::Session_component::device(Device_name const &name)
{
	if (!name.valid_string())
		return Device_capability();

	Device_name_string const device_name { name.string() };

	enum class Type { UNKNOWN, PS2, PIT, ACPI } device_type { Type::UNKNOWN };

	Platform::Acpi::Device const *acpi_device = nullptr;

	if (device_name == "PS2")
		device_type = Type::PS2;

	else if (device_name == "PIT")
		device_type = Type::PIT;

	else if (_acpi_device_registry)
		device_type = _acpi_device_registry->lookup(device_name).convert<Type>(
			[&] (Acpi::Device const *device) {
				acpi_device = device;
				return Type::ACPI;
			},
			[&] (Acpi::Device_registry::Lookup_failed) { return Type::UNKNOWN; });

	bool const found = device_type != Type::UNKNOWN;

	if (!found) {
		error("unknown device name '", device_name, "'");
		return Device_capability();
	}

	if (!permit_device(device_name.string())) {
		error("denied access to device '", device_name, "' for "
		      "session '", _label, "'");
		return Device_capability();
	}

	try {
		Device_component * dev = nullptr;

		switch (device_type) {
			case Type::PS2:
				dev = new (_md_alloc) Nonpci::Ps2(_env, _pciconf, *this,
				                                  _global_heap, _delayer,
				                                  _devices_bars);
				break;
			case Type::PIT:
				dev = new (_md_alloc) Nonpci::Pit(_env, _pciconf, *this,
				                                  _global_heap, _delayer,
				                                  _devices_bars);
				break;
			case Type::ACPI:
				dev = new (_md_alloc) Nonpci::Acpi(*acpi_device,
				                                   _env, _pciconf, *this,
				                                   _md_alloc,
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
