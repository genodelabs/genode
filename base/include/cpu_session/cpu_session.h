/*
 * \brief  CPU (processing time) manager session interface
 * \author Christian Helmuth
 * \date   2006-06-27
 *
 * :Question:
 *
 *   Why are thread operations not methods of the thread but
 *   methods of the CPU session?
 *
 * :Answer:
 *
 *   This enables the CPU session to impose policies on thread
 *   operations. These policies are based on the session
 *   construction arguments. If thread operations would be
 *   provided as thread methods, Thread would need to consult
 *   its container object (its CPU session) about the authorization
 *   of each operation and, thereby, would introduce a circular
 *   dependency between CPU session and Thread.
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__CPU_SESSION__CPU_SESSION_H_
#define _INCLUDE__CPU_SESSION__CPU_SESSION_H_

#include <base/stdint.h>
#include <base/exception.h>
#include <base/thread_state.h>
#include <base/rpc_args.h>
#include <base/signal.h>
#include <base/affinity.h>
#include <thread/capability.h>
#include <pager/capability.h>
#include <session/session.h>
#include <ram_session/ram_session.h>

namespace Genode {

	struct Cpu_session : Session
	{
			/*********************
			 ** Exception types **
			 *********************/

			class Thread_creation_failed : public Exception { };
			class State_access_failed : public Exception { };
			class Out_of_metadata : public Exception { };

			static const char *service_name() { return "CPU"; }

			enum { THREAD_NAME_LEN = 48 };
			enum { PRIORITY_LIMIT = 1 << 16 };
			enum { DEFAULT_PRIORITY = 0 };

			typedef Rpc_in_buffer<THREAD_NAME_LEN> Name;

			virtual ~Cpu_session() { }

			/**
			 * Create a new thread
			 *
			 * \param   name  name for the thread
			 * \param   utcb  Base of the UTCB that will be used by the thread
			 * \return        capability representing the new thread
			 * \throw         Thread_creation_failed
			 * \throw         Out_of_metadata
			 */
			virtual Thread_capability create_thread(Name const &name,
			                                        addr_t utcb = 0) = 0;

			/**
			 * Get dataspace of the UTCB that is used by the specified thread
			 */
			virtual Ram_dataspace_capability utcb(Thread_capability thread) = 0;

			/**
			 * Kill an existing thread
			 *
			 * \param thread  capability of the thread to kill
			 */
			virtual void kill_thread(Thread_capability thread) = 0;

			/**
			 * Set paging capabilities for thread
			 *
			 * \param thread  thread to configure
			 * \param pager   capability used to propagate page faults
			 */
			virtual int set_pager(Thread_capability thread,
			                      Pager_capability  pager) = 0;

			/**
			 * Modify instruction and stack pointer of thread - start the
			 * thread
			 *
			 * \param thread  thread to start
			 * \param ip      initial instruction pointer
			 * \param sp      initial stack pointer
			 *
			 * \return        0 on success
			 */
			virtual int start(Thread_capability thread, addr_t ip, addr_t sp) = 0;

			/**
			 * Pause the specified thread
			 *
			 * After calling this function, the execution of the thread can be
			 * continued by calling 'resume'.
			 */
			virtual void pause(Thread_capability thread) = 0;

			/**
			 * Resume the specified thread
			 */
			virtual void resume(Thread_capability thread) = 0;

			/**
			 * Cancel a currently blocking operation
			 *
			 * \param thread  thread to unblock
			 */
			virtual void cancel_blocking(Thread_capability thread) = 0;

			/**
			 * Get the current state of a specific thread
			 *
			 * \param thread  targeted thread
			 * \return        state of the targeted thread
			 * \throw         State_access_failed
			 */
			virtual Thread_state state(Thread_capability thread) = 0;

			/**
			 * Override the current state of a specific thread
			 *
			 * \param thread  targeted thread
			 * \param state   state that shall be applied
			 * \throw         State_access_failed
			 */
			virtual void state(Thread_capability thread,
			                   Thread_state const &state) = 0;

			/**
			 * Register signal handler for exceptions of the specified thread
			 *
			 * If 'thread' is an invalid capability, the default exception
			 * handler for the CPU session is set. This handler is used for
			 * all threads that have no explicitly installed exception handler.
			 * The new default signal handler will take effect for threads
			 * created after the call.
			 *
			 * On Linux, this exception is delivered when the process triggers
			 * a SIGCHLD. On other platforms, this exception is delivered on
			 * the occurrence of CPU exceptions such as division by zero.
			 */
			virtual void exception_handler(Thread_capability         thread,
			                               Signal_context_capability handler) = 0;

			/**
			 * Enable/disable single stepping for specified thread.
			 *
			 * Since this functions is currently supported by a small number of
			 * platforms, we provide a default implementation
			 *
			 * \param thread  thread to set into single step mode
			 * \param enable  true = enable single-step mode; false = disable
			 */
			virtual void single_step(Thread_capability, bool) {}

			/**
			 * Return affinity space of CPU nodes available to the CPU session
			 *
			 * The dimension of the affinity space as returned by this function
			 * represent the physical CPUs that are available.
			 */
			virtual Affinity::Space affinity_space() const = 0;

			/**
			 * Define affinity of thread to one or multiple CPU nodes
			 *
			 * In the normal case, a thread is assigned to a single CPU.
			 * Specifying more than one CPU node is supposed to principally
			 * allow a CPU service to balance the load of threads among
			 * multiple CPUs.
			 */
			virtual void affinity(Thread_capability thread,
			                      Affinity::Location affinity) = 0;

			/**
			 * Translate generic priority value to kernel-specific priority levels
			 *
			 * \param pf_prio_limit  maximum priority used for the kernel, must
			 *                       be power of 2
			 * \param prio           generic priority value as used by the CPU
			 *                       session interface
			 * \return               platform-specific priority value
			 */
			static unsigned scale_priority(unsigned pf_prio_limit, unsigned prio)
			{
				/* if no priorities are used, use the platform priority limit */
				if (prio == 0) return pf_prio_limit;

				/* scale value to platform priority range 0..pf_prio_limit */
				return (prio*pf_prio_limit)/Cpu_session::PRIORITY_LIMIT;
			}

			/**
			 * Request trace control dataspace
			 *
			 * The trace-control dataspace is used to propagate tracing
			 * control information from core to the threads of a CPU session.
			 *
			 * The trace-control dataspace is accounted to the CPU session.
			 */
			virtual Dataspace_capability trace_control() = 0;

			/**
			 * Request index of a trace control block for given thread
			 *
			 * The trace control dataspace contains the control blocks for
			 * all threads of the CPU session. Each thread gets assigned a
			 * different index by the CPU service.
			 */
			virtual unsigned trace_control_index(Thread_capability thread) = 0;

			/**
			 * Request trace buffer for the specified thread
			 *
			 * The trace buffer is not accounted to the CPU session. It is
			 * owned by a TRACE session.
			 */
			virtual Dataspace_capability trace_buffer(Thread_capability thread) = 0;

			/**
			 * Request trace policy
			 *
			 * The trace policy buffer is not accounted to the CPU session. It
			 * is owned by a TRACE session.
			 */
			virtual Dataspace_capability trace_policy(Thread_capability thread) = 0;


			/*********************
			 ** RPC declaration **
			 *********************/

			GENODE_RPC_THROW(Rpc_create_thread, Thread_capability, create_thread,
			                 GENODE_TYPE_LIST(Thread_creation_failed, Out_of_metadata),
			                 Name const &, addr_t);
			GENODE_RPC(Rpc_utcb, Ram_dataspace_capability, utcb, Thread_capability);
			GENODE_RPC(Rpc_kill_thread, void, kill_thread, Thread_capability);
			GENODE_RPC(Rpc_set_pager, int, set_pager, Thread_capability, Pager_capability);
			GENODE_RPC(Rpc_start, int, start, Thread_capability, addr_t, addr_t);
			GENODE_RPC(Rpc_pause, void, pause, Thread_capability);
			GENODE_RPC(Rpc_resume, void, resume, Thread_capability);
			GENODE_RPC(Rpc_cancel_blocking, void, cancel_blocking, Thread_capability);
			GENODE_RPC_THROW(Rpc_get_state, Thread_state, state,
			                 GENODE_TYPE_LIST(State_access_failed),
			                 Thread_capability);
			GENODE_RPC_THROW(Rpc_set_state, void, state,
			                 GENODE_TYPE_LIST(State_access_failed),
			                 Thread_capability, Thread_state const &);
			GENODE_RPC(Rpc_exception_handler, void, exception_handler,
			                                  Thread_capability, Signal_context_capability);
			GENODE_RPC(Rpc_single_step, void, single_step, Thread_capability, bool);
			GENODE_RPC(Rpc_affinity_space, Affinity::Space, affinity_space);
			GENODE_RPC(Rpc_affinity, void, affinity, Thread_capability, Affinity::Location);
			GENODE_RPC(Rpc_trace_control, Dataspace_capability, trace_control);
			GENODE_RPC(Rpc_trace_control_index, unsigned, trace_control_index, Thread_capability);
			GENODE_RPC(Rpc_trace_buffer, Dataspace_capability, trace_buffer, Thread_capability);
			GENODE_RPC(Rpc_trace_policy, Dataspace_capability, trace_policy, Thread_capability);

			/*
			 * 'GENODE_RPC_INTERFACE' declaration done manually
			 *
			 * The number of RPC function of this interface exceeds the maximum
			 * number of elements supported by 'Meta::Type_list'. Therefore, we
			 * construct the type list by hand using nested type tuples instead
			 * of employing the convenience macro 'GENODE_RPC_INTERFACE'.
			 */
			typedef Meta::Type_tuple<Rpc_create_thread,
			        Meta::Type_tuple<Rpc_utcb,
			        Meta::Type_tuple<Rpc_kill_thread,
			        Meta::Type_tuple<Rpc_set_pager,
			        Meta::Type_tuple<Rpc_start,
			        Meta::Type_tuple<Rpc_pause,
			        Meta::Type_tuple<Rpc_resume,
			        Meta::Type_tuple<Rpc_cancel_blocking,
			        Meta::Type_tuple<Rpc_set_state,
			        Meta::Type_tuple<Rpc_get_state,
			        Meta::Type_tuple<Rpc_exception_handler,
			        Meta::Type_tuple<Rpc_single_step,
			        Meta::Type_tuple<Rpc_affinity_space,
			        Meta::Type_tuple<Rpc_affinity,
			        Meta::Type_tuple<Rpc_trace_control,
			        Meta::Type_tuple<Rpc_trace_control_index,
			        Meta::Type_tuple<Rpc_trace_buffer,
			        Meta::Type_tuple<Rpc_trace_policy,
			                         Meta::Empty>
			        > > > > > > > > > > > > > > > > > Rpc_functions;
	};
}

#endif /* _INCLUDE__CPU_SESSION__CPU_SESSION_H_ */
