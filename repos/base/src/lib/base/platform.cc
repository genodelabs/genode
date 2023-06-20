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

#include <deprecated/env.h>
#include <base/internal/platform.h>
#include <base/internal/globals.h>
#include <base/connection.h>
#include <base/service.h>

using namespace Genode;

static Platform *_platform_ptr;


Env_deprecated *Genode::env_deprecated()
{
	if (!_platform_ptr) {
		error("missing call of init_platform");
		for (;;);
	}

	struct Impl : Env_deprecated, Noncopyable
	{
		Platform &_pf;

		Impl(Platform &pf) : _pf(pf) { }

		Parent                *parent()          override { return &_pf.parent; }
		Cpu_session           *cpu_session()     override { return &_pf.cpu; }
		Cpu_session_capability cpu_session_cap() override { return  _pf.cpu.rpc_cap(); }
		Region_map            *rm_session()      override { return &_pf.rm; }
		Pd_session            *pd_session()      override { return &_pf.pd; }
		Pd_session_capability  pd_session_cap()  override { return  _pf.pd.rpc_cap(); }
	};

	static Impl impl { *_platform_ptr };

	return &impl;
}


void Genode::init_parent_resource_requests(Genode::Env &env)
{
	/**
	 * Catch up asynchronous resource request and notification
	 * mechanism construction of the expanding parent environment
	 */
	using Parent = Expanding_parent_client;
	static_cast<Parent*>(&env.parent())->init_fallback_signal_handling();
}


void Genode::init_platform()
{
	static Genode::Platform platform;

	init_log(platform.parent);
	init_rpc_cap_alloc(platform.parent);
	init_cap_slab(platform.pd, platform.parent);
	init_thread(platform.cpu, platform.rm);
	init_thread_start(platform.pd.rpc_cap());
	init_thread_bootstrap(platform.parent.main_thread_cap());

	env_stack_area_ram_allocator = &platform.pd;
	env_stack_area_region_map    = &platform.stack_area;

	_platform_ptr = &platform;
}


void Genode::binary_ready_hook_for_platform() { }
