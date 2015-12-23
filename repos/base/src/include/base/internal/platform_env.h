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
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__INTERNAL__PLATFORM_ENV_H_
#define _INCLUDE__BASE__INTERNAL__PLATFORM_ENV_H_

/* Genode includes */
#include <base/printf.h>
#include <base/env.h>
#include <base/heap.h>

/* base-internal includes */
#include <base/internal/platform_env_common.h>


namespace Genode {
	struct Expanding_cpu_session_client;
	class Platform_env;
}


struct Genode::Expanding_cpu_session_client : Upgradeable_client<Genode::Cpu_session_client>
{
	Expanding_cpu_session_client(Genode::Cpu_session_capability cap)
	:
		/*
		 * We need to upcast the capability because on some platforms (i.e.,
		 * NOVA), 'Cpu_session_client' refers to a platform-specific session
		 * interface ('Nova_cpu_session').
		 */
		Upgradeable_client<Genode::Cpu_session_client>
			(static_cap_cast<Genode::Cpu_session_client::Rpc_interface>(cap))
	{ }

	Thread_capability
	create_thread(size_t quota, Name const &name, addr_t utcb)
	{
		return retry<Cpu_session::Out_of_metadata>(
			[&] () {
				return Cpu_session_client::create_thread(quota, name, utcb); },
			[&] () { upgrade_ram(8*1024); });
	}
};


class Genode::Platform_env : public Genode::Env, public Emergency_ram_reserve
{
	private:

		Expanding_parent_client _parent_client;

		struct Resources
		{
			template <typename T>
			Capability<T> request(Parent &parent, char const *service)
			{
				return static_cap_cast<T>(parent.session(service, ""));
			}

			Expanding_ram_session_client ram;
			Expanding_cpu_session_client cpu;
			Expanding_rm_session_client  rm;
			Pd_session_client            pd;

			Resources(Parent &parent)
			:
				ram(request<Ram_session>(parent, "Env::ram_session")),
				cpu(request<Cpu_session>(parent, "Env::cpu_session")),
				rm (request<Rm_session> (parent, "Env::rm_session")),
				pd (request<Pd_session> (parent, "Env::pd_session"))
			{ }
		};

		Resources _resources;

		Heap _heap;

		/*
		 * The '_heap' must be initialized before the '_stack_area'
		 * because the 'Local_parent' performs a dynamic memory allocation
		 * due to the creation of the stack area's sub-RM session.
		 */
		Attached_stack_area _stack_area { _parent_client, _resources.rm };

		char _initial_heap_chunk[sizeof(addr_t) * 4096];

		/*
		 * Emergency RAM reserve
		 *
		 * See the comment of '_fallback_sig_cap()' in 'env/env.cc'.
		 */
		constexpr static size_t  _emergency_ram_size() { return 8*1024; }
		Ram_dataspace_capability _emergency_ram_ds;

	public:

		/**
		 * Standard constructor
		 */
		Platform_env()
		:
			_parent_client(Genode::parent_cap(), *this),
			_resources(_parent_client),
			_heap(&_resources.ram, &_resources.rm, Heap::UNLIMITED,
			      _initial_heap_chunk, sizeof(_initial_heap_chunk)),
			_emergency_ram_ds(_resources.ram.alloc(_emergency_ram_size()))
		{
			env_stack_area_ram_session = &_resources.ram;
			env_stack_area_rm_session  = &_stack_area;
		}

		/*
		 * Support functions for implementing fork on Noux.
		 */
		void reinit(Native_capability::Dst, long) override;
		void reinit_main_thread(Rm_session_capability &) override;


		/*************************************
		 ** Emergency_ram_reserve interface **
		 *************************************/

		void release() {

			PDBG("used before freeing emergency=%zu", _resources.ram.used());
			_resources.ram.free(_emergency_ram_ds);
			PDBG("used after freeing emergency=%zu", _resources.ram.used());
		}


		/*******************
		 ** Env interface **
		 *******************/

		Parent                 *parent()          override { return &_parent_client; }
		Ram_session            *ram_session()     override { return &_resources.ram; }
		Ram_session_capability  ram_session_cap() override { return  _resources.ram; }
		Cpu_session            *cpu_session()     override { return &_resources.cpu; }
		Cpu_session_capability  cpu_session_cap() override { return  _resources.cpu; }
		Rm_session             *rm_session()      override { return &_resources.rm; }
		Pd_session             *pd_session()      override { return &_resources.pd; }
		Pd_session_capability   pd_session_cap()  override { return  _resources.pd; }
		Allocator              *heap()            override { return &_heap; }
};

#endif /* _INCLUDE__BASE__INTERNAL__PLATFORM_ENV_H_ */
