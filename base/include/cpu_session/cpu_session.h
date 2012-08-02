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
 * Copyright (C) 2006-2012 Genode Labs GmbH
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
			 * Retrieve thread list of CPU session
			 *
			 * The next() function returns an invalid capability if the
			 * specified thread does not exists or if it is the last one
			 * of the CPU session.
			 */
			virtual Thread_capability first() = 0;
			virtual Thread_capability next(Thread_capability curr) = 0;

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
			 * Return thread state
			 *
			 * \param thread     thread to spy on
			 * \param state_dst  result
			 *
			 * \return           0 on success
			 */
			virtual int state(Thread_capability thread,
			                  Thread_state *state_dst) = 0;

			/**
			 * Register signal handler for exceptions of the specified thread
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
			virtual void single_step(Thread_capability thread, bool enable) {}

			/**
			 * Translate generic priority value to kernel-specific priority levels
			 *
			 * \param pf_prio_limit  maximum priority used for the kernel, must
			 *                       be power of 2
			 * \param prio           generic priority value as used by the CPU
			 *                       session interface
			 * \param inverse        order of platform priorities, if true
			 *                       'pf_prio_limit' corresponds to the highest
			 *                       priority, otherwise it refers to the
			 *                       lowest priority.
			 * \return               platform-specific priority value
			 */
			static unsigned scale_priority(unsigned pf_prio_limit, unsigned prio,
			                               bool inverse = true)
			{
				/* if no priorities are used, use the platform priority limit */
				if (prio == 0) return pf_prio_limit;

				/*
				 * Generic priority values are (0 is highest, 'PRIORITY_LIMIT'
				 * is lowest. On platforms where priority levels are defined
				 * the other way round, we have to invert the priority value.
				 */
				prio = inverse ? Cpu_session::PRIORITY_LIMIT - prio : prio;

				/* scale value to platform priority range 0..pf_prio_limit */
				return (prio*pf_prio_limit)/Cpu_session::PRIORITY_LIMIT;
			}

			/*********************
			 ** RPC declaration **
			 *********************/

			GENODE_RPC_THROW(Rpc_create_thread, Thread_capability, create_thread,
			                 GENODE_TYPE_LIST(Thread_creation_failed),
			                 Name const &, addr_t);
			GENODE_RPC(Rpc_utcb, Ram_dataspace_capability, utcb, Thread_capability);
			GENODE_RPC(Rpc_kill_thread, void, kill_thread, Thread_capability);
			GENODE_RPC(Rpc_first, Thread_capability, first,);
			GENODE_RPC(Rpc_next, Thread_capability, next, Thread_capability);
			GENODE_RPC(Rpc_set_pager, int, set_pager, Thread_capability, Pager_capability);
			GENODE_RPC(Rpc_start, int, start, Thread_capability, addr_t, addr_t);
			GENODE_RPC(Rpc_pause, void, pause, Thread_capability);
			GENODE_RPC(Rpc_resume, void, resume, Thread_capability);
			GENODE_RPC(Rpc_cancel_blocking, void, cancel_blocking, Thread_capability);
			GENODE_RPC(Rpc_state, int, state, Thread_capability, Thread_state *);
			GENODE_RPC(Rpc_exception_handler, void, exception_handler,
			                                  Thread_capability, Signal_context_capability);
			GENODE_RPC(Rpc_single_step, void, single_step, Thread_capability, bool);

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
			        Meta::Type_tuple<Rpc_first,
			        Meta::Type_tuple<Rpc_next,
			        Meta::Type_tuple<Rpc_set_pager,
			        Meta::Type_tuple<Rpc_start,
			        Meta::Type_tuple<Rpc_pause,
			        Meta::Type_tuple<Rpc_resume,
			        Meta::Type_tuple<Rpc_cancel_blocking,
			        Meta::Type_tuple<Rpc_state,
			        Meta::Type_tuple<Rpc_exception_handler,
			        Meta::Type_tuple<Rpc_single_step,
			                         Meta::Empty>
			        > > > > > > > > > > > > Rpc_functions;
	};
}

#endif /* _INCLUDE__CPU_SESSION__CPU_SESSION_H_ */
