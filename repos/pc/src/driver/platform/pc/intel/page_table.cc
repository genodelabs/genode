/*
 * \brief  x86_64 DMAR page table definitions
 * \author Johannes Schlatow
 * \date   2023-11-06
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <intel/page_table.h>

void Intel::Level_1_translation_table::generate(Genode::Generator &g, Report_helper const &) const
{
	using Descriptor = Level_1_descriptor;

	for (unsigned long i = 0; i < MAX_ENTRIES; i++)
		if (Descriptor::present(_entries[i]))
			Descriptor::generate_page(i, _entries[i], g);
}


void Intel::Level_2_translation_table::generate(Genode::Generator &g,
                                                Report_helper const &report_helper) const
{
	using Descriptor = Page_directory_descriptor<SIZE_LOG2_2MB>;

	for (unsigned long i = 0; i < MAX_ENTRIES; i++) {
		auto e = _entries[i];

		switch (Descriptor::type(e)) {
		case Page_table_entry::INVALID: break;
		case Page_table_entry::BLOCK: Descriptor::generate_page(i, e, g);
		                              break;
		case Page_table_entry::TABLE:
			Descriptor::generate<Level_1_translation_table>(i, e, g, report_helper);
		};
	}
}


void Intel::Level_3_translation_table::generate(Genode::Generator &g,
                                                Report_helper const &report_helper) const
{
	using Descriptor = Page_directory_descriptor<SIZE_LOG2_1GB>;

	for (unsigned long i = 0; i < MAX_ENTRIES; i++) {
		auto e = _entries[i];

		switch (Descriptor::type(e)) {
		case Page_table_entry::INVALID: break;
		case Page_table_entry::BLOCK: Descriptor::generate_page(i, e, g);
		                              break;
		case Page_table_entry::TABLE:
			Descriptor::generate<Level_2_translation_table>(i, e, g, report_helper);
		};
	}
}


void Intel::Level_4_translation_table::generate(Genode::Generator &g,
                                                Report_helper const &report_helper) const
{
	using Descriptor = Level_4_descriptor;

	for (unsigned long i = 0; i < MAX_ENTRIES; i++) {
		auto e = _entries[i];

		if (!Descriptor::present(e))
			continue;

		Descriptor::generate<Level_3_translation_table>(i, e, g, report_helper);
	}
}
