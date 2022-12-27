/*
 * \brief  Bridge related PCI information
 * \author Stefan Kalkowski
 * \date   2022-05-04
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/registry.h>
#include <pci/types.h>

using namespace Genode;

struct Bridge : Registry<Bridge>::Element
{
	Pci::Bdf   bdf;
	Pci::bus_t from;
	Pci::bus_t to;

	Registry<Bridge> sub_bridges {};

	Bridge(Registry<Bridge> & registry,
		   Pci::Bdf           bdf,
	       Pci::bus_t         from,
	       Pci::bus_t         to)
	:
		Registry<Bridge>::Element(registry, *this),
		bdf(bdf), from(from), to(to) { }

	bool behind(Pci::bus_t bus) {
		return from <= bus && bus <= to; }

	template <typename FN>
	void find_bridge(Pci::bus_t bus, FN const & fn) {
		if (!behind(bus))
			return;

		bool found = false;
		sub_bridges.for_each([&] (Bridge & b) {
			if (!b.behind(bus))
				return;
			b.find_bridge(bus, fn);
			found = true;
		});

		if (!found) fn(*this);
	}
};
