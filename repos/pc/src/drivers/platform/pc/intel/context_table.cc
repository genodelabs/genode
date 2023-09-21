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

static void attribute_hex(Genode::Xml_generator & xml, char const * name,
                          unsigned long long value)
{
	xml.attribute(name, Genode::String<32>(Genode::Hex(value)));
}


void Intel::Context_table::generate(Xml_generator & xml,
                                    Env           & env,
                                    Report_helper & report_helper)
{
	for_each(0, [&] (Pci::rid_t id) {
		if (!present(id))
			return;

		xml.node("context_entry", [&] () {
			addr_t stage2_addr = stage2_pointer(id);

			xml.attribute("device",   Pci::Bdf::Routing_id::Device::get(id));
			xml.attribute("function", Pci::Bdf::Routing_id::Function::get(id));
			attribute_hex(xml, "hi",           hi(id));
			attribute_hex(xml, "lo",           lo(id));
			attribute_hex(xml, "domain",       domain(id));
			attribute_hex(xml, "agaw",         agaw(id));
			attribute_hex(xml, "type",         translation_type(id));
			attribute_hex(xml, "stage2_table", stage2_addr);
			xml.attribute("fault_processing", !fault_processing_disabled(id));

			switch (agaw(id)) {
				case Hi::Address_width::AGAW_3_LEVEL:
					using Table3 = Intel::Level_3_translation_table;

					/* dump stage2 table */
					report_helper.with_table<Table3>(stage2_addr,
						[&] (Table3 & stage2_table) {
							stage2_table.generate(xml, env, report_helper); });
					break;
				case Hi::Address_width::AGAW_4_LEVEL:
					using Table4 = Intel::Level_4_translation_table;

					/* dump stage2 table */
					report_helper.with_table<Table4>(stage2_addr,
						[&] (Table4 & stage2_table) {
							stage2_table.generate(xml, env, report_helper); });
					break;
				default:
					xml.node("unsupported-agaw-error", [&] () {});
			}
		});
	});
}

