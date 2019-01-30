/*
 * \brief  Platform environment of Genode process
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2006-07-28
 *
 * This file is a generic variant of the platform environment, which is
 * suitable for platforms such as L4ka::Pistachio and L4/Fiasco. On other
 * platforms, it may be replaced by a platform-specific version residing
 * in the corresponding 'base-<platform>' repository.
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
#include <base/log.h>
#include <base/env.h>
#include <base/heap.h>
#include <deprecated/env.h>

/* base-internal includes */
#include <base/internal/globals.h>
#include <base/internal/parent_cap.h>
#include <base/internal/attached_stack_area.h>
#include <base/internal/expanding_cpu_session_client.h>
#include <base/internal/expanding_pd_session_client.h>
#include <base/internal/expanding_region_map_client.h>
#include <base/internal/expanding_parent_client.h>


namespace Genode {

	class Platform_env_base : public Env_deprecated { };
	class Platform_env;
}


class Genode::Platform_env : public Platform_env_base
{
	private:

		Expanding_parent_client _parent_client;

		struct Resources
		{
			template <typename T>
			Capability<T> request(Parent &parent, Parent::Client::Id id)
			{
				return static_cap_cast<T>(parent.session_cap(id));
			}

			Expanding_pd_session_client  pd;
			Expanding_cpu_session_client cpu;
			Expanding_region_map_client  rm;

			Resources(Parent &parent)
			:
				pd (parent, request<Pd_session> (parent, Parent::Env::pd())),
				cpu(parent, request<Cpu_session>(parent, Parent::Env::cpu()),
				                                 Parent::Env::cpu()),
				rm(parent, pd.rpc_cap(), pd.address_space(), Parent::Env::pd())
			{ }
		};

		Resources _resources;

		Heap _heap;

		/*
		 * The '_heap' must be initialized before the '_stack_area'
		 * because the 'Local_parent' performs a dynamic memory allocation
		 * due to the creation of the stack area's sub-RM session.
		 */
		Attached_stack_area _stack_area { _parent_client, _resources.pd.rpc_cap() };

	public:

		/**
		 * Standard constructor
		 */
		Platform_env()
		:
			_parent_client(Genode::parent_cap()),
			_resources(_parent_client),
			_heap(&_resources.pd, &_resources.rm, Heap::UNLIMITED)
		{
			env_stack_area_ram_allocator = &_resources.pd;
			env_stack_area_region_map    = &_stack_area;
		}

		/*
		 * Support functions for implementing fork on Noux.
		 */
		void reinit(Native_capability::Raw) override;
		void reinit_main_thread(Capability<Region_map> &) override;


		/******************************
		 ** Env_deprecated interface **
		 ******************************/

		Parent                 *parent()          override { return &_parent_client; }
		Cpu_session            *cpu_session()     override { return &_resources.cpu; }
		Cpu_session_capability  cpu_session_cap() override { return  _resources.cpu.rpc_cap(); }
		Pd_session             *pd_session()      override { return &_resources.pd; }
		Pd_session_capability   pd_session_cap()  override { return  _resources.pd.rpc_cap(); }
		Region_map             *rm_session()      override { return &_resources.rm; }
};

#endif /* _INCLUDE__BASE__INTERNAL__PLATFORM_ENV_H_ */
