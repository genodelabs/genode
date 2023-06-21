/*
 * \brief  Core-specific environment
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

#ifndef _CORE__INCLUDE__CORE_ENV_H_
#define _CORE__INCLUDE__CORE_ENV_H_

/* Genode includes */
#include <base/env.h>

/* base-internal includes */
#include <base/internal/globals.h>

/* core includes */
#include <platform.h>
#include <core_region_map.h>
#include <pd_session_component.h>
#include <synced_ram_allocator.h>
#include <assertion.h>

namespace Core {

	class Core_env;
	extern Core_env &core_env();
}


class Core::Core_env : public Noncopyable
{
	private:

		enum { ENTRYPOINT_STACK_SIZE = 20 * 1024 };

		/*
		 * Initialize the stack area before creating the first thread,
		 * which happens to be the '_entrypoint'.
		 */
		bool _init_stack_area() { init_stack_area(); return true; }
		bool _stack_area_initialized = _init_stack_area();

		Rpc_entrypoint       _entrypoint;
		Core_region_map      _region_map;
		Pd_session_component _pd_session;
		Synced_ram_allocator _synced_ram_allocator { _pd_session };

	public:

		Core_env()
		:
			_entrypoint(nullptr, ENTRYPOINT_STACK_SIZE, "entrypoint",
			            Affinity::Location()),
			_region_map(_entrypoint),
			_pd_session(_entrypoint,
			            _entrypoint,
			            Session::Resources {
			                Ram_quota { platform().ram_alloc().avail() },
			                Cap_quota { platform().max_caps() } },
			            Session::Label("core"),
			            Session::Diag{false},
			            platform().ram_alloc(),
			            Ram_dataspace_factory::any_phys_range(),
			            Ram_dataspace_factory::Virt_range { platform().vm_start(),
			                                                platform().vm_size() },
			            Pd_session_component::Managing_system::PERMITTED,
			            _region_map,
			            *((Pager_entrypoint *)nullptr),
			            "" /* args to native PD */,
			            platform_specific().core_mem_alloc())
		{
			_pd_session.init_cap_and_ram_accounts();
		}

		Rpc_entrypoint &entrypoint()    { return _entrypoint; }
		Ram_allocator  &ram_allocator() { return _synced_ram_allocator; }
		Region_map     &local_rm()      { return _region_map; }

		Rpc_entrypoint &signal_ep();

		Parent                 *parent()          { return nullptr; }
		Region_map             *rm_session()      { return &_region_map; }
		Pd_session             *pd_session()      { return &_pd_session; }
		Cpu_session            *cpu_session()     { ASSERT_NEVER_CALLED; }
		Cpu_session_capability  cpu_session_cap() { ASSERT_NEVER_CALLED; }
		Pd_session_capability   pd_session_cap()  { return _pd_session.cap(); }
};

#endif /* _CORE__INCLUDE__CORE_ENV_H_ */
