/*
 * \brief  Runtime of Genode component
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2006-07-28
 */

/*
 * Copyright (C) 2006-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__RUNTIME_H_
#define _INCLUDE__BASE__INTERNAL__RUNTIME_H_

/* base-internal includes */
#include <base/internal/parent_cap.h>
#include <base/internal/globals.h>
#include <base/internal/attached_stack_area.h>
#include <base/internal/expanding_cpu_session_client.h>
#include <base/internal/expanding_pd_session_client.h>
#include <base/internal/expanding_region_map_client.h>
#include <base/internal/expanding_parent_client.h>

#ifdef USED_BY_CORE
#error base/interal/platform.h must not be included by core
#endif

namespace Genode { class Runtime; }


struct Genode::Runtime : Noncopyable
{
	Expanding_parent_client parent { Genode::parent_cap() };

	template <typename T> Capability<T> _request(Parent::Client::Id id)
	{
		return parent.session_cap(id).convert<Capability<T>>(
			[&] (Capability<Session> cap)   { return static_cap_cast<T>(cap); },
			[&] (Parent::Session_cap_error) { return Capability<T>(); });
	}

	Expanding_pd_session_client pd {
		parent, _request<Pd_session>(Parent::Env::pd()) };

	Expanding_cpu_session_client cpu {
		parent, _request<Cpu_session>(Parent::Env::cpu()), Parent::Env::cpu() };

	Expanding_region_map_client pd_rm {
		parent, pd.rpc_cap(), pd.address_space(), Parent::Env::pd() };

	Pd_local_rm      local_rm { pd_rm };
	Pd_ram_allocator ram      { pd };

	Attached_stack_area stack_area { parent, pd.rpc_cap() };

	Runtime()
	{
		env_stack_area_ram_allocator = &ram;
		env_stack_area_region_map    = &stack_area;
	}
};

#endif /* _INCLUDE__BASE__INTERNAL__RUNTIME_H_ */
