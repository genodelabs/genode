/*
 * \brief  ACPI device information from config
 * \author Christian Helmuth
 * \date   2022-05-16
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include "acpi_devices.h"

namespace Platform { namespace Acpi {
	struct Device_impl;

	typedef String<16> Str;
}}


void Platform::Acpi::Device::Resource::print(Output &o) const
{
	using Genode::print;
	switch (type) {
	case Type::IRQ:
		print(o, "IRQ [", base);
		print(o, " ", irq == Irq::EDGE ? "edge" : irq == Irq::LEVEL_LOW ? "level-low" : "level-high");
		print(o, "]");
		break;
	case Type::IOMEM:
		print(o, "IOMEM ");
		print(o, "[", Hex(base, Hex::Prefix::OMIT_PREFIX, Hex::PAD));
		print(o, "-", Hex(base + (size - 1), Hex::Prefix::OMIT_PREFIX, Hex::PAD));
		print(o, "]");
		break;
	case Type::IOPORT:
		print(o, "IOPORT ");
		print(o, "[", Hex((unsigned short)base, Hex::Prefix::OMIT_PREFIX, Hex::PAD));
		print(o, "-", Hex((unsigned short)(base + (size - 1)), Hex::Prefix::OMIT_PREFIX, Hex::PAD));
		print(o, "]");
		break;
	}
}


class Platform::Acpi::Device_impl : private Registry<Device>::Element,
                                    public Platform::Acpi::Device
{
	private:

		Hid _hid; /* ACPI Spec 6.1.5 Hardware ID */

		struct Resource_element : Registry<Resource_element>::Element
		{
			unsigned id;
			Resource res;

			Resource_element(Registry<Resource_element> &registry,
			                 unsigned                    id,
			                 Resource                    res)
			:
				Registry<Resource_element>::Element(registry, *this),
				id(id), res(res)
			{ }
		};

		Registry<Resource_element> _resource_registry { };

		Resource_result _lookup_resource(Resource::Type type, unsigned const id) const
		{
			Resource const *res = nullptr;
			_resource_registry.for_each([&] (Resource_element const &e) {
				if (e.res.type == type && e.id == id)
					res = &e.res;
			});

			if (res)
				return *res;
			else
				return Invalid_resource { };
		}

		unsigned _max_irq_id    { 0 };
		unsigned _max_iomem_id  { 0 };
		unsigned _max_ioport_id { 0 };

	public:

		Device_impl(Registry<Device> &registry,
		            Allocator &heap, Xml_node config)
		:
			Registry<Device>::Element(registry, *this),
			_hid(config.attribute_value("name", Hid("ACPI0000")))
		{
			config.for_each_sub_node("irq", [&] (Xml_node node) {
				auto irq = [&] (Str const &mode, Str const &polarity) {
					if (mode == "level") {
						if (polarity == "high")
							return Resource::Irq::LEVEL_HIGH;
						else
							return Resource::Irq::LEVEL_LOW;
					} else {
						return Resource::Irq::EDGE;
					}
				};
				new (heap)
					Resource_element {
						_resource_registry,
						_max_irq_id++,
						Resource {
							.type = Resource::Type::IRQ,
							.base = node.attribute_value("number", addr_t(0)),
							.irq  = irq(node.attribute_value("mode", Str("unchanged")),
							            node.attribute_value("polarity", Str("unchanged"))) } };
			});
			config.for_each_sub_node("io_mem", [&] (Xml_node node) {
				new (heap)
					Resource_element {
						_resource_registry,
						_max_iomem_id++,
						Resource {
							.type = Resource::Type::IOMEM,
							.base = node.attribute_value("address", addr_t(0)),
							.size = node.attribute_value("size", size_t(0)) } };
			});
			config.for_each_sub_node("io_port_range", [&] (Xml_node node) {
				new (heap)
					Resource_element {
						_resource_registry,
						_max_ioport_id++,
						Resource {
							.type = Resource::Type::IOPORT,
							.base = node.attribute_value("address", addr_t(0)),
							.size = node.attribute_value("size", size_t(0)) } };
			});
		}

		~Device_impl() { error("unexpected call of ", __func__); }

		/* Platform::Acpi::Device interface */

		Hid hid() const override { return _hid; }

		Resource_result resource(unsigned idx) const override
		{
			/*
			 * Index of all IOMEM and IOPORT resources - no IRQ!
			 *
			 * first _max_iomem_id IOMEM, then _max_ioport_id IOPORT
			 */
			if (idx < _max_iomem_id)
				return iomem(idx);
			else if (idx < _max_iomem_id + _max_ioport_id)
				return ioport(idx - _max_iomem_id);
			else
				return Invalid_resource { };
		}

		Resource_result irq(unsigned id) const override
		{
			return _lookup_resource(Resource::Type::IRQ, id);
		}

		Resource_result iomem(unsigned id) const override
		{
			return _lookup_resource(Resource::Type::IOMEM, id);
		}

		Resource_result ioport(unsigned id) const override
		{
			return _lookup_resource(Resource::Type::IOPORT, id);
		}
};


Platform::Acpi::Device_registry::Lookup_result
Platform::Acpi::Device_registry::lookup(Platform::Acpi::Device::Hid hid) const
{
	Device const *found = nullptr;

	this->for_each([&] (Device const &device) {
		if (device.hid() == hid)
			found = &device;
	});

	if (found)
		return found;
	else
		return Lookup_failed { };
}


void Platform::Acpi::Device_registry::init_devices(Allocator &heap, Xml_node config)
{
	/* init only once */
	if (_initialized)
		return;

	config.for_each_sub_node("device", [&] (Xml_node node) {
		if (node.attribute_value("type", Str()) == "acpi")
			new (heap) Device_impl(*this, heap, node);
	});

	_initialized = true;
}
