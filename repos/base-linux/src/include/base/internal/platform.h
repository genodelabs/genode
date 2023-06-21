/*
 * \brief  Linux-specific environment
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2006-07-28
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__PLATFORM_H_
#define _INCLUDE__BASE__INTERNAL__PLATFORM_H_

/* Genode includes */
#include <base/heap.h>

/* base-internal includes */
#include <base/internal/globals.h>
#include <base/internal/expanding_cpu_session_client.h>
#include <base/internal/expanding_region_map_client.h>
#include <base/internal/expanding_pd_session_client.h>
#include <base/internal/expanding_parent_client.h>
#include <base/internal/region_map_mmap.h>
#include <base/internal/local_rm_session.h>
#include <base/internal/local_pd_session.h>
#include <base/internal/local_parent.h>

namespace Genode { struct Platform; }


struct Genode::Platform
{
	Region_map_mmap rm { false };

	static Capability<Parent> _obtain_parent_cap();

	Local_parent parent { _obtain_parent_cap(), rm, heap };

	Pd_session_capability pd_cap =
		static_cap_cast<Pd_session>(parent.session_cap(Parent::Env::pd()));

	Cpu_session_capability cpu_cap =
		static_cap_cast<Cpu_session>(parent.session_cap(Parent::Env::cpu()));

	Local_pd_session pd { parent, pd_cap };

	Expanding_cpu_session_client cpu { parent, cpu_cap, Parent::Env::cpu() };

	Heap heap { pd, rm };

	Platform() { _attach_stack_area(); }

	/**
	 * Attach stack area to local address space (for non-hybrid components)
	 */
	void _attach_stack_area();
};

#endif /* _INCLUDE__BASE__INTERNAL__PLATFORM_H_ */
