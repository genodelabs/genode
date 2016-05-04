/*
 * \brief  Core-specific instance of the CPU session/thread interfaces
 * \author Christian Helmuth
 * \date   2006-07-17
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CPU_SESSION_COMPONENT_H_
#define _CPU_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/rpc_server.h>
#include <cpu_session/client.h>

/* GDB monitor includes */
#include "thread_info.h"

using namespace Genode;
using namespace Gdb_monitor;

class Cpu_session_component : public Rpc_object<Cpu_session>
{
	private:

		Cpu_session_client _parent_cpu_session;
		Signal_receiver *_exception_signal_receiver;

		Append_list<Thread_info> _thread_list;

		Thread_info *_thread_info(Thread_capability thread_cap);

	public:

		/**
		 * Constructor
		 */
		Cpu_session_component(Signal_receiver *exception_signal_receiver, const char *args);

		/**
		 * Destructor
		 */
		~Cpu_session_component();

		unsigned long lwpid(Thread_capability thread_cap);
		Thread_capability thread_cap(unsigned long lwpid);
		Thread_capability first();
		Thread_capability next(Thread_capability);

		/***************************
		 ** CPU session interface **
		 ***************************/

		Thread_capability create_thread(Capability<Pd_session>, Name const &,
		                                Affinity::Location, Weight, addr_t) override;
		Ram_dataspace_capability utcb(Thread_capability thread) override;
		void kill_thread(Thread_capability) override;
		int start(Thread_capability, addr_t, addr_t) override;
		void pause(Thread_capability thread_cap) override;
		void resume(Thread_capability thread_cap) override;
		void cancel_blocking(Thread_capability) override;
		Thread_state state(Thread_capability) override;
		void state(Thread_capability, Thread_state const &) override;
		void exception_handler(Thread_capability         thread,
		                       Signal_context_capability handler) override;
		void single_step(Thread_capability thread, bool enable) override;
		Affinity::Space affinity_space() const override;
		void affinity(Thread_capability, Affinity::Location) override;
		Dataspace_capability trace_control() override;
		unsigned trace_control_index(Thread_capability) override;
		Dataspace_capability trace_buffer(Thread_capability) override;
		Dataspace_capability trace_policy(Thread_capability) override;
		int ref_account(Cpu_session_capability c) override;
		int transfer_quota(Cpu_session_capability c, size_t q) override;
		Quota quota() override;
		Capability<Native_cpu> native_cpu() override;
};

#endif /* _CPU_SESSION_COMPONENT_H_ */
