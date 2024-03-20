/*
 * \brief  IOAPIC reporting from ACPI information in list models
 * \author Johannes Schlatow
 * \date   2024-03-20
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


#include <util/list_model.h>

using namespace Genode;

struct Ioapic : List_model<Ioapic>::Element
{
	using Ioapic_name = String<16>;

	uint8_t  id;
	addr_t   addr;
	uint32_t base_irq;

	Ioapic(uint8_t id, addr_t addr, uint32_t base_irq)
	: id(id), addr(addr), base_irq(base_irq)
	{ }

	Ioapic_name name() const { return Ioapic_name("ioapic", id); }

	bool matches(Xml_node const &node) const
	{
		return id == node.attribute_value("id", 0UL);
	}

	static bool type_matches(Xml_node const &node)
	{
		return node.has_type("ioapic");
	}
};
