/*
 * \brief  RPC capability factory
 * \author Norman Feske
 * \date   2016-01-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__RPC_CAP_FACTORY_H_
#define _CORE__INCLUDE__RPC_CAP_FACTORY_H_

/* Genode includes */
#include <base/rpc_server.h>
#include <base/heap.h>
#include <base/tslab.h>
#include <base/object_pool.h>

/* base-internal includes */
#include <base/internal/page_size.h>


namespace Genode { class Rpc_cap_factory; }

class Genode::Rpc_cap_factory
{
	private:

		struct Entry : Object_pool<Entry>::Entry
		{
			Entry(Native_capability cap) : Object_pool<Entry>::Entry(cap) {}
		};

		Object_pool<Entry> _pool { };

		/*
		 * Dimension '_entry_slab' such that slab blocks (including the
		 * meta-data overhead of the sliced-heap blocks) are page sized.
		 */
		static constexpr size_t SLAB_BLOCK_SIZE =
			get_page_size() - Sliced_heap::meta_data_size();

		uint8_t _initial_sb[SLAB_BLOCK_SIZE];

		Tslab<Entry, SLAB_BLOCK_SIZE> _entry_slab;

	public:

		Rpc_cap_factory(Allocator &md_alloc)
		: _entry_slab(md_alloc, _initial_sb) { }

		~Rpc_cap_factory();

		Native_capability alloc(Native_capability ep);

		void free(Native_capability cap);
};

#endif /* _CORE__INCLUDE__RPC_CAP_FACTORY_H_ */
