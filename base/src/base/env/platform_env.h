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

#ifndef _PLATFORM_ENV_H_
#define _PLATFORM_ENV_H_

/* Genode includes */
#include <base/printf.h>
#include <base/env.h>
#include <base/heap.h>

/* local includes */
#include <platform_env_common.h>


namespace Genode {
	struct Expanding_rm_session_client;
	struct Expanding_cpu_session_client;
	class Platform_env;
}


struct Genode::Expanding_rm_session_client : Upgradeable_client<Genode::Rm_session_client>
{
	Expanding_rm_session_client(Rm_session_capability cap)
	: Upgradeable_client<Genode::Rm_session_client>(cap) { }

	Local_addr attach(Dataspace_capability ds, size_t size, off_t offset,
	                  bool use_local_addr, Local_addr local_addr,
	                  bool executable)
	{
		return retry<Rm_session::Out_of_metadata>(
			[&] () {
				return Rm_session_client::attach(ds, size, offset,
				                                 use_local_addr,
				                                 local_addr,
				                                 executable); },
			[&] () { upgrade_ram(8*1024); });
	}

	Pager_capability add_client(Thread_capability thread)
	{
		return retry<Rm_session::Out_of_metadata>(
			[&] () { return Rm_session_client::add_client(thread); },
			[&] () { upgrade_ram(8*1024); });
	}
};


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

	Thread_capability create_thread(Name const &name, addr_t utcb)
	{
		return retry<Cpu_session::Out_of_metadata>(
			[&] () { return Cpu_session_client::create_thread(name, utcb); },
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
			Capability<T> request(Parent &parent, char const *service) {
				return static_cap_cast<T>(parent.session(service, "")); }

			Ram_session_capability       ram_cap;
			Expanding_ram_session_client ram = { ram_cap };
			Cpu_session_capability       cpu_cap;
			Expanding_cpu_session_client cpu = { cpu_cap };
			Expanding_rm_session_client  rm;
			Pd_session_client            pd;

			Resources(Parent &parent) :
				ram_cap(request<Ram_session>(parent, "Env::ram_session")),
				cpu_cap(request<Cpu_session>(parent, "Env::cpu_session")),
				rm     (request<Rm_session> (parent, "Env::rm_session")),
				pd     (request<Pd_session> (parent, "Env::pd_session"))
			{ }
		};

		Resources _resources;
		Heap      _heap;

		char _initial_heap_chunk[sizeof(addr_t) * 4096];

		/*
		 * Emergency RAM reserve
		 *
		 * See the comment of '_fallback_sig_cap()' in 'env/env.cc'.
		 */
		constexpr static size_t  _emergency_ram_size() { return 4*1024; }
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
		{ }

		void reload_parent_cap(Native_capability::Dst, long);


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

		Parent                 *parent()          { return &_parent_client; }
		Ram_session            *ram_session()     { return &_resources.ram; }
		Ram_session_capability  ram_session_cap() { return  _resources.ram_cap; }
		Cpu_session            *cpu_session()     { return &_resources.cpu; }
		Cpu_session_capability  cpu_session_cap() { return  _resources.cpu_cap; }
		Rm_session             *rm_session()      { return &_resources.rm; }
		Pd_session             *pd_session()      { return &_resources.pd; }
		Allocator              *heap()            { return &_heap; }
};

#endif /* _PLATFORM_ENV_H_ */
