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

#ifndef _SRC__DRIVERS__PLATFORM__INTEL__DEFAULT_MAPPINGS_H_
#define _SRC__DRIVERS__PLATFORM__INTEL__DEFAULT_MAPPINGS_H_

/* local includes */
#include <intel/managed_root_table.h>
#include <intel/report_helper.h>
#include <intel/page_table.h>

namespace Intel {
	using namespace Genode;

	class Default_mappings;
}


class Intel::Default_mappings
{
	public:

		using Allocator = Managed_root_table::Allocator;

		enum Translation_levels { LEVEL3, LEVEL4 };

	private:

		Allocator          & _table_allocator;

		Managed_root_table   _root_table;

		bool                 _force_flush;

		Translation_levels   _levels;

		addr_t               _default_table_phys;

		static addr_t _construct_default_table(Allocator          & alloc,
		                                       Translation_levels   levels)
		{
			switch (levels) {
				case Translation_levels::LEVEL3:
					return alloc.construct<Level_3_translation_table>();
				case Translation_levels::LEVEL4:
					return alloc.construct<Level_4_translation_table>();
			}

			return 0;
		}

		void _insert_context(Managed_root_table &, Pci::Bdf const &, addr_t, Domain_id);

	public:

		void insert_translation(addr_t, addr_t, size_t, Page_flags, uint32_t);

		void enable_device(Pci::Bdf const &, Domain_id);

		void copy_stage2(Managed_root_table &, Pci::Bdf const &);
		void copy_stage2(Managed_root_table &);

		Default_mappings(Env                        & env,
		                 Allocator                  & table_allocator,
		                 Translation_table_registry & registry,
		                 bool                         force_flush,
		                 Translation_levels           levels)
		: _table_allocator(table_allocator),
		  _root_table(env, table_allocator, registry, force_flush),
		  _force_flush(force_flush),
		  _levels(levels),
		  _default_table_phys(_construct_default_table(_table_allocator, _levels))
		{ }

		~Default_mappings();

};

#endif /* _SRC__DRIVERS__PLATFORM__INTEL__DEFAULT_MAPPINGS_H_ */
