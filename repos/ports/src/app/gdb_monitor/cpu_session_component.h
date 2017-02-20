/*
 * \brief  Core-specific instance of the CPU session/thread interfaces
 * \author Christian Helmuth
 * \date   2006-07-17
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CPU_SESSION_COMPONENT_H_
#define _CPU_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/allocator.h>
#include <base/env.h>
#include <base/rpc_server.h>
#include <base/service.h>
#include <base/thread.h>
#include <base/printf.h>
#include <cpu_session/client.h>
#include <parent/parent.h>
#include <pd_session/capability.h>

/* GDB monitor includes */
#include "append_list.h"
#include "genode_child_resources.h"

namespace Gdb_monitor
{
	using namespace Genode;

	class Cpu_session_component;
	class Cpu_thread_component;
	class Local_cpu_factory;

	typedef Local_service<Cpu_session_component> Cpu_service;
}


class Gdb_monitor::Cpu_session_component : public Rpc_object<Cpu_session>
{

	private:

		Env                                     &_env;

		Parent::Client                           _parent_client;
		
		Id_space<Parent::Client>::Element const  _id_space_element
		{ _parent_client, _env.id_space() };

		Rpc_entrypoint                          &_ep;
		Allocator                               &_md_alloc;

		Pd_session_capability                    _core_pd;

		Cpu_session_client                       _parent_cpu_session;
		Signal_receiver                         &_exception_signal_receiver;

		Append_list<Cpu_thread_component>        _thread_list;

		bool                                     _stop_new_threads = true;
		Lock                                     _stop_new_threads_lock;

		Capability<Cpu_session::Native_cpu>      _native_cpu_cap;

		Capability<Cpu_session::Native_cpu> _setup_native_cpu();
		void _cleanup_native_cpu();

	public:

		/**
		 * Constructor
		 */
		Cpu_session_component(Env                   &env,
		                      Rpc_entrypoint        &ep,
		                      Allocator             &md_alloc,
		                      Pd_session_capability  core_pd,
		                      Signal_receiver       &exception_signal_receiver,
		                      const char            *args,
		                      Affinity const        &affinity);

		/**
		 * Destructor
		 */
		~Cpu_session_component();

		Cpu_session &parent_cpu_session();
		Rpc_entrypoint &thread_ep();
		Signal_receiver &exception_signal_receiver();
		Thread_capability thread_cap(unsigned long lwpid);
		unsigned long lwpid(Thread_capability thread_cap);
		Cpu_thread_component *lookup_cpu_thread(unsigned long lwpid);
		Cpu_thread_component *lookup_cpu_thread(Thread_capability thread_cap);
		int signal_pipe_read_fd(Thread_capability thread_cap);
		int send_signal(Thread_capability thread_cap, int signo);
		void handle_unresolved_page_fault();
		void stop_new_threads(bool stop);
		bool stop_new_threads();
		Lock &stop_new_threads_lock();
		int handle_initial_breakpoint(unsigned long lwpid);
		void pause_all_threads();
		void resume_all_threads();
		Thread_capability first();
		Thread_capability next(Thread_capability);

		/***************************
		 ** CPU session interface **
		 ***************************/

		Thread_capability create_thread(Capability<Pd_session>,
		                                Name const &,
		                                Affinity::Location,
		                                Weight,
		                                addr_t) override;
		void kill_thread(Thread_capability) override;
		void exception_sigh(Signal_context_capability handler) override;
		Affinity::Space affinity_space() const override;
		Dataspace_capability trace_control() override;
		int ref_account(Cpu_session_capability c) override;
		int transfer_quota(Cpu_session_capability c, size_t q) override;
		Quota quota() override;
		Capability<Native_cpu> native_cpu() override;
};


class Gdb_monitor::Local_cpu_factory : public Cpu_service::Factory
{
	private:

		Env                     &_env;
		Rpc_entrypoint          &_ep;

		Allocator               &_md_alloc;
		Pd_session_capability    _core_pd;
		Signal_receiver         &_signal_receiver;
		Genode_child_resources  *_genode_child_resources;


	public:

		Local_cpu_factory(Env                    &env,
		                  Rpc_entrypoint         &ep,
		                  Allocator              &md_alloc,
		                  Pd_session_capability   core_pd,
		                  Signal_receiver        &signal_receiver,
		                  Genode_child_resources *genode_child_resources)
		: _env(env), _ep(ep),
		  _md_alloc(md_alloc),
		  _core_pd(core_pd),
		  _signal_receiver(signal_receiver),
		  _genode_child_resources(genode_child_resources)
		  { }

		/***********************
		 ** Factory interface **
		 ***********************/

		Cpu_session_component &create(Args const &args, Affinity affinity) override
		{
			Cpu_session_component *cpu_session_component =
				new (_md_alloc)
					Cpu_session_component(_env,
					                      _ep,
					                      _md_alloc,
					                      _core_pd,
					                      _signal_receiver,
					                      args.string(),
					                      affinity);
			_genode_child_resources->cpu_session_component(cpu_session_component);
			return *cpu_session_component;
		}

		void upgrade(Cpu_session_component &, Args const &) override { }

		void destroy(Cpu_session_component &session) override
		{
			Genode::destroy(_md_alloc, &session);
		}
};

#endif /* _CPU_SESSION_COMPONENT_H_ */
