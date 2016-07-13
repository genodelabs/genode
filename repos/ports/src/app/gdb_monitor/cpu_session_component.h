/*
 * \brief  Core-specific instance of the CPU session/thread interfaces
 * \author Christian Helmuth
 * \date   2006-07-17
 */

/*
 * Copyright (C) 2006-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CPU_SESSION_COMPONENT_H_
#define _CPU_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/allocator.h>
#include <base/rpc_server.h>
#include <base/thread.h>
#include <base/printf.h>
#include <cpu_session/client.h>
#include <pd_session/capability.h>

/* GDB monitor includes */
#include "append_list.h"

namespace Gdb_monitor
{
	class Cpu_session_component;
	class Cpu_thread_component;
	using namespace Genode;
}

class Gdb_monitor::Cpu_session_component : public Rpc_object<Cpu_session>
{

	private:

		Rpc_entrypoint *_thread_ep;
		Allocator      *_md_alloc;

		Pd_session_capability _core_pd;

		Cpu_session_client _parent_cpu_session;
		Signal_receiver *_exception_signal_receiver;

		Append_list<Cpu_thread_component> _thread_list;

		bool _stop_new_threads = true;
		Lock _stop_new_threads_lock;

		Capability<Cpu_session::Native_cpu> _native_cpu_cap;

		Capability<Cpu_session::Native_cpu> _setup_native_cpu();
		void _cleanup_native_cpu();

	public:

		/**
		 * Constructor
		 */
		Cpu_session_component(Rpc_entrypoint *thread_ep,
		                      Allocator *md_alloc,
		                      Pd_session_capability core_pd,
		                      Signal_receiver *exception_signal_receiver,
		                      const char *args);

		/**
		 * Destructor
		 */
		~Cpu_session_component();

		Cpu_session &parent_cpu_session();
		Rpc_entrypoint &thread_ep();
		Signal_receiver *exception_signal_receiver();
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

#endif /* _CPU_SESSION_COMPONENT_H_ */
