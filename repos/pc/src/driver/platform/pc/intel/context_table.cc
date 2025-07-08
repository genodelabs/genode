/*
 * \brief  Intel IOMMU Context Table implementation
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
#include <intel/context_table.h>
#include <intel/report_helper.h>
#include <intel/page_table.h>

static void attribute_hex(Genode::Generator &g, char const * name,
                          unsigned long long value)
{
	g.attribute(name, Genode::String<32>(Genode::Hex(value)));
}


void Intel::Context_table::generate(Generator &g,
                                    Report_helper &report_helper)
{
	for_each(0, [&] (Pci::rid_t id) {
		if (!present(id))
			return;

		g.node("context_entry", [&] () {
			addr_t stage2_addr = stage2_pointer(id);

			g.attribute("device",   Pci::Bdf::Routing_id::Device::get(id));
			g.attribute("function", Pci::Bdf::Routing_id::Function::get(id));
			attribute_hex(g, "hi",           hi(id));
			attribute_hex(g, "lo",           lo(id));
			attribute_hex(g, "domain",       domain(id));
			attribute_hex(g, "agaw",         agaw(id));
			attribute_hex(g, "type",         translation_type(id));
			attribute_hex(g, "stage2_table", stage2_addr);
			g.attribute("fault_processing", !fault_processing_disabled(id));

			switch (agaw(id)) {
				case Hi::Address_width::AGAW_3_LEVEL:
					using Table3 = Intel::Level_3_translation_table;

					/* dump stage2 table */
					report_helper.with_table<Table3>(stage2_addr,
						[&] (Table3 &stage2_table) {
							stage2_table.generate(g, report_helper); });
					break;
				case Hi::Address_width::AGAW_4_LEVEL:
					using Table4 = Intel::Level_4_translation_table;

					/* dump stage2 table */
					report_helper.with_table<Table4>(stage2_addr,
						[&] (Table4 &stage2_table) {
							stage2_table.generate(g, report_helper); });
					break;
				default:
					g.node("unsupported-agaw-error", [&] () {});
			}
		});
	});
}

