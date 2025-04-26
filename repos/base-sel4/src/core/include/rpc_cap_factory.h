/*
 * \brief  RPC capability factory
 * \author Norman Feske
 * \date   2016-01-19
 */

/*
 * Copyright (C) 2016-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__RPC_CAP_FACTORY_H_
#define _CORE__INCLUDE__RPC_CAP_FACTORY_H_

/* Genode includes */
#include <base/allocator.h>
#include <base/capability.h>
#include <base/object_pool.h>
#include <base/tslab.h>

/* base-internal includes */
#include <base/internal/page_size.h>

/* core includes */
#include <types.h>

namespace Core { class Rpc_cap_factory; }


class Core::Rpc_cap_factory
{
	private:

		struct Entry : Object_pool<Entry>::Entry
		{
			Entry(Native_capability cap) : Object_pool<Entry>::Entry(cap) {}
		};

		Object_pool<Entry> _pool { };

		enum { SBS = 960*sizeof(long) };
		uint8_t _initial_sb[SBS];

		Tslab<Entry, SBS> _entry_slab;

		Mutex             _mutex { };

	public:

		Rpc_cap_factory(Allocator &md_alloc)
		: _entry_slab(md_alloc, _initial_sb) { }

		~Rpc_cap_factory();

		Native_capability alloc(Native_capability ep);

		void free(Native_capability cap);
};

#endif /* _CORE__INCLUDE__RPC_CAP_FACTORY_H_ */
