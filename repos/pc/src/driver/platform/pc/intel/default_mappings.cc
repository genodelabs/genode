/*
 * \brief  Default translation table structures
 * \author Johannes Schlatow
 * \date   2023-11-15
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


/* local includes */
#include <intel/default_mappings.h>

void Intel::Default_mappings::_insert_context(Managed_root_table & root,
                                              Pci::Bdf const &     bdf,
                                              addr_t               paddr,
                                              Domain_id            domain_id)
{
	using L3_table = Level_3_translation_table;
	using L4_table = Level_4_translation_table;

	switch (_levels) {
		case Translation_levels::LEVEL3:
			root.insert_context<L3_table::address_width()>(bdf, paddr, domain_id);
			break;
		case Translation_levels::LEVEL4:
			root.insert_context<L4_table::address_width()>(bdf, paddr, domain_id);
			break;
	}
}

void Intel::Default_mappings::insert_translation(addr_t va, addr_t pa,
                                                 size_t size, Page_flags flags,
                                                 uint32_t page_sizes)
{
	using L3_table = Level_3_translation_table;
	using L4_table = Level_4_translation_table;

	switch (_levels)
	{
		case Translation_levels::LEVEL3:
			_table_allocator.with_table<L3_table>(_default_table_phys,
				[&] (L3_table & t) {
					t.insert_translation(va, pa, size, flags, _table_allocator,
					                     _force_flush, page_sizes);
				}, [&] () {});
			break;
		case Translation_levels::LEVEL4:
			_table_allocator.with_table<L4_table>(_default_table_phys,
				[&] (L4_table & t) {
					t.insert_translation(va, pa, size, flags, _table_allocator,
					                     _force_flush, page_sizes);
				}, [&] () {});
			break;
	}
}


void Intel::Default_mappings::enable_device(Pci::Bdf const & bdf,
                                            Domain_id        domain_id)
{
	_insert_context(_root_table, bdf, _default_table_phys, domain_id);
}


void Intel::Default_mappings::copy_stage2(Managed_root_table & dst_root,
                                          Pci::Bdf const &     bdf)
{
	_root_table.with_stage2_pointer(bdf, [&] (addr_t phys_addr, Domain_id domain) {
		_insert_context(dst_root, bdf, phys_addr, domain); });
}


void Intel::Default_mappings::copy_stage2(Managed_root_table & dst_root)
{
	_root_table.for_each_stage2_pointer(
		[&] (Pci::Bdf const & bdf, addr_t phys_addr, Domain_id domain) {
			_insert_context(dst_root, bdf, phys_addr, domain); });
}


Intel::Default_mappings::~Default_mappings()
{
	/* destruct default translation table */
	switch(_levels) {
		case Translation_levels::LEVEL3:
			_table_allocator.destruct<Level_3_translation_table>(_default_table_phys);
			break;
		case Translation_levels::LEVEL4:
			_table_allocator.destruct<Level_4_translation_table>(_default_table_phys);
			break;
	}
}
