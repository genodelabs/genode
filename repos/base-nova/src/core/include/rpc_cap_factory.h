/*
 * \brief  RPC capability factory
 * \author Norman Feske
 * \date   2016-01-19
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__RPC_CAP_FACTORY_H_
#define _CORE__INCLUDE__RPC_CAP_FACTORY_H_

/* Genode includes */
#include <util/list.h>
#include <base/lock.h>
#include <base/capability.h>
#include <base/tslab.h>
#include <base/printf.h>

namespace Genode { class Rpc_cap_factory; }


class Genode::Rpc_cap_factory
{
	private:

		struct Cap_object : List<Cap_object>::Element
		{
			Genode::addr_t _cap_sel;

			Cap_object(addr_t cap_sel) : _cap_sel(cap_sel) {}
		};

		Tslab<Cap_object, 128> _slab;
		List<Cap_object>       _list;
		Lock                   _lock;

	public:

		Rpc_cap_factory(Allocator &md_alloc);

		~Rpc_cap_factory();

		/**
		 * Allocate RPC capability
		 *
		 * \throw Allocator::Out_of_memory
		 *
		 * This function is invoked via Nova_native_pd::alloc_rpc_cap.
		 */
		Native_capability alloc(Native_capability ep, addr_t entry, addr_t mtd);

		Native_capability alloc(Native_capability)
		{
			PWRN("unexpected call to non-implemented Rpc_cap_factory::alloc");
			return Native_capability();
		}

		void free(Native_capability cap);
};

#endif /* _CORE__INCLUDE__RPC_CAP_FACTORY_H_ */
