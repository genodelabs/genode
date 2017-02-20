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

/* base-internal includes */
#include <base/internal/globals.h>
#include <base/internal/parent_cap.h>
#include <base/internal/attached_stack_area.h>
#include <base/internal/expanding_cpu_session_client.h>
#include <base/internal/expanding_region_map_client.h>
#include <base/internal/expanding_ram_session_client.h>
#include <base/internal/expanding_parent_client.h>


namespace Genode { class Platform_env; }


class Genode::Platform_env : public Env_deprecated,
                             public Expanding_parent_client::Emergency_ram_reserve
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

			Expanding_ram_session_client ram;
			Expanding_cpu_session_client cpu;
			Pd_session_client            pd;
			Expanding_region_map_client  rm;

			Resources(Parent &parent)
			:
				ram(request<Ram_session>(parent, Parent::Env::ram()),
				                         Parent::Env::ram()),
				cpu(request<Cpu_session>(parent, Parent::Env::cpu()),
				                         Parent::Env::cpu()),
				pd (request<Pd_session> (parent, Parent::Env::pd())),
				rm (pd, pd.address_space(), Parent::Env::pd())
			{ }
		};

		Resources _resources;

		Heap _heap;

		/*
		 * The '_heap' must be initialized before the '_stack_area'
		 * because the 'Local_parent' performs a dynamic memory allocation
		 * due to the creation of the stack area's sub-RM session.
		 */
		Attached_stack_area _stack_area { _parent_client, _resources.pd };

		/*
		 * Emergency RAM reserve
		 *
		 * See the comment of '_fallback_sig_cap()' in 'env/env.cc'.
		 */
		constexpr static size_t  _emergency_ram_size() { return 16*1024; }
		Ram_dataspace_capability _emergency_ram_ds;

	public:

		/**
		 * Standard constructor
		 */
		Platform_env()
		:
			_parent_client(Genode::parent_cap(), *this),
			_resources(_parent_client),
			_heap(&_resources.ram, &_resources.rm, Heap::UNLIMITED),
			_emergency_ram_ds(_resources.ram.alloc(_emergency_ram_size()))
		{
			env_stack_area_ram_session = &_resources.ram;
			env_stack_area_region_map  = &_stack_area;
		}

		/*
		 * Support functions for implementing fork on Noux.
		 */
		void reinit(Native_capability::Raw) override;
		void reinit_main_thread(Capability<Region_map> &) override;


		/*************************************
		 ** Emergency_ram_reserve interface **
		 *************************************/

		void release() {

			log("used before freeing emergency=", _resources.ram.used());
			_resources.ram.free(_emergency_ram_ds);
			log("used after freeing emergency=", _resources.ram.used());
		}


		/******************************
		 ** Env_deprecated interface **
		 ******************************/

		Parent                 *parent()          override { return &_parent_client; }
		Ram_session            *ram_session()     override { return &_resources.ram; }
		Ram_session_capability  ram_session_cap() override { return  _resources.ram; }
		Cpu_session            *cpu_session()     override { return &_resources.cpu; }
		Cpu_session_capability  cpu_session_cap() override { return  _resources.cpu; }
		Region_map             *rm_session()      override { return &_resources.rm; }
		Pd_session             *pd_session()      override { return &_resources.pd; }
		Pd_session_capability   pd_session_cap()  override { return  _resources.pd; }
		Allocator              *heap()            override { return &_heap; }
};

#endif /* _INCLUDE__BASE__INTERNAL__PLATFORM_ENV_H_ */
