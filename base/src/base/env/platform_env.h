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

#ifndef _INCLUDE__BASE__PLATFORM_ENV_H_
#define _INCLUDE__BASE__PLATFORM_ENV_H_

#include <base/env.h>

#include <base/heap.h>
#include <parent/client.h>
#include <ram_session/client.h>
#include <rm_session/client.h>
#include <cpu_session/client.h>
#include <pd_session/client.h>


namespace Genode {

	class Platform_env : public Env
	{
		class Expanding_rm_session_client : public Rm_session_client
		{
			Rm_session_capability _cap;

			public:

				Expanding_rm_session_client(Rm_session_capability cap)
				: Rm_session_client(cap), _cap(cap) { }

				Local_addr attach(Dataspace_capability ds,
				                  size_t size = 0, off_t offset = 0,
				                  bool use_local_addr = false,
				                  Local_addr local_addr = (addr_t)0,
				                  bool executable = false) {

					bool try_again = false;
					do {
						try {
							return Rm_session_client::attach(ds, size, offset,
							                                 use_local_addr,
							                                 local_addr,
							                                 executable);

						} catch (Rm_session::Out_of_metadata) {

							/* give up if the error occurred a second time */
							if (try_again)
								break;

							PINF("upgrading quota donation for Env::RM session");
							env()->parent()->upgrade(_cap, "ram_quota=8K");
							try_again = true;
						}
					} while (try_again);

					return (addr_t)0;
				}

				Pager_capability add_client(Thread_capability thread)
				{
					bool try_again = false;
					do {
						try {
							return Rm_session_client::add_client(thread);
						} catch (Rm_session::Out_of_metadata) {

							/* give up if the error occurred a second time */
							if (try_again)
								break;

							PINF("upgrading quota donation for Env::RM session");
							env()->parent()->upgrade(_cap, "ram_quota=8K");
							try_again = true;
						}
					} while (try_again);

					return Pager_capability();
				}

				void remove_client(Pager_capability pager)
				{
					Rm_session_client::remove_client(pager);
				}
		};

		class Expanding_ram_session_client : public Ram_session_client
		{
			Ram_session_capability _cap;

			public:

				Expanding_ram_session_client(Ram_session_capability cap)
				: Ram_session_client(cap), _cap(cap) { }

				Ram_dataspace_capability alloc(size_t size, bool cached) {
					bool try_again = false;
					do {
						try {
							return Ram_session_client::alloc(size, cached);

						} catch (Ram_session::Out_of_metadata) {

							/* give up if the error occurred a second time */
							if (try_again)
								break;

							PINF("upgrading quota donation for Env::RAM session");
							env()->parent()->upgrade(_cap, "ram_quota=8K");
							try_again = true;
						}
					} while (try_again);

					return Ram_dataspace_capability();
				}
		};

		class Expanding_cpu_session_client : public Cpu_session_client
		{
			Cpu_session_capability _cap;

			public:

				Expanding_cpu_session_client(Cpu_session_capability cap)
				: Cpu_session_client(cap), _cap(cap) { }

				Thread_capability create_thread(Name const &name, addr_t utcb) {
					bool try_again = false;
					do {
						try {
							return Cpu_session_client::create_thread(name, utcb);
						} catch (Cpu_session::Out_of_metadata) {

							/* give up if the error occurred a second time */
							if (try_again)
								break;

							PINF("upgrading quota donation for Env::CPU session");
							env()->parent()->upgrade(_cap, "ram_quota=8K");
							try_again = true;
						}
					} while (try_again);

					return Thread_capability();
				}
		};

		private:

			Parent_client _parent_client;

			struct Resources
			{
				Ram_session_capability       ram_cap;
				Expanding_ram_session_client ram;
				Cpu_session_capability       cpu_cap;
				Expanding_cpu_session_client cpu;
				Expanding_rm_session_client  rm;
				Pd_session_client            pd;

				Resources(Parent &parent)
				:
					ram_cap(static_cap_cast<Ram_session>(parent.session("Env::ram_session", ""))),
					ram(ram_cap),
					cpu_cap(static_cap_cast<Cpu_session>(parent.session("Env::cpu_session", ""))),
					cpu(cpu_cap),
					rm(static_cap_cast<Rm_session>(parent.session("Env::rm_session", ""))),
					pd(static_cap_cast<Pd_session>(parent.session("Env::pd_session", "")))
				{ }
			};

			Resources _resources;
			Heap      _heap;

			char _initial_junk[4 * 4096];

		public:

			/**
			 * Standard constructor
			 */
			Platform_env()
			:
				_parent_client(Genode::parent_cap()),
				_resources(_parent_client),
				_heap(&_resources.ram, &_resources.rm, Heap::UNLIMITED,
				      _initial_junk, sizeof(_initial_junk))
			{ }

			void reload_parent_cap(Native_capability::Dst, long);


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
}

#endif /* _INCLUDE__BASE__PLATFORM_ENV_H_ */
