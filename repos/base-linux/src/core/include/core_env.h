/*
 * \brief  Core-specific environment for Linux
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
#include <base/service.h>

/* core includes */
#include <platform.h>
#include <core_parent.h>
#include <core_pd_session.h>
#include <ram_session_component.h>
#include <core_pd_session.h>

/* base-internal includes */
#include <base/internal/platform_env.h>

namespace Genode { void init_stack_area(); }

namespace Genode {

	/**
	 * Lock-guarded wrapper for a RAM session
	 *
	 * In contrast to regular components, core's RAM session is not
	 * synchronized via the RPC entrypoint.
	 */
	class Synced_ram_session : public Ram_session
	{
		private:

			Lock mutable _lock;
			Ram_session &_ram_session;

		public:

			Synced_ram_session(Ram_session &ram_session) : _ram_session(ram_session) { }


			/***************************
			 ** RAM-session interface **
			 ***************************/

			Ram_dataspace_capability alloc(size_t size, Cache_attribute cached) override
			{
				Lock::Guard lock_guard(_lock);
				return _ram_session.alloc(size, cached);
			}

			void free(Ram_dataspace_capability ds) override
			{
				Lock::Guard lock_guard(_lock);
				_ram_session.free(ds);
			}

			size_t dataspace_size(Ram_dataspace_capability ds) const override
			{
				Lock::Guard lock_guard(_lock);
				return _ram_session.dataspace_size(ds);
			}

			void ref_account(Ram_session_capability session) override
			{
				Lock::Guard lock_guard(_lock);
				_ram_session.ref_account(session);
			}

			void transfer_quota(Ram_session_capability session, Ram_quota amount) override
			{
				Lock::Guard lock_guard(_lock);
				_ram_session.transfer_quota(session, amount);
			}

			Ram_quota ram_quota() const override
			{
				Lock::Guard lock_guard(_lock);
				return _ram_session.ram_quota();
			}

			Ram_quota used_ram() const override
			{
				Lock::Guard lock_guard(_lock);
				return _ram_session.used_ram();
			}
	};


	class Core_env : public Platform_env_base
	{
		private:

			enum { STACK_SIZE = 2048 * sizeof(Genode::addr_t) };

			/*
			 * Initialize the stack area before creating the first thread,
			 * which happens to be the '_entrypoint'.
			 */
			bool _init_stack_area() { init_stack_area(); return true; }
			bool _stack_area_initialized = _init_stack_area();

			Rpc_entrypoint        _entrypoint { nullptr, STACK_SIZE, "entrypoint" };
			Ram_session_component _ram_session;
			Synced_ram_session    _synced_ram_session { _ram_session };

			/*
			 * The core-local PD session is provided by a real RPC object
			 * dispatched by the same entrypoint as the signal-source RPC
			 * objects. This is needed to allow the 'Pd_session::submit'
			 * method to issue out-of-order replies to
			 * 'Signal_source::wait_for_signal' calls.
			 */
			Core_pd_session_component _pd_session_component;
			Pd_session_client         _pd_session_client;

			Registry<Service> _services;

			Heap _heap { _synced_ram_session, *Platform_env_base::rm_session() };

			Core_parent _core_parent { _heap, _services };

			typedef String<100> Ram_args;

			static Session::Resources _ram_resources()
			{
				return { Ram_quota { platform()->ram_alloc()->avail() },
				         Cap_quota { 1000 } };
			}

		public:

			/**
			 * Constructor
			 */
			Core_env()
			:
				Platform_env_base(Ram_session_capability(),
				                  Cpu_session_capability(),
				                  Pd_session_capability()),
				_ram_session(_entrypoint,
				             _ram_resources(),
				             Session::Label("core"),
				             Session::Diag{false},
				             *platform()->ram_alloc(),
				             *Platform_env_base::rm_session(),
				             Ram_session_component::any_phys_range()),
				_pd_session_component(_entrypoint),
				_pd_session_client(_entrypoint.manage(&_pd_session_component))
			{
				_ram_session.init_ram_account();
			}

			/**
			 * Destructor
			 */
			~Core_env() { parent()->exit(0); }


			Rpc_entrypoint *entrypoint() { return &_entrypoint; }


			/******************************
			 ** Env_deprecated interface **
			 ******************************/

			Parent                 *parent()          override { return &_core_parent; }
			Ram_session            *ram_session()     override { return &_ram_session; }
			Ram_session_capability  ram_session_cap() override { return  _ram_session.cap(); }
			Pd_session             *pd_session()      override { return &_pd_session_client; }
			Allocator              *heap()            override { log(__func__, ": not implemented"); return nullptr; }

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
