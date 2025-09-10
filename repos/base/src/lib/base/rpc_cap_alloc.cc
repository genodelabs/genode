/*
 * \brief  RPC entrypoint support for allocating RPC object capabilities
 * \author Norman Feske
 * \date   2016-01-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/env.h>
#include <util/retry.h>
#include <base/sleep.h>
#include <base/rpc_server.h>
#include <pd_session/client.h>

/* base-internal includes */
#include <base/internal/globals.h>
#include <base/internal/runtime.h>

using namespace Genode;


static Parent *_parent_ptr;
static Parent &_parent()
{
	if (_parent_ptr)
		return *_parent_ptr;

	error("missing call of init_rpc_cap_alloc");
	for (;;);
}


void Genode::init_rpc_cap_alloc(Parent &parent) { _parent_ptr = &parent; }


Rpc_entrypoint::Alloc_rpc_cap_result
Rpc_entrypoint::_alloc_rpc_cap(Runtime &runtime, Native_capability, addr_t)
{
	for (;;) {

		Ram_quota ram_upgrade { 0 };
		Cap_quota cap_upgrade { 0 };

		Native_capability result { };

		runtime.pd.alloc_rpc_cap(_cap).with_result(
			[&] (Native_capability cap) { result = cap; },
			[&] (Alloc_error e) {
				switch (e) {
				case Alloc_error::OUT_OF_RAM:  ram_upgrade = { 2*1024*sizeof(long) }; break;
				case Alloc_error::OUT_OF_CAPS: cap_upgrade = { 4 };                   break;
				case Alloc_error::DENIED:
					error("allocation of RPC cap denied");
					sleep_forever();
				}
			});

		if (result.valid())
			return result;

		_parent().upgrade(Parent::Env::pd(),
		                  String<100>("ram_quota=", ram_upgrade, ", "
		                              "cap_quota=", cap_upgrade).string());
	}
}


void Rpc_entrypoint::_free_rpc_cap(Runtime &runtime, Native_capability cap)
{
	return runtime.pd.free_rpc_cap(cap);
}
