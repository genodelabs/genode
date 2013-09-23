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

		Thread_info *_thread_info(Thread_capability const &thread_cap);

	public:

		/**
		 * Constructor
		 */
		Cpu_session_component(Signal_receiver *exception_signal_receiver,
		                      const char *args);

		/**
		 * Destructor
		 */
		~Cpu_session_component();

		unsigned long lwpid(Thread_capability const &thread_cap);
		Thread_capability thread_cap(unsigned long lwpid);
		Thread_capability first();
		Thread_capability next(Thread_capability const &);

		/***************************
		 ** CPU session interface **
		 ***************************/

		Thread_capability create_thread(Name const &, addr_t);
		Ram_dataspace_capability utcb(Thread_capability const &thread);
		void kill_thread(Thread_capability const &);
		int set_pager(Thread_capability const &, Pager_capability const &);
		int start(Thread_capability const &, addr_t, addr_t);
		void pause(Thread_capability const &);
		void resume(Thread_capability const &);
		void cancel_blocking(Thread_capability const &);
		int name(Thread_capability const &, char *, Genode::size_t);
		Thread_state state(Thread_capability const &);
		void state(Thread_capability const &, Thread_state const &);
		void exception_handler(Thread_capability const &,
		                       Signal_context_capability const &);
		void single_step(Thread_capability const &thread, bool enable);
		Affinity::Space affinity_space() const;
		void affinity(Thread_capability const &, Affinity::Location);
		Dataspace_capability trace_control();
		unsigned trace_control_index(Thread_capability const &);
		Dataspace_capability trace_buffer(Thread_capability const &);
		Dataspace_capability trace_policy(Thread_capability const &);
};

#endif /* _CPU_SESSION_COMPONENT_H_ */
