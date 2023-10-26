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

	bool matches(Xml_node const &node) const
	{
		return (from == node.attribute_value("irq", ~0U));
	}

	static bool type_matches(Xml_node const &node)
	{
		return node.has_type("irq_override");
	}
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

	bool matches(Xml_node const &node) const
	{
		rid_t const bdf = node.attribute_value<rid_t>("bridge_bdf", 0xff);
		return bridge_bdf == Bdf::bdf(bdf) &&
		       dev == node.attribute_value<uint8_t>("device",     0xff) &&
		       pin == node.attribute_value<uint8_t>("device_pin", 0xff);
	}

	static bool type_matches(Xml_node const &node)
	{
		return node.has_type("routing");
	}
};
