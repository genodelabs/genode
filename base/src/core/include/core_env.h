/*
 * \brief  Core-specific environment
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2006-07-28
 *
 * The Core-specific environment ensures that all sessions of Core's
 * environment a local (_component) not remote (_client).
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__CORE_ENV_H_
#define _CORE__INCLUDE__CORE_ENV_H_

/* Genode includes */
#include <base/env.h>
#include <base/heap.h>
#include <ram_session/client.h>

/* core includes */
#include <core_parent.h>
#include <core_rm_session.h>
#include <cap_session_component.h>
#include <ram_session_component.h>

namespace Genode {

	/**
	 * Lock-guarded version of a RAM-session implementation
	 *
	 * \param RAM_SESSION_IMPL  non-thread-safe RAM-session class
	 *
	 * In contrast to normal processes, core's 'env()->ram_session()' is not
	 * synchronized by an RPC interface. However, it is accessed by different
	 * threads using the 'env()->heap()' and the sliced heap used for
	 * allocating sessions to core's services.
	 */
	template <typename RAM_SESSION_IMPL>
	class Synchronized_ram_session : public RAM_SESSION_IMPL
	{
		private:

			Lock _lock;

		public:

			/**
			 * Constructor
			 */
			Synchronized_ram_session(Rpc_entrypoint  *ds_ep,
			                         Rpc_entrypoint  *ram_session_ep,
			                         Range_allocator *ram_alloc,
			                         Allocator       *md_alloc,
			                         const char      *args,
			                         size_t           quota_limit = 0)
			:
				RAM_SESSION_IMPL(ds_ep, ram_session_ep, ram_alloc, md_alloc, args, quota_limit)
			{ }


			/***************************
			 ** RAM-session interface **
			 ***************************/

			Ram_dataspace_capability alloc(size_t size, bool cached)
			{
				Lock::Guard lock_guard(_lock);
				return RAM_SESSION_IMPL::alloc(size, cached);
			}

			void free(Ram_dataspace_capability ds)
			{
				Lock::Guard lock_guard(_lock);
				RAM_SESSION_IMPL::free(ds);
			}

			int ref_account(Ram_session_capability session)
			{
				Lock::Guard lock_guard(_lock);
				return RAM_SESSION_IMPL::ref_account(session);
			}

			int transfer_quota(Ram_session_capability session, size_t size)
			{
				Lock::Guard lock_guard(_lock);
				return RAM_SESSION_IMPL::transfer_quota(session, size);
			}

			size_t quota()
			{
				Lock::Guard lock_guard(_lock);
				return RAM_SESSION_IMPL::quota();
			}

			size_t used()
			{
				Lock::Guard lock_guard(_lock);
				return RAM_SESSION_IMPL::used();
			}
	};


	class Core_env : public Env
	{
		private:

			typedef Synchronized_ram_session<Ram_session_component> Core_ram_session;

			enum { ENTRYPOINT_STACK_SIZE = 8*1024 };

			Core_parent                  _core_parent;
			Cap_session_component        _cap_session;
			Rpc_entrypoint               _entrypoint;
			Core_rm_session              _rm_session;
			Core_ram_session             _ram_session;
			Heap                         _heap;
			Ram_session_capability const _ram_session_cap;

		public:

			/**
			 * Constructor
			 */
			Core_env() :
				_entrypoint(&_cap_session, ENTRYPOINT_STACK_SIZE, "entrypoint"),
				_rm_session(&_entrypoint),
				_ram_session(&_entrypoint, &_entrypoint,
				             platform()->ram_alloc(), platform()->core_mem_alloc(),
				             "ram_quota=4M", platform()->ram_alloc()->avail()),
				_heap(&_ram_session, &_rm_session),
				_ram_session_cap(_entrypoint.manage(&_ram_session))
			{ }

			/**
			 * Destructor
			 */
			~Core_env() { parent()->exit(0); }

			Cap_session    *cap_session() { return &_cap_session; }
			Rpc_entrypoint *entrypoint()  { return &_entrypoint; }


			/*******************
			 ** Env interface **
			 *******************/

			Parent                 *parent()          { return &_core_parent; }
			Ram_session            *ram_session()     { return &_ram_session; }
			Ram_session_capability  ram_session_cap() { return  _ram_session_cap; }
			Rm_session             *rm_session()      { return &_rm_session; }
			Allocator              *heap()            { return &_heap; }

			Cpu_session *cpu_session()
			{
				PWRN("not implemented");
				return 0;
			}

			Pd_session *pd_session()
			{
				PWRN("not implemented");
				return 0;
			}
	};


	/**
	 * Request pointer to static environment of Core
	 */
	extern Core_env *core_env();
}

#endif /* _CORE__INCLUDE__CORE_ENV_H_ */
