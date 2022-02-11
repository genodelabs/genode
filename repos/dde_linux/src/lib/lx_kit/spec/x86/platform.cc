/*
 * \brief  Legacy platform session wrapper
 * \author Josef Soentgen
 * \date   2022-01-14
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/ram_allocator.h>
#include <util/xml_generator.h>

/* DDE includes */
#include <lx_kit/env.h>
#include <lx_kit/device.h>
#include <platform_session/device.h>


using Str = Genode::String<16>;

template <typename T, typename... TAIL>
static Str to_string(T const &arg, TAIL &&... args)
{
	return Str(arg, args...);
}


template <typename FN>
static void scan_resources(Legacy_platform::Device &device,
                              FN const &fn)
{
	using R = Legacy_platform::Device::Resource;
	for (unsigned resource_id = 0; resource_id < 6; resource_id++) {

		R const resource = device.resource(resource_id);
		if (resource.type() != R::INVALID)
			fn(resource_id, resource);
	}
}


static Genode::String<16> create_device_node(Genode::Xml_generator &xml,
                                             Legacy_platform::Device &device)
{
	struct {
		char const    *key;
		unsigned char  value;
	} bdf[3] = {
		{ .key = "bus",  .value = 0u },
		{ .key = "dev",  .value = 0u },
		{ .key = "func", .value = 0u },
	};
	device.bus_address(&bdf[0].value, &bdf[1].value, &bdf[2].value);

	/* start arbitrarily and count up */
	static unsigned char irq = 8;

	using namespace Genode;

	Str name = to_string("pci-",
	                     Hex(bdf[0].value, Hex::OMIT_PREFIX), ":",
	                     Hex(bdf[1].value, Hex::OMIT_PREFIX), ".",
	                     Hex(bdf[2].value, Hex::OMIT_PREFIX));

	xml.node("device", [&] () {
		xml.attribute("name", name);
		xml.attribute("type", "pci");

		for (auto i : bdf) {
			xml.node("property", [&] () {
				xml.attribute("name",  i.key);
				xml.attribute("value", to_string(i.value));
			});
		}

		xml.node("irq", [&] () {
			xml.attribute("number", irq++);
		});

		using R = Legacy_platform::Device::Resource;

		scan_resources(device, [&] (unsigned id, R const &r) {

			xml.node(r.type() == R::MEMORY ? "io_mem" : "io_port", [&] () {
				xml.attribute("phys_addr", to_string(Hex(r.base())));
				xml.attribute("size",      to_string(Hex(r.size())));
				xml.attribute("bar",       id);
			});
		});
	});

	return name;
}


Platform::Connection::Connection(Genode::Env &env)
:
	_env { env }
{
	try {
		_legacy_platform.construct(env);
	} catch (...) {
		Genode::error("could not construct legacy platform connection");
		throw;
	}

	/* empirically determined */
	_legacy_platform->upgrade_ram(32768);
	_legacy_platform->upgrade_caps(8);

	Genode::Xml_generator xml { _devices_node_buffer,
	                            sizeof (_devices_node_buffer),
	                            "devices", [&] () {
		_legacy_platform->with_upgrade([&] () {

			/* scan the virtual bus but limit to MAX_DEVICES */
			Legacy_platform::Device_capability cap { };
			for (auto &dev : _devices_list) {

				cap = _legacy_platform->next_device(cap, 0x0u, 0x0u);
				if (!cap.valid()) break;

				Legacy_platform::Device_client device { cap };
				Str name = create_device_node(xml, device);
				dev.construct(name, cap);
			}
		});
	} };

	_devices_node.construct(_devices_node_buffer,
	                        sizeof (_devices_node_buffer));
}


Legacy_platform::Device_capability
Platform::Connection::device_cap(char const *name)
{
	for (auto const &dev : _devices_list) {
		if (!dev.constructed())
			continue;

		if (dev->name == name)
			return dev->cap;
	}

	return Legacy_platform::Device_capability();
}


void Platform::Connection::update() { }


Genode::Ram_dataspace_capability
Platform::Connection::alloc_dma_buffer(size_t size, Cache)
{
	return _legacy_platform->with_upgrade([&] () {
		return _legacy_platform->alloc_dma_buffer(size, Genode::Cache::UNCACHED);
	});
}


void Platform::Connection::free_dma_buffer(Ram_dataspace_capability ds_cap)
{
	_legacy_platform->free_dma_buffer(ds_cap);
}


Genode::addr_t Platform::Connection::dma_addr(Ram_dataspace_capability ds_cap)
{
	return _legacy_platform->dma_addr(ds_cap);
}


static Legacy_platform::Device::Access_size convert(Platform::Device::Config_space::Access_size size)
{
	using PAS = Platform::Device::Config_space::Access_size;
	using LAS = Legacy_platform::Device::Access_size;
	switch (size) {
	case PAS::ACCESS_8BIT:
		return LAS::ACCESS_8BIT;
	case PAS::ACCESS_16BIT:
		 return LAS::ACCESS_16BIT;
	case PAS::ACCESS_32BIT:
		return LAS::ACCESS_32BIT;
	}

	return LAS::ACCESS_8BIT;
}


template <typename FN>
static void apply(Platform::Device const &device,
                  Genode::Xml_node const &devices,
                  FN const &fn)
{
	using namespace Genode;
	using N = Platform::Device::Name;

	auto lookup_device = [&] (Xml_node const &node) {
		if (node.attribute_value("name", N()) == device.name())
			fn(node);
	};

	devices.for_each_sub_node("device", lookup_device);
}


