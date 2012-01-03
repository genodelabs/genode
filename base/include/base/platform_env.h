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
 * Copyright (C) 2006-2012 Genode Labs GmbH
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
				                  Local_addr local_addr = (addr_t)0) {

					bool try_again;
					do {
						try_again = false;
						try {
							return Rm_session_client::attach(ds, size, offset,
							                                 use_local_addr,
							                                 local_addr);

						} catch (Rm_session::Out_of_metadata) {

							/* give up if the error occurred a second time */
							if (try_again)
								break;

							PINF("upgrade quota donation for Env::RM session");
							env()->parent()->upgrade(_cap, "ram_quota=8K");
							try_again = true;
						}
					} while (try_again);

					return (addr_t)0;
				}
		};

		class Expanding_ram_session_client : public Ram_session_client
		{
			Ram_session_capability _cap;

			public:

				Expanding_ram_session_client(Ram_session_capability cap)
				: Ram_session_client(cap), _cap(cap) { }

				Ram_dataspace_capability alloc(size_t size) {
					bool try_again;
					do {
						try_again = false;
						try {
							return Ram_session_client::alloc(size);

						} catch (Ram_session::Out_of_metadata) {

							/* give up if the error occurred a second time */
							if (try_again)
								break;

							PINF("upgrade quota donation for Env::RAM session");
							env()->parent()->upgrade(_cap, "ram_quota=8K");
							try_again = true;
						}
					} while (try_again);

					return Ram_dataspace_capability();
				}
		};

		private:

			Parent_client                 _parent_client;
			Parent                       *_parent;
			Ram_session_capability        _ram_session_cap;
			Expanding_ram_session_client  _ram_session_client;
			Cpu_session_client            _cpu_session_client;
			Expanding_rm_session_client   _rm_session_client;
			Pd_session_client             _pd_session_client;
			Heap                          _heap;


		public:

			/**
			 * Standard constructor
			 */
			Platform_env()
			:
				_parent_client(Genode::parent_cap()), _parent(&_parent_client),
				_ram_session_cap(static_cap_cast<Ram_session>(parent()->session("Env::ram_session", ""))),
				_ram_session_client(_ram_session_cap),
				_cpu_session_client(static_cap_cast<Cpu_session>(parent()->session("Env::cpu_session", ""))),
				_rm_session_client(static_cap_cast<Rm_session>(parent()->session("Env::rm_session", ""))),
				_pd_session_client(static_cap_cast<Pd_session>(parent()->session("Env::pd_session", ""))),
				_heap(ram_session(), rm_session())
			{ }


			/*******************
			 ** Env interface **
			 *******************/

			Parent                 *parent()          { return  _parent; }
			Ram_session            *ram_session()     { return &_ram_session_client; }
			Ram_session_capability  ram_session_cap() { return  _ram_session_cap; }
			Cpu_session            *cpu_session()     { return &_cpu_session_client; }
			Rm_session             *rm_session()      { return &_rm_session_client; }
			Pd_session             *pd_session()      { return &_pd_session_client; }
			Allocator              *heap()            { return &_heap; }
	};
}

#endif /* _INCLUDE__BASE__PLATFORM_ENV_H_ */
