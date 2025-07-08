/*
 * \brief  Intel IOMMU Root Table implementation
 * \author Johannes Schlatow
 * \date   2023-08-31
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <intel/root_table.h>
#include <intel/context_table.h>
#include <intel/report_helper.h>

static void attribute_hex(Genode::Generator &g, char const * name,
                          unsigned long long value)
{
	g.attribute(name, Genode::String<32>(Genode::Hex(value)));
}


void Intel::Root_table::generate(Generator &g,
                                 Report_helper &report_helper)
{
	for_each([&] (uint8_t bus) {
		if (!present(bus))
			return;

		addr_t ctx_addr = address(bus);

		g.node("root_entry", [&] () {
			g.attribute("bus", bus);
			attribute_hex(g, "context_table", ctx_addr);

			auto fn = [&] (Context_table &context) {
				context.generate(g, report_helper);
			};

			/* dump context table */
			report_helper.with_table<Context_table>(ctx_addr, fn);
		});
	});
}
