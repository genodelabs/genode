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
};


struct Rmrr_policy : List_model<Rmrr>::Update_policy
{
	Heap & heap;

	void destroy_element(Rmrr & rmrr) {
		destroy(heap, &rmrr); }

	Rmrr & create_element(Xml_node node)
	{
		bus_t bus = 0;
		dev_t dev = 0;
		func_t fn = 0;
		addr_t start = node.attribute_value("start", 0UL);
		addr_t end   = node.attribute_value("end", 0UL);

		node.with_optional_sub_node("scope", [&] (Xml_node node) {
			bus = node.attribute_value<uint8_t>("bus_start", 0U);
			node.with_optional_sub_node("path", [&] (Xml_node node) {
				dev = node.attribute_value<uint8_t>("dev", 0);
				fn  = node.attribute_value<uint8_t>("func", 0);
			});
		});

		return *(new (heap) Rmrr({bus, dev, fn}, start, (end-start+1)));
	}

	void update_element(Rmrr &, Xml_node) {}

	static bool element_matches_xml_node(Rmrr const & rmrr,
	                                     Genode::Xml_node node)
	{
		addr_t start = node.attribute_value("start", 0UL);
		addr_t end   = node.attribute_value("end", 0UL);
		return rmrr.addr == start &&
		       rmrr.size == (end-start+1);
	}

	static bool node_is_element(Genode::Xml_node node) {
		return node.has_type("rmrr"); }

	Rmrr_policy(Heap & heap) : heap(heap) {}
};
