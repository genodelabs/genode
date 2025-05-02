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
#include <util/list.h>
#include <base/mutex.h>
#include <base/capability.h>
#include <base/tslab.h>
#include <base/rpc_server.h>

/* core includes */
#include <types.h>

namespace Core { class Rpc_cap_factory; }


class Core::Rpc_cap_factory
{
	private:

		struct Cap_object : List<Cap_object>::Element
		{
			addr_t _cap_sel;

			Cap_object(addr_t cap_sel) : _cap_sel(cap_sel) {}
		};

		enum { SBS = 960*sizeof(long) };
		uint8_t _initial_sb[SBS];

		Tslab<Cap_object, SBS> _slab;
		List<Cap_object>       _list { };
		Mutex                  _mutex { };

	public:

		Rpc_cap_factory(Allocator &md_alloc);

		~Rpc_cap_factory();

		using Alloc_result = Attempt<Native_capability, Alloc_error>;

		Alloc_result alloc(Native_capability ep, addr_t entry, addr_t mtd);

		/* unused on NOVA */
		Alloc_result alloc(Native_capability) { return Alloc_error::DENIED; }

		void free(Native_capability cap);
};

#endif /* _CORE__INCLUDE__RPC_CAP_FACTORY_H_ */
