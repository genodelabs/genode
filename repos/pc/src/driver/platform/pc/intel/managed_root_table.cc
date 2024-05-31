/*
 * \brief  Allocation and configuration helper for root and context tables
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
#include <intel/managed_root_table.h>

void Intel::Managed_root_table::remove_context(Pci::Bdf const & bdf,
                                               addr_t           phys_addr)
{
	_with_context_table(bdf.bus, [&] (Context_table & ctx) {
		Pci::rid_t      rid = Pci::Bdf::rid(bdf);

		if (ctx.stage2_pointer(rid) != phys_addr)
			error("Trying to remove foreign translation table for ", bdf);

		ctx.remove(rid, _force_flush);
	});
}


void Intel::Managed_root_table::remove_context(addr_t phys_addr)
{
	Root_table::for_each([&] (uint8_t bus) {
		_with_context_table(bus, [&] (Context_table & ctx) {
			Context_table::for_each(0, [&] (Pci::rid_t id) {
				if (!ctx.present(id))
					return;

				if (ctx.stage2_pointer(id) != phys_addr)
					return;

				Pci::Bdf bdf = { (Pci::bus_t) bus,
				                 (Pci::dev_t) Pci::Bdf::Routing_id::Device::get(id),
				                 (Pci::func_t)Pci::Bdf::Routing_id::Function::get(id) };
				remove_context(bdf, phys_addr);
			});
		});
	});
}


Intel::Managed_root_table::~Managed_root_table()
{
	_table_allocator.destruct<Root_table>(_root_table_phys);

	/* destruct context tables */
	_table_allocator.with_table<Root_table>(_root_table_phys,
		[&] (Root_table & root_table) {
			Root_table::for_each([&] (uint8_t bus) {
				if (root_table.present(bus)) {
					addr_t phys_addr = root_table.address(bus);
					_table_allocator.destruct<Context_table>(phys_addr);
				}
			});
		}, [&] () {});
}
