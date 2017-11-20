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

namespace Genode {

	class Core_env;
	extern Core_env *core_env();
}


class Genode::Core_env : public Env_deprecated
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
			_entrypoint(nullptr, ENTRYPOINT_STACK_SIZE, "entrypoint"),
			_region_map(_entrypoint),
			_pd_session(_entrypoint,
			            _entrypoint,
			            Session::Resources {
			                Ram_quota { platform()->ram_alloc()->avail() },
			                Cap_quota { platform()->max_caps() } },
			            Session::Label("core"),
			            Session::Diag{false},
			            *platform()->ram_alloc(),
			            Ram_dataspace_factory::any_phys_range(),
			            Ram_dataspace_factory::Virt_range { platform()->vm_start(), platform()->vm_size() },
			            _region_map,
			            *((Pager_entrypoint *)nullptr),
			            "" /* args to native PD */,
			            *platform_specific()->core_mem_alloc())
		{
			_pd_session.init_cap_and_ram_accounts();
		}

		~Core_env() { parent()->exit(0); }

		Rpc_entrypoint *entrypoint()    { return &_entrypoint; }
		Ram_allocator  &ram_allocator() { return  _synced_ram_allocator; }
		Region_map     &local_rm()      { return  _region_map; }

		Rpc_entrypoint &signal_ep();

		/******************************
		 ** Env_deprecated interface **
		 ******************************/

		Parent                 *parent()          override { return nullptr; }
		Ram_session            *ram_session()     override { return &_pd_session; }
		Ram_session_capability  ram_session_cap() override { return  _pd_session.cap(); }
		Region_map             *rm_session()      override { return &_region_map; }
		Pd_session             *pd_session()      override { return &_pd_session; }
		Allocator              *heap()            override { ASSERT_NEVER_CALLED; }
		Cpu_session            *cpu_session()     override { ASSERT_NEVER_CALLED; }
		Cpu_session_capability  cpu_session_cap() override { ASSERT_NEVER_CALLED; }
		Pd_session_capability   pd_session_cap()  override { return _pd_session.cap(); }

		void reinit(Capability<Parent>::Raw) override { }
		void reinit_main_thread(Capability<Region_map> &) override { }
};

#endif /* _CORE__INCLUDE__CORE_ENV_H_ */
