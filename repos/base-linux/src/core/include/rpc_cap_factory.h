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
#include <base/allocator.h>
#include <base/capability.h>

/* core includes */
#include <types.h>

namespace Core { struct Rpc_cap_factory; }


struct Core::Rpc_cap_factory : Noncopyable
{
	using Alloc_result = Attempt<Native_capability, Alloc_error>;

	Rpc_cap_factory(Allocator &) { }

	Alloc_result alloc(Native_capability) { return Native_capability(); }

	void free(Native_capability) { }
};

#endif /* _CORE__INCLUDE__RPC_CAP_FACTORY_H_ */
