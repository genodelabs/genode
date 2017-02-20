/*
 * \brief   Boot information
 * \author  Stefan Kalkowski
 * \date    2016-10-26
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _BOOTINFO_H_
#define _BOOTINFO_H_

#include <core_mmio.h>
#include <translation_table.h>
#include <translation_table_allocator_tpl.h>

namespace Genode { struct Bootinfo; }

struct Genode::Bootinfo
{
	using Table                   = Translation_table;
	static constexpr size_t COUNT = Table::CORE_TRANS_TABLE_COUNT;
	using Table_allocator         = Translation_table_allocator_tpl<COUNT>;
	using Mapping_pool            = Array<Mapping, 32>;

	Table            * const table;
	Table_allocator  * const table_allocator;
	Mapping_pool       const elf_mappings;
	Core_mmio          const core_mmio;
	Memory_region_array      ram_regions;

	Bootinfo(Table            * const table,
	         Table_allocator  * const table_alloc,
	         Mapping_pool       const elf_mappings,
	         Core_mmio          const core_mmio)
	: table(table), table_allocator(table_alloc),
	  elf_mappings(elf_mappings), core_mmio(core_mmio) {}
};

#endif /* _BOOTINFO_H_ */
