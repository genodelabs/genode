/*
 * \brief  CPU thread interface
 * \author Norman Feske
 * \date   2016-05-10
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__CPU_THREAD__CPU_THREAD_H_
#define _INCLUDE__CPU_THREAD__CPU_THREAD_H_

#include <base/stdint.h>
#include <base/exception.h>
#include <base/thread_state.h>
#include <base/signal.h>
#include <base/affinity.h>
#include <dataspace/capability.h>

namespace Genode { struct Cpu_thread; }


struct Genode::Cpu_thread : Interface
{
	class State_access_failed : public Exception { };

	/**
	 * Get dataspace of the thread's user-level thread-control block (UTCB)
	 */
	virtual Dataspace_capability utcb() = 0;

	/**
	 * Modify instruction and stack pointer of thread - start the thread
	 *
	 * \param thread  thread to start
	 * \param ip      initial instruction pointer
	 * \param sp      initial stack pointer
	 */
	virtual void start(addr_t ip, addr_t sp) = 0;

	/**
	 * Pause the thread
	 *
	 * After calling this method, the execution of the thread can be
	 * continued by calling 'resume'.
	 */
	virtual void pause() = 0;

	/**
	 * Resume the thread
	 */
	virtual void resume() = 0;

	/**
	 * Cancel a currently blocking operation
	 */
	virtual void cancel_blocking() = 0;

	/**
	 * Get the current thread state
	 *
	 * \return  state of the targeted thread
	 * \throw   State_access_failed
	 */
	virtual Thread_state state() = 0;

	/**
	 * Override the current thread state
	 *
	 * \param state   state that shall be applied
	 * \throw         State_access_failed
	 */
	virtual void state(Thread_state const &state) = 0;

	/**
	 * Register signal handler for exceptions of the thread
	 *
	 * On Linux, this exception is delivered when the process triggers
	 * a SIGCHLD. On other platforms, this exception is delivered on
	 * the occurrence of CPU exceptions such as division by zero.
	 */
	virtual void exception_sigh(Signal_context_capability handler) = 0;

	/**
	 * Enable/disable single stepping
	 *
	 * Since this method is currently supported by a small number of
	 * platforms, we provide a default implementation
	 *
	 * \param enabled  true = enable single-step mode; false = disable
	 */
	virtual void single_step(bool enabled) = 0;

	/**
	 * Define affinity of thread to one or multiple CPU nodes
	 *
	 * In the normal case, a thread is assigned to a single CPU. Specifying
	 * more than one CPU node is supposed to principally allow a CPU service to
	 * balance the load of threads among multiple CPUs.
	 *
	 * \param location  location within the affinity space of the thread's
	 *                  CPU session
	 */
	virtual void affinity(Affinity::Location location) = 0;

	/**
	 * Request index within the trace control block of the thread's CPU session
	 *
	 * The trace control dataspace contains the control blocks for all threads
	 * of the CPU session. Each thread gets assigned a different index by the
	 * CPU service.
	 */
	virtual unsigned trace_control_index() = 0;

	/**
	 * Request trace buffer for the thread
	 *
	 * The trace buffer is not accounted to the CPU session. It is owned by a
	 * TRACE session.
	 */
	virtual Dataspace_capability trace_buffer() = 0;

	/**
	 * Request trace policy
	 *
	 * The trace policy buffer is not accounted to the CPU session. It is owned
	 * by a TRACE session.
	 */
	virtual Dataspace_capability trace_policy() = 0;


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC(Rpc_utcb, Dataspace_capability, utcb);
	GENODE_RPC(Rpc_start, void, start, addr_t, addr_t);
	GENODE_RPC(Rpc_pause, void, pause);
	GENODE_RPC(Rpc_resume, void, resume);
	GENODE_RPC(Rpc_cancel_blocking, void, cancel_blocking);
	GENODE_RPC_THROW(Rpc_get_state, Thread_state, state,
	                 GENODE_TYPE_LIST(State_access_failed));
	GENODE_RPC_THROW(Rpc_set_state, void, state,
	                 GENODE_TYPE_LIST(State_access_failed),
	                 Thread_state const &);
	GENODE_RPC(Rpc_exception_sigh, void, exception_sigh, Signal_context_capability);
	GENODE_RPC(Rpc_single_step, void, single_step, bool);
	GENODE_RPC(Rpc_affinity, void, affinity, Affinity::Location);
	GENODE_RPC(Rpc_trace_control_index, unsigned, trace_control_index);
	GENODE_RPC(Rpc_trace_buffer, Dataspace_capability, trace_buffer);
	GENODE_RPC(Rpc_trace_policy, Dataspace_capability, trace_policy);

	GENODE_RPC_INTERFACE(Rpc_utcb, Rpc_start, Rpc_pause, Rpc_resume,
	                     Rpc_cancel_blocking, Rpc_set_state, Rpc_get_state,
	                     Rpc_exception_sigh, Rpc_single_step, Rpc_affinity,
	                     Rpc_trace_control_index, Rpc_trace_buffer,
	                     Rpc_trace_policy);
};

#endif /* _INCLUDE__CPU_THREAD__CPU_THREAD_H_ */