static unsigned bar_size(Platform::Device const &dev,
                         Genode::Xml_node const &devices, unsigned bar)
{
	if (bar > 6)
		return 0;

	using namespace Genode;

	unsigned val = 0;
	apply(dev, devices, [&] (Xml_node device) {
		device.for_each_sub_node("io_mem", [&] (Xml_node node) {
			if (node.attribute_value("bar", 6u) != bar)
				return;

			val = node.attribute_value("size", 0u);
		});
	});

	return val;
}


static unsigned char irq_line(Platform::Device const &dev,
                              Genode::Xml_node const &devices)
{
	using namespace Genode;

	enum : unsigned char { INVALID_IRQ_LINE = 0xffu };
	unsigned char irq = INVALID_IRQ_LINE;

	apply(dev, devices, [&] (Xml_node device) {
		device.for_each_sub_node("irq", [&] (Xml_node node) {
			irq = node.attribute_value("number", (unsigned char)INVALID_IRQ_LINE);
		});
	});

	return irq;
}


Platform::Device::Device(Connection &platform, Type)
: _platform { platform } { }


Platform::Device::Device(Connection &platform, Name name)
:
	_platform   { platform },
	_device_cap { _platform.device_cap(name.string()) },
	_name       { name }
{
	if (!_device_cap.valid()) {
		Genode::error(__func__, ": could not get device capability");
		throw -1;
	}
}


unsigned Platform::Device::Config_space::read(unsigned char address,
                                              Access_size size)
{
	// 32bit BARs only for now
	if (address >= 0x10 && address <= 0x24) {
		unsigned const bar = (address - 0x10) / 4;
		if (_device._bar_checked_for_size[bar]) {
			_device._bar_checked_for_size[bar] = 0;
			return bar_size(_device, *_device._platform._devices_node, bar);
		}
	}

	if (address == 0x3c)
		return irq_line(_device, *_device._platform._devices_node);

	if (address == 0x34)
		return 0u;

	if (address > 0x3f)
		return 0u;

	Legacy_platform::Device::Access_size const as = convert(size);
	Legacy_platform::Device_client device { _device._device_cap };
	return device.config_read(address, as);
}


void Platform::Device::Config_space::write(unsigned char address,
                                           unsigned value,
                                           Access_size size)
{
	// 32bit BARs only for now
	if (address >= 0x10 && address <= 0x24) {
		unsigned const bar = (address - 0x10) / 4;
		if (value == 0xffffffffu)
			_device._bar_checked_for_size[bar] = 1;
		return;
	}

	if (address != 0x04)
		return;

	Legacy_platform::Device::Access_size const as = convert(size);
	Legacy_platform::Device_client device { _device._device_cap };
	device.config_write(address, value, as);
}


Genode::size_t Platform::Device::Mmio::size() const
{
	return _attached_ds.constructed() ? _attached_ds->size() : 0;
}


void *Platform::Device::Mmio::_local_addr()
{
	if (!_attached_ds.constructed()) {
		Legacy_platform::Device_client device { _device._device_cap };

		Genode::uint8_t const id =
			device.phys_bar_to_virt((Genode::uint8_t)_index.value);

		Genode::Io_mem_session_capability io_mem_cap =
			device.io_mem(id);

		Io_mem_session_client io_mem_client(io_mem_cap);

		_attached_ds.construct(Lx_kit::env().env.rm(),
		                       io_mem_client.dataspace());
	}

	return _attached_ds->local_addr<void*>();
}


Platform::Device::Irq::Irq(Platform::Device &device, Index index)
:
	_device { device },
	_index { index }
{
	Legacy_platform::Device_client client { _device._device_cap };

	_irq.construct(client.irq((Genode::uint8_t)index.value));
}

void Platform::Device::Irq::ack()
{
	_irq->ack_irq();
}


void Platform::Device::Irq::sigh(Signal_context_capability sigh)
{
	_irq->sigh(sigh);
	_irq->ack_irq();
}


void Platform::Device::Irq::sigh_omit_initial_signal(Signal_context_capability sigh)
{
	_irq->sigh(sigh);
}


static Platform::Device::Config_space::Access_size access_size(unsigned len)
{
	using AS = Platform::Device::Config_space::Access_size;
	AS as = AS::ACCESS_8BIT;
	if (len == 4)      as = AS::ACCESS_32BIT;
	else if (len == 2) as = AS::ACCESS_16BIT;
	else               as = AS::ACCESS_8BIT;

	return as;
}


bool Lx_kit::Device::read_config(unsigned reg, unsigned len, unsigned *val)
{
	if (!_pdev.constructed())
		enable();

	if (!val)
		return false;

	using AS = Platform::Device::Config_space::Access_size;
	AS const as = access_size(len);

	*val = Platform::Device::Config_space(*_pdev).read((unsigned char)reg, as);
	return true;
}


bool Lx_kit::Device::write_config(unsigned reg, unsigned len, unsigned val)
{
	if (!_pdev.constructed())
		return false;

	using AS = Platform::Device::Config_space::Access_size;
	AS const as = access_size(len);

	Platform::Device::Config_space(*_pdev).write((unsigned char)reg, val, as);
	return true;
}
