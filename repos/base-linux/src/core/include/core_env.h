/*
 * \brief  Core-specific environment for Linux
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2006-07-28
 *
 * The Core-specific environment ensures that all sessions of Core's
 * environment a local.
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__CORE_ENV_H_
#define _CORE__INCLUDE__CORE_ENV_H_

/* core includes */
#include <platform.h>
#include <core_parent.h>
#include <core_pd_session.h>
#include <ram_session_component.h>
#include <core_pd_session.h>
#include <base/service.h>

/* base-internal includes */
#include <base/internal/platform_env.h>

namespace Genode { void init_stack_area(); }

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


	class Core_env : public Platform_env_base
	{
		public:

			/**
			 * Entrypoint with support for local object access
			 *
			 * Within core, there are a few cases where the RPC objects must
			 * be invoked by direct function calls instead of using RPC.
			 * I.e., when an entrypoint dispatch function performs a memory
			 * allocation via the 'Sliced_heap', the 'attach' function of
			 * 'Rm_session_mmap' tries to obtain the dataspace's size and fd.
			 * Normally, this would be done by calling the entrypoint but the
			 * entrypoint cannot call itself. To support this special case,
			 * the 'Entrypoint' extends the 'Rpc_entrypoint' with the
			 * functionality needed to lookup an RPC object by its capability.
			 */
			struct Entrypoint : Rpc_entrypoint
			{
				enum { STACK_SIZE = 2048 * sizeof(Genode::addr_t) };

				Entrypoint()
				:
					Rpc_entrypoint(nullptr, STACK_SIZE, "entrypoint")
				{ }
			};

		private:

			typedef Synchronized_ram_session<Ram_session_component> Core_ram_session;

			/*
			 * Initialize the stack area before creating the first thread,
			 * which happens to be the '_entrypoint'.
			 */
			bool _init_stack_area() { init_stack_area(); return true; }
			bool _stack_area_initialized = _init_stack_area();

			Entrypoint                   _entrypoint;
			Core_ram_session             _ram_session;

			/*
			 * The core-local PD session is provided by a real RPC object
			 * dispatched by the same entrypoint as the signal-source RPC
			 * objects. This is needed to allow the 'Pd_session::submit'
			 * method to issue out-of-order replies to
			 * 'Signal_source::wait_for_signal' calls.
			 */
			Core_pd_session_component _pd_session_component;
			Pd_session_client         _pd_session_client;

			Heap                         _heap;
			Ram_session_capability const _ram_session_cap;

			Registry<Service> _services;

			Core_parent _core_parent { _heap, _services };

		public:

			/**
			 * Constructor
			 */
			Core_env()
			:
				Platform_env_base(Ram_session_capability(),
				                  Cpu_session_capability(),
				                  Pd_session_capability()),
				_ram_session(&_entrypoint, &_entrypoint,
				             platform()->ram_alloc(), platform()->core_mem_alloc(),
				             "ram_quota=4M", platform()->ram_alloc()->avail()),
				_pd_session_component(_entrypoint /* XXX use a different entrypoint */),
				_pd_session_client(_entrypoint.manage(&_pd_session_component)),
				_heap(&_ram_session, Platform_env_base::rm_session()),
				_ram_session_cap(_entrypoint.manage(&_ram_session))
			{ }

			/**
			 * Destructor
			 */
			~Core_env() { parent()->exit(0); }


			/**************************************
			 ** Core-specific accessor functions **
			 **************************************/

			Entrypoint  *entrypoint()  { return &_entrypoint; }


			/******************************
			 ** Env_deprecated interface **
			 ******************************/

			Parent                 *parent()          override { return &_core_parent; }
			Ram_session            *ram_session()     override { return &_ram_session; }
			Ram_session_capability  ram_session_cap() override { return  _ram_session_cap; }
			Pd_session             *pd_session()      override { return &_pd_session_client; }
			Allocator              *heap()            override { return &_heap; }

			Cpu_session_capability cpu_session_cap() override
			{
				warning(__FILE__, ":", __LINE__, " not implemented");
				return Cpu_session_capability();
			}

			Registry<Service> &services() { return _services; }
	};


	/**
	 * Request pointer to static environment of Core
	 */
	extern Core_env *core_env();
}

#endif /* _CORE__INCLUDE__CORE_ENV_H_ */
