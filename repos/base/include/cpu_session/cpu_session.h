/*
 * \brief  CPU (processing time) manager session interface
 * \author Christian Helmuth
 * \date   2006-06-27
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__CPU_SESSION__CPU_SESSION_H_
#define _INCLUDE__CPU_SESSION__CPU_SESSION_H_

#include <util/attempt.h>
#include <cpu_session/capability.h>
#include <cpu_thread/cpu_thread.h>
#include <base/rpc_args.h>
#include <session/session.h>
#include <dataspace/capability.h>
#include <pd_session/pd_session.h>

namespace Genode {

	struct Cpu_session;
	struct Cpu_session_client;

	using Thread_capability = Capability<Cpu_thread>;
}


struct Genode::Cpu_session : Session
{
	/**
	 * \noapi
	 */
	static const char *service_name() { return "CPU"; }

	/*
	 * A CPU session consumes a dataspace capability for the session-object
	 * allocation, its session capability, the capability of the 'Native_cpu'
	 * RPC interface, and a capability for the trace-control dataspace.
	 */
	static constexpr unsigned CAP_QUOTA = 6;
	static constexpr size_t   RAM_QUOTA = 36*1024;

	using Client = Cpu_session_client;


	/*********************
	 ** Exception types **
	 *********************/

	enum { PRIORITY_LIMIT = 1 << 16 };
	enum { QUOTA_LIMIT_LOG2 = 15 };
	enum { QUOTA_LIMIT = 1 << QUOTA_LIMIT_LOG2 };
	enum { DEFAULT_PRIORITY = 0 };

	/**
	 * Thread weight argument type for 'create_thread'
	 */
	struct Weight
	{
		static constexpr size_t DEFAULT_WEIGHT = 10;
		size_t value = DEFAULT_WEIGHT;
		Weight() { }
		explicit Weight(size_t value) : value(value) { }
	};

	using Name = String<32>;

	/**
	 * Physical quota configuration
	 */
	struct Quota;

	virtual ~Cpu_session() { }

	enum class Create_thread_error { OUT_OF_RAM, OUT_OF_CAPS, DENIED };
	using Create_thread_result = Attempt<Thread_capability, Create_thread_error>;

	/**
	 * Create a new thread
	 *
	 * \param pd        protection domain where the thread will be executed
	 * \param name      name for the thread
	 * \param affinity  CPU affinity, referring to the session-local
	 *                  affinity space
	 * \param weight    CPU quota that shall be granted to the thread
	 * \param utcb      base of the UTCB that will be used by the thread
	 * \return          capability representing the new thread
	 */
	virtual Create_thread_result create_thread(Capability<Pd_session> pd,
	                                           Name const            &name,
	                                           Affinity::Location     affinity,
	                                           Weight                 weight,
	                                           addr_t                 utcb = 0) = 0;

	/**
	 * Kill an existing thread
	 *
	 * \param thread  capability of the thread to kill
	 */
	virtual void kill_thread(Thread_capability thread) = 0;

	/**
	 * Register default signal handler for exceptions
	 *
	 * This handler is used for all threads that have no explicitly installed
	 * exception handler.
	 *
	 * On Linux, this exception is delivered when the process triggers
	 * a SIGCHLD. On other platforms, this exception is delivered on
	 * the occurrence of CPU exceptions such as division by zero.
	 */
	virtual void exception_sigh(Signal_context_capability) = 0;

	/**
	 * Return affinity space of CPU nodes available to the CPU session
	 *
	 * The dimension of the affinity space as returned by this method
	 * represent the physical CPUs that are available.
	 */
	virtual Affinity::Space affinity_space() const = 0;

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
		/*
		 * Generic priority values are (0 is highest, 'PRIORITY_LIMIT'
		 * is lowest. On platforms where priority levels are defined
		 * the other way round, we have to invert the priority value.
		 */
		prio = inverse ? Cpu_session::PRIORITY_LIMIT - prio : prio;

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
	 * Define reference account for the CPU session
	 *
	 * \param   cpu_session    reference account
	 *
	 * \return  0 on success
	 *
	 * Each CPU session requires another CPU session as reference
	 * account to transfer quota to and from. The reference account can
	 * be defined only once.
	 */
	virtual int ref_account(Cpu_session_capability cpu_session) = 0;

	/**
	 * Transfer quota to another CPU session
	 *
	 * \param cpu_session  receiver of quota donation
	 * \param amount       percentage of the session quota scaled up to
	 *                     the 'QUOTA_LIMIT' space
	 * \return             0 on success
	 *
	 * Quota can only be transfered if the specified CPU session is
	 * either the reference account for this session or vice versa.
	 */
	virtual int transfer_quota(Cpu_session_capability cpu_session,
	                           size_t amount) = 0;

	/**
	 * Return quota configuration of the session
	 */
	virtual Quota quota() = 0;

	/**
	 * Scale up 'value' from its space with 'limit' to the 'QUOTA_LIMIT' space
	 */
	template<typename T = size_t>
	static size_t quota_lim_upscale(size_t const value, size_t const limit) {
		return ((T)value << Cpu_session::QUOTA_LIMIT_LOG2) / limit; }

	/**
	 * Scale down 'value' from the 'QUOTA_LIMIT' space to a space with 'limit'
	 */
	static size_t quota_lim_downscale(size_t const value, size_t const limit) {
		return (size_t)(((uint64_t)value * limit) >> Cpu_session::QUOTA_LIMIT_LOG2); }


	/*****************************************
	 ** Access to kernel-specific interface **
	 *****************************************/

	/**
	 * Common base class of kernel-specific CPU interfaces
	 */
	struct Native_cpu;

	/**
	 * Return capability to kernel-specific CPU operations
	 */
	virtual Capability<Native_cpu> native_cpu() = 0;


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC(Rpc_create_thread, Create_thread_result, create_thread,
	           Capability<Pd_session>, Name const &, Affinity::Location,
	           Weight, addr_t);
	GENODE_RPC(Rpc_kill_thread, void, kill_thread, Thread_capability);
	GENODE_RPC(Rpc_exception_sigh, void, exception_sigh, Signal_context_capability);
	GENODE_RPC(Rpc_affinity_space, Affinity::Space, affinity_space);
	GENODE_RPC(Rpc_trace_control, Dataspace_capability, trace_control);
	GENODE_RPC(Rpc_ref_account, int, ref_account, Cpu_session_capability);
	GENODE_RPC(Rpc_transfer_quota, int, transfer_quota, Cpu_session_capability, size_t);
	GENODE_RPC(Rpc_quota, Quota, quota);
	GENODE_RPC(Rpc_native_cpu, Capability<Native_cpu>, native_cpu);

	GENODE_RPC_INTERFACE(Rpc_create_thread, Rpc_kill_thread, Rpc_exception_sigh,
	                     Rpc_affinity_space, Rpc_trace_control, Rpc_ref_account,
	                     Rpc_transfer_quota, Rpc_quota, Rpc_native_cpu);
};


struct Genode::Cpu_session::Quota
{
	size_t super_period_us;
	size_t us;
};

#endif /* _INCLUDE__CPU_SESSION__CPU_SESSION_H_ */
