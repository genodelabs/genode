/*
 * \brief  Reserved memory region reporting from ACPI information in list models
 * \author Stefan Kalkowski
 * \date   2022-09-12
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


#include <base/heap.h>
#include <pci/types.h>
#include <util/list_model.h>

using namespace Genode;
using namespace Pci;


struct Rmrr : List_model<Rmrr>::Element
{
	Bdf    bdf;
	addr_t addr;
	size_t size;

	Rmrr(Bdf bdf, addr_t addr, size_t size)
	: bdf(bdf), addr(addr), size(size) {}

	bool matches(Xml_node const &node) const
	{
		addr_t start = node.attribute_value("start", 0UL);
		addr_t end   = node.attribute_value("end", 0UL);
		return addr == start &&
		       size == (end-start+1);
	}

	static bool type_matches(Xml_node const &node)
	{
		return node.has_type("rmrr");
	}
};
