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
#include <ram_session_component.h>
#include <synced_ram_session.h>
#include <assertion.h>

namespace Genode {
	class Core_env;
	extern Core_env *core_env();
}


class Genode::Core_env : public Env_deprecated
{
	private:

		enum { ENTRYPOINT_STACK_SIZE = 2048 * sizeof(Genode::addr_t) };

		/*
		 * Initialize the stack area before creating the first thread,
		 * which happens to be the '_entrypoint'.
		 */
		bool _init_stack_area() { init_stack_area(); return true; }
		bool _stack_area_initialized = _init_stack_area();

		Rpc_entrypoint        _entrypoint;
		Core_region_map       _region_map;
		Ram_session_component _ram_session;
		Synced_ram_session    _synced_ram_session { _ram_session };

	public:

		Core_env()
		:
			_entrypoint(nullptr, ENTRYPOINT_STACK_SIZE, "entrypoint"),
			_region_map(_entrypoint),
			_ram_session(_entrypoint,
			             Session::Resources {
			                 Ram_quota { platform()->ram_alloc()->avail() },
			                 Cap_quota { platform()->max_caps() } },
			             Session::Label("core"),
			             Session::Diag{false},
			             *platform()->ram_alloc(),
			             _region_map,
			             Ram_session_component::any_phys_range())
		{
			_ram_session.init_ram_account();
		}

		~Core_env() { parent()->exit(0); }

		Rpc_entrypoint *entrypoint() { return &_entrypoint; }


		/******************************
		 ** Env_deprecated interface **
		 ******************************/

		Parent                 *parent()          override { return nullptr; }
		Ram_session            *ram_session()     override { return &_synced_ram_session; }
		Ram_session_capability  ram_session_cap() override { return  _ram_session.cap(); }
		Region_map             *rm_session()      override { return &_region_map; }
		Pd_session             *pd_session()      override { return nullptr; }
		Allocator              *heap()            override { ASSERT_NEVER_CALLED; }
		Cpu_session            *cpu_session()     override { ASSERT_NEVER_CALLED; }
		Cpu_session_capability  cpu_session_cap() override { ASSERT_NEVER_CALLED; }
		Pd_session_capability   pd_session_cap()  override { ASSERT_NEVER_CALLED; }

		void reinit(Capability<Parent>::Raw) override { }
		void reinit_main_thread(Capability<Region_map> &) override { }
};


#endif /* _CORE__INCLUDE__CORE_ENV_H_ */
