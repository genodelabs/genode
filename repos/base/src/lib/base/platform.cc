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

#include <base/internal/platform.h>

using namespace Genode;


void Genode::init_parent_resource_requests(Genode::Env &env)
{
	using Parent = Expanding_parent_client;
	static_cast<Parent*>(&env.parent())->init_fallback_signal_handling();
}


Platform &Genode::init_platform()
{
	static Genode::Platform platform;

	init_log(platform.parent);
	init_rpc_cap_alloc(platform.parent);
	init_cap_slab(platform.pd, platform.parent);
	init_thread(platform.cpu, platform.rm);
	init_thread_start(platform.pd.rpc_cap());
	init_thread_bootstrap(platform.cpu, platform.parent.main_thread_cap());
	init_signal_receiver(platform.pd, platform.parent);

	return platform;
}


void Genode::binary_ready_hook_for_platform() { }
