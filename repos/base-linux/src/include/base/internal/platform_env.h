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

#ifndef _INCLUDE__BASE__INTERNAL__PLATFORM_ENV_H_
#define _INCLUDE__BASE__INTERNAL__PLATFORM_ENV_H_

/* Genode includes */
#include <base/heap.h>

/* base-internal includes */
#include <base/internal/expanding_cpu_session_client.h>
#include <base/internal/expanding_region_map_client.h>
#include <base/internal/expanding_pd_session_client.h>
#include <base/internal/expanding_parent_client.h>
#include <base/internal/region_map_mmap.h>
#include <base/internal/local_rm_session.h>
#include <base/internal/local_pd_session.h>
#include <base/internal/local_parent.h>

namespace Genode {
	class Platform_env_base;
	class Platform_env;
}


/**
 * Common base class of the 'Platform_env' implementations for core and
 * non-core processes.
 */
class Genode::Platform_env_base : public Env_deprecated
{
	private:

		Cpu_session_capability       _cpu_session_cap;
		Expanding_cpu_session_client _cpu_session_client;
		Region_map_mmap              _region_map_mmap;
		Pd_session_capability        _pd_session_cap;

	protected:

		/*
		 * The '_local_pd_session' is protected because it is needed by
		 * 'Platform_env' to initialize the stack area. This must not happen
		 * in 'Platform_env_base' because the procedure differs between
		 * core and non-core components.
		 */
		Local_pd_session _local_pd_session;

	public:

		/**
		 * Constructor
		 */
		Platform_env_base(Parent &parent,
		                  Cpu_session_capability cpu_cap,
		                  Pd_session_capability  pd_cap)
		:
			_cpu_session_cap(cpu_cap),
			_cpu_session_client(parent, cpu_cap, Parent::Env::cpu()),
			_region_map_mmap(false),
			_pd_session_cap(pd_cap),
			_local_pd_session(parent, _pd_session_cap)
		{ }

		/**
		 * Constructor used by 'Core_env'
		 */
		Platform_env_base(Parent &parent)
		:
			Platform_env_base(parent, Cpu_session_capability(), Pd_session_capability())
		{ }


		/******************************
		 ** Env_deprecated interface **
		 ******************************/

		Region_map             *rm_session()      override { return &_region_map_mmap; }
		Cpu_session            *cpu_session()     override { return &_cpu_session_client; }
		Cpu_session_capability  cpu_session_cap() override { return  _cpu_session_cap; }
		Pd_session             *pd_session()      override { return &_local_pd_session; }
		Pd_session_capability   pd_session_cap()  override { return  _pd_session_cap; }

		void reinit(Native_capability::Raw) override;
		void reinit_main_thread(Capability<Region_map> &) override;
};


/**
 * 'Platform_env' used by all processes except for core
 */
class Genode::Platform_env : public Platform_env_base
{
	private:

		/**
		 * Return instance of parent interface
		 */
		Local_parent &_parent();

		Heap _heap;

		/**
		 * Attach stack area to local address space (for non-hybrid components)
		 */
		void _attach_stack_area();

	public:

		/**
		 * Constructor
		 */
		Platform_env();

		/**
		 * Destructor
		 */
		~Platform_env() { _parent().exit(0); }


		/******************************
		 ** Env_deprecated interface **
		 ******************************/

		Parent *parent() override { return &_parent(); }
};

#endif /* _INCLUDE__BASE__INTERNAL__PLATFORM_ENV_H_ */
