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

#ifndef _SRC__DRIVERS__PLATFORM__INTEL__MANAGED_ROOT_TABLE_H_
#define _SRC__DRIVERS__PLATFORM__INTEL__MANAGED_ROOT_TABLE_H_

/* Genode includes */
#include <base/env.h>
#include <base/attached_ram_dataspace.h>

/* local includes */
#include <intel/root_table.h>
#include <intel/context_table.h>
#include <intel/domain_allocator.h>
#include <intel/report_helper.h>
#include <cpu/page_table_allocator.h>

namespace Intel {
	using namespace Genode;

	class Managed_root_table;
}


class Intel::Managed_root_table : public Registered_translation_table
{
	public:

		using Allocator = Genode::Page_table_allocator<4096>;

	private:

		Env         & _env;

		Allocator   & _table_allocator;

		addr_t        _root_table_phys  { _table_allocator.construct<Root_table>() };

		bool          _force_flush;

		template <typename FN>
		void _with_context_table(uint8_t bus, FN && fn, bool create = false)
		{
			auto no_match_fn = [&] () { };

			_table_allocator.with_table<Root_table>(_root_table_phys,
				[&] (Root_table & root_table) {

					/* allocate table if not present */
					bool new_table { false };
					if (!root_table.present(bus)) {
						if (!create) return;

						root_table.address(bus,
						                   _table_allocator.construct<Context_table>(),
						                   _force_flush);
						new_table = true;
					}

					_table_allocator.with_table<Context_table>(root_table.address(bus),
						[&] (Context_table & ctx) {
							if (_force_flush && new_table)
								ctx.flush_all();

							fn(ctx);
						}, no_match_fn);

				}, no_match_fn);
		}

	public:

		addr_t phys_addr() { return _root_table_phys; }

		template <typename FN>
		void for_each_stage2_pointer(FN && fn)
		{
			Root_table::for_each([&] (uint8_t bus) {
				_with_context_table(bus, [&] (Context_table & ctx) {
					Pci::rid_t start_rid = Pci::Bdf::rid(Pci::Bdf { bus, 0, 0 });

					Context_table::for_each(start_rid, [&] (Pci::rid_t rid) {
						if (!ctx.present(rid))
							return;

						fn(Pci::Bdf::bdf(rid), ctx.stage2_pointer(rid), ctx.domain(rid));
					});
				});
			});
		}

		template <typename FN>
		void with_stage2_pointer(Pci::Bdf const & bdf, FN && fn)
		{
			_with_context_table(bdf.bus, [&] (Context_table & ctx) {
				Pci::rid_t rid = Pci::Bdf::rid(bdf);

				if (ctx.present(rid))
					fn(ctx.stage2_pointer(rid), ctx.domain(rid));
			});
		}

		/* add second-stage table */
		template <unsigned ADDRESS_WIDTH>
		Domain_id insert_context(Pci::Bdf const & bdf, addr_t phys_addr, Domain_id domain)
		{
			Domain_id cur_domain { };

			_with_context_table(bdf.bus, [&] (Context_table & ctx) {
				Pci::rid_t      rid = Pci::Bdf::rid(bdf);

				if (ctx.present(rid))
					cur_domain = Domain_id(ctx.domain(rid));

				ctx.insert<ADDRESS_WIDTH>(rid, phys_addr, domain.value, _force_flush);
			}, true);

			return cur_domain;
		}

		/* remove second-stage table for particular device */
		void remove_context(Pci::Bdf const &, addr_t);

		/* remove second-stage table for all devices */
		void remove_context(addr_t);

		/* Registered_translation_table interface */
		addr_t virt_addr(addr_t pa) override
		{
			addr_t va { 0 };
			_table_allocator.with_table<Context_table>(pa,
				[&] (Context_table & table) { va = (addr_t)&table; },
				[&] ()                      { va = 0; });

			return va;
		}

		Managed_root_table(Env & env,
		                   Allocator & table_allocator,
		                   Translation_table_registry & registry,
		                   bool force_flush)
		: Registered_translation_table(registry),
		  _env(env),
		  _table_allocator(table_allocator),
		  _force_flush(force_flush)
		{ }

		~Managed_root_table();
};

#endif /* _SRC__DRIVERS__PLATFORM__INTEL__MANAGED_ROOT_TABLE_H_ */
