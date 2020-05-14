/*
 * \brief  Platform driver - Device model policy
 * \author Stefan Kalkowski
 * \date   2020-05-13
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <device.h>

using Driver::Device_model;
using Driver::Device;

struct Irq_update_policy : Genode::List_model<Device::Irq>::Update_policy
{
	Genode::Allocator & alloc;

	Irq_update_policy(Genode::Allocator & alloc) : alloc(alloc) {}

	void destroy_element(Element & irq) {
		Genode::destroy(alloc, &irq); }

	Element & create_element(Genode::Xml_node node)
	{
		unsigned number = node.attribute_value<unsigned>("number", 0);
		return *(new (alloc) Element(number));
	}

	void update_element(Element &, Genode::Xml_node) {}

	static bool element_matches_xml_node(Element const & irq, Genode::Xml_node node)
	{
		unsigned number = node.attribute_value<unsigned>("number", 0);
		return number == irq.number;
	}

	static bool node_is_element(Genode::Xml_node node)
	{
		using Name = Genode::String<16>;
		return node.has_type("resource") &&
		       (node.attribute_value("name", Name()) == "IRQ");
	}
};


struct Io_mem_update_policy : Genode::List_model<Device::Io_mem>::Update_policy
{
	Genode::Allocator & alloc;

	Io_mem_update_policy(Genode::Allocator & alloc) : alloc(alloc) {}

	void destroy_element(Element & iomem) {
		Genode::destroy(alloc, &iomem); }

	Element & create_element(Genode::Xml_node node)
	{
		Genode::addr_t base = node.attribute_value<Genode::addr_t>("address", 0);
		Genode::size_t size = node.attribute_value<Genode::size_t>("size",    0);
		return *(new (alloc) Element(base, size));
	}

	void update_element(Element &, Genode::Xml_node) {}

	static bool element_matches_xml_node(Element const & iomem, Genode::Xml_node node)
	{
		Genode::addr_t base = node.attribute_value<Genode::addr_t>("address", 0);
		Genode::size_t size = node.attribute_value<Genode::size_t>("size",    0);
		return (base == iomem.base) && (size == iomem.size);
	}

	static bool node_is_element(Genode::Xml_node node)
	{
		bool iomem = node.attribute_value("name", Genode::String<16>())
		             == "IO_MEM";
		return node.has_type("resource") && iomem;
	}
};


struct Property_update_policy : Genode::List_model<Device::Property>::Update_policy
{
	Genode::Allocator & alloc;

	Property_update_policy(Genode::Allocator & alloc) : alloc(alloc) {}

	void destroy_element(Element & p) {
		Genode::destroy(alloc, &p); }

	Element & create_element(Genode::Xml_node node)
	{
		return *(new (alloc)
			Element(node.attribute_value("name",  Element::Name()),
			        node.attribute_value("value", Element::Value())));
	}

	void update_element(Element &, Genode::Xml_node) {}

	static bool element_matches_xml_node(Element const & prop, Genode::Xml_node node)
	{
		Element::Name  n = node.attribute_value("name", Element::Name());
		Element::Value v = node.attribute_value("value", Element::Value());
		return (n == prop.name) && (v == prop.value);
	}

	static bool node_is_element(Genode::Xml_node node) {
		return node.has_type("property"); }
};


void Device_model::destroy_element(Device & device)
{
	{
		Irq_update_policy policy(_alloc);
		device._irq_list.destroy_all_elements(policy);
	}

	{
		Io_mem_update_policy policy(_alloc);
		device._io_mem_list.destroy_all_elements(policy);
	}

	{
		Property_update_policy policy(_alloc);
		device._property_list.destroy_all_elements(policy);
	}

	Genode::destroy(_alloc, &device);
}


Device & Device_model::create_element(Genode::Xml_node node)
{
	Device::Name name = node.attribute_value("name", Device::Name());
	return *(new (_alloc) Device(name));
}


void Device_model::update_element(Device & device,
                                  Genode::Xml_node node)
{
	{
		Irq_update_policy policy(_alloc);
		device._irq_list.update_from_xml(policy, node);
	}

	{
		Io_mem_update_policy policy(_alloc);
		device._io_mem_list.update_from_xml(policy, node);
	}

	{
		Property_update_policy policy(_alloc);
		device._property_list.update_from_xml(policy, node);
	}
}


bool Device_model::element_matches_xml_node(Device const & dev,
                                            Genode::Xml_node n) {
	return dev.name() == n.attribute_value("name", Device::Name()); }


bool Device_model::node_is_element(Genode::Xml_node node) {
	return node.has_type("device"); }
