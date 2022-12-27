/*
 * \brief  Interrupt related ACPI information in list models
 * \author Stefan Kalkowski
 * \date   2021-12-12
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


#include <base/heap.h>
#include <pci/types.h>
#include <util/list_model.h>
#include <util/register.h>

#include <bridge.h>

using namespace Genode;
using namespace Pci;


struct Irq_override : List_model<Irq_override>::Element
{
	struct Flags : Register<8>
	{
		struct Polarity : Bitfield<0, 2>
		{
			enum { HIGH = 1, LOW = 3 };
		};

		struct Mode : Bitfield<2, 2>
		{
			enum { EDGE = 1, LEVEL = 3 };
		};
	};

	irq_line_t from;
	irq_line_t to;

	Flags::access_t flags;

	Irq_override(irq_line_t      from,
	             irq_line_t      to,
	             Flags::access_t flags)
	: from(from), to(to), flags(flags) {}

	void generate(Xml_generator & generator, irq_line_t & irq)
	{
		if (irq != from)
			return;

		irq = to;

		using Polarity = Flags::Polarity;
		Flags::access_t polarity = Polarity::get(flags);

		if (polarity == Polarity::HIGH)
			generator.attribute("polarity", "high");
		if (polarity == Polarity::LOW)
			generator.attribute("polarity", "low");

		using Mode = Irq_override::Flags::Mode;
		Flags::access_t mode = Mode::get(flags);

		if (mode == Mode::EDGE)
			generator.attribute("mode", "edge");
		if (mode == Mode::LEVEL)
			generator.attribute("mode", "level");
	}
};


struct Irq_override_policy : List_model<Irq_override>::Update_policy
{
	Heap & heap;

	void destroy_element(Irq_override & irq) {
		destroy(heap, &irq); }

	Irq_override & create_element(Xml_node node)
	{
		return *(new (heap)
			Irq_override(node.attribute_value<uint8_t>("irq",   0xff),
			             node.attribute_value<uint8_t>("gsi",   0xff),
			             node.attribute_value<uint8_t>("flags", 0)));
	}

	void update_element(Irq_override &, Xml_node) {}

	static bool element_matches_xml_node(Irq_override const & irq,
	                                     Genode::Xml_node     node) {
		return irq.from == node.attribute_value("irq", ~0U); }

	static bool node_is_element(Genode::Xml_node node) {
		return node.has_type("irq_override"); }

	Irq_override_policy(Heap & heap) : heap(heap) {}
};


struct Irq_routing : List_model<Irq_routing>::Element
{
	Bdf        bridge_bdf;
	dev_t      dev;
	irq_pin_t  pin;
	irq_line_t to;

	Irq_routing(Bdf        bridge_bdf,
	            dev_t      dev,
	            irq_pin_t  pin,
	            irq_line_t to)
	:
		bridge_bdf(bridge_bdf),
		dev(dev), pin(pin), to(to) {}

	void route(Bridge     & bridge,
	           dev_t        device,
	           irq_pin_t    p,
	           irq_line_t & irq)
	{
		if (!(bridge_bdf == bridge.bdf && dev == device && pin == p))
			return;

		irq = to;
	}
};


struct Irq_routing_policy : List_model<Irq_routing>::Update_policy
{
	Heap & heap;

	void destroy_element(Irq_routing & irq) {
		destroy(heap, &irq); }

	Irq_routing & create_element(Xml_node node)
	{
		rid_t bridge_bdf = node.attribute_value<rid_t>("bridge_bdf", 0xff);
		return *(new (heap)
			Irq_routing(Bdf::bdf(bridge_bdf),
			            node.attribute_value<uint8_t>("device",     0xff),
			            node.attribute_value<uint8_t>("device_pin", 0xff),
			            node.attribute_value<uint8_t>("gsi",        0xff)));
	}

	void update_element(Irq_routing &, Xml_node) {}

	static bool element_matches_xml_node(Irq_routing const & ir,
	                                     Genode::Xml_node    node)
	{
		rid_t bridge_bdf = node.attribute_value<rid_t>("bridge_bdf", 0xff);
		return ir.bridge_bdf == Bdf::bdf(bridge_bdf) &&
		       ir.dev == node.attribute_value<uint8_t>("device",     0xff) &&
		       ir.pin == node.attribute_value<uint8_t>("device_pin", 0xff);
	}

	static bool node_is_element(Genode::Xml_node node) {
		return node.has_type("routing"); }

	Irq_routing_policy(Heap & heap) : heap(heap) {}
};
