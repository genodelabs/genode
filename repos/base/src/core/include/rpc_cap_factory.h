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

#include <base/allocator.h>
#include <base/lock.h>
#include <base/capability.h>

namespace Genode { class Rpc_cap_factory; }

class Genode::Rpc_cap_factory
{
	private:

		static long _unique_id_cnt;
		static Lock &_lock();

	public:

		Rpc_cap_factory(Allocator &) { }

		Native_capability alloc(Native_capability ep)
		{
			Lock::Guard lock_guard(_lock());

			return Native_capability(ep.dst(), ++_unique_id_cnt);
		}

		void free(Native_capability) { }
};

#endif /* _CORE__INCLUDE__RPC_CAP_FACTORY_H_ */
