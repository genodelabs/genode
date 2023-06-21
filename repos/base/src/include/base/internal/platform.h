/*
 * \brief  Platform of Genode component
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

#ifndef _INCLUDE__BASE__INTERNAL__PLATFORM_H_
#define _INCLUDE__BASE__INTERNAL__PLATFORM_H_

/* base-internal includes */
#include <base/internal/parent_cap.h>
#include <base/internal/globals.h>
#include <base/internal/attached_stack_area.h>
#include <base/internal/expanding_cpu_session_client.h>
#include <base/internal/expanding_pd_session_client.h>
#include <base/internal/expanding_region_map_client.h>
#include <base/internal/expanding_parent_client.h>

namespace Genode { class Platform; }


struct Genode::Platform : Noncopyable
{
	Expanding_parent_client parent { Genode::parent_cap() };

	template <typename T> Capability<T> _request(Parent::Client::Id id)
	{
		return static_cap_cast<T>(parent.session_cap(id));
	}

	Expanding_pd_session_client pd {
		parent, _request<Pd_session>(Parent::Env::pd()) };

	Expanding_cpu_session_client cpu {
		parent, _request<Cpu_session>(Parent::Env::cpu()), Parent::Env::cpu() };

	Expanding_region_map_client rm {
		parent, pd.rpc_cap(), pd.address_space(), Parent::Env::pd() };

	Attached_stack_area stack_area { parent, pd.rpc_cap() };

	Platform()
	{
		env_stack_area_ram_allocator = &pd;
		env_stack_area_region_map    = &stack_area;
	}
};

#endif /* _INCLUDE__BASE__INTERNAL__PLATFORM_H_ */
