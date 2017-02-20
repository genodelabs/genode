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
#include <base/allocator.h>
#include <base/object_pool.h>

namespace Genode { class Rpc_cap_factory; }

class Genode::Rpc_cap_factory
{
	private:

		struct Entry : Object_pool<Entry>::Entry
		{
			Entry(Native_capability cap) : Object_pool<Entry>::Entry(cap) {}
		};

		Object_pool<Entry> _pool;
		Allocator         &_md_alloc;

	public:

		Rpc_cap_factory(Allocator &md_alloc) : _md_alloc(md_alloc) { }

		~Rpc_cap_factory();

		Native_capability alloc(Native_capability ep);

		void free(Native_capability cap);
};

#endif /* _CORE__INCLUDE__RPC_CAP_FACTORY_H_ */
