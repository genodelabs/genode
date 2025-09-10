/*
 * \brief  Environment initialization
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2006-07-27
 */

/*
 * Copyright (C) 2006-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/internal/runtime.h>

using namespace Genode;


void Genode::init_parent_resource_requests(Genode::Env &env)
{
	using Parent = Expanding_parent_client;
	static_cast<Parent*>(&env.parent())->init_fallback_signal_handling();
}


Runtime &Genode::init_runtime()
{
	static Genode::Runtime runtime;

	init_log(runtime.parent);
	init_rpc_cap_alloc(runtime.parent);
	init_cap_slab(runtime.pd, runtime.parent);
	init_signal_receiver(runtime.pd, runtime.parent);

	return runtime;
}


void Genode::binary_ready_hook_for_platform() { }
