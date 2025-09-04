/*
 * \brief  Interface between kernel and userland
 * \author Stefan Kalkowski
 * \author Martin stein
 * \date   2011-11-30
 */

/*
 * Copyright (C) 2011-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__KERNEL__INTERFACE_H_
#define _INCLUDE__KERNEL__INTERFACE_H_

/* base-hw includes */
#include <kernel/types.h>

namespace Kernel {

	/**
	 * Kernel system call IDs
	 */
	enum class Call_id : Call_arg {
		CACHE_CLEAN_INV,
		CACHE_COHERENT,
		CACHE_INV,
		CACHE_SIZE,
		CAP_ACK,
		CAP_DESTROY,
		PRINT,
		RPC_CALL,
		RPC_REPLY,
		RPC_REPLY_AND_WAIT,
		RPC_WAIT,
		SIG_ACK,
		SIG_KILL,
		SIG_PENDING,
		SIG_SUBMIT,
		SIG_WAIT,
		THREAD_RESTART,
		THREAD_STOP,
		THREAD_YIELD,
		TIME,
		TIMEOUT,
		TIMEOUT_MAX_US,
		VCPU_PAUSE,
		VCPU_RUN,
	};


	/******************************************************************
	 ** Kernel call with 1 to 6 arguments                            **
	 **                                                              **
	 ** These functions must not be inline to ensure that objects,   **
	 ** which are referenced by arguments, are tagged as "used" even **
	 ** though only the pointer gets handled in here.                **
	 ******************************************************************/

	Call_ret arch_call(Call_arg arg_0);

	Call_ret arch_call(Call_arg arg_0,
	                   Call_arg arg_1);

	Call_ret arch_call(Call_arg arg_0,
	                   Call_arg arg_1,
	                   Call_arg arg_2);

	Call_ret arch_call(Call_arg arg_0,
	                   Call_arg arg_1,
	                   Call_arg arg_2,
	                   Call_arg arg_3);

	Call_ret arch_call(Call_arg arg_0,
	                   Call_arg arg_1,
	                   Call_arg arg_2,
	                   Call_arg arg_3,
	                   Call_arg arg_4);

	Call_ret arch_call(Call_arg arg_0,
	                   Call_arg arg_1,
	                   Call_arg arg_2,
	                   Call_arg arg_3,
	                   Call_arg arg_4,
	                   Call_arg arg_5);

	Call_ret_64 arch_call_64(Call_arg arg_0);

	inline auto call(Call_id id, auto &&... args)
	{
		return arch_call((Call_arg)id, args...);
	}

	/**
	 * Install timeout for calling thread
	 *
	 * \param  duration_us  timeout duration in microseconds
	 * \param  sigid        local name of signal context to trigger
	 *
	 * This call overwrites the last timeout installed by the thread.
	 */
	inline void timeout(timeout_t const duration_us, capid_t const sigid)
	{
		call(Call_id::TIMEOUT, duration_us, sigid);
	}


	/**
	 * Return value of a free-running, uniform counter
	 *
	 * The counter has a constant frequency and does not wrap twice during
	 * a time period of 'timeout_max_us()' microseconds.
	 */
	inline time_t time()
	{
		return arch_call_64((Call_arg)Call_id::TIME);
	}


	/**
	 * Return the constant maximum installable timeout in microseconds
	 *
	 * The return value is also the maximum delay to call 'timeout_age_us'
	 * for a timeout after its installation.
	 */
	inline time_t timeout_max_us()
	{
		return arch_call_64((Call_arg)Call_id::TIMEOUT_MAX_US);
	}


	/**
	 * Stops the calling thread, which becomes inactive until being activated
	 * again, e.g., by the restart_thread syscall.
	 */
	inline void thread_stop()
	{
		call(Call_id::THREAD_STOP);
	}

	enum class Thread_restart_result : Call_ret {
		RESTARTED, ALREADY_ACTIVE, INVALID };

	/**
	 * Activate a thread
	 *
	 * \param thread_id  capability ID of the targeted thread
	 *
	 * \return  wether the thread was inactive beforehand
	 */
	inline Thread_restart_result thread_restart(capid_t const thread_id)
	{
		switch (call(Call_id::THREAD_RESTART, thread_id)) {
		case (Call_ret)Thread_restart_result::RESTARTED:
			return Thread_restart_result::RESTARTED;
		case (Call_ret)Thread_restart_result::ALREADY_ACTIVE:
			return Thread_restart_result::ALREADY_ACTIVE;
		default: break;
		};
		return Thread_restart_result::INVALID;
	}


	/**
	 * Yield the calling thread's current cpu time to others.
	 * It strongly depends on the current CPU's schedule what actually
	 * will happen, and if the calling thread will be interrupted at all.
	 */
	inline void thread_yield()
	{
		call(Call_id::THREAD_YIELD);
	}

	/**
	 * Enforce coherent view on memory region, on architectures with
	 * split caches (instruction and data)
	 *
	 * \param base  base of the region within the active component
	 * \param size  size of the region
	 */
	inline void cache_coherent_region(addr_t const base, size_t const size)
	{
		call(Call_id::CACHE_COHERENT, base, size);
	}


	/**
	 * Clean and invalidate cache lines of the given memory region
	 *
	 * \param base  base of the region within the active component
	 * \param size  size of the region
	 */
	inline void cache_clean_invalidate_data_region(addr_t const base,
	                                        size_t const size)
	{
		call(Call_id::CACHE_CLEAN_INV, base, size);
	}


	/**
	 * Invalidate cache lines of the given memory region
	 *
	 * \param base  base of the region within the active component
	 * \param size  size of the region
	 */
	inline void cache_invalidate_data_region(addr_t const base,
	                                  size_t const size)
	{
		call(Call_id::CACHE_INV, base, size);
	}


	/**
	 * Return size of a cache line
	 */
	inline size_t cache_line_size()
	{
		return (size_t)call(Call_id::CACHE_SIZE);
	}


	enum class Rpc_result : Call_ret { OK, OUT_OF_CAPS };

	/**
	 * Call another thread and wait for the answer
	 *
	 * \param thread_id  capability ID of targeted thread
	 * \param rcv_caps   number of capabilities the caller expects to receive
	 *
	 * \return Ok or Out_of_caps when not enough capabilities were left
	 */
	inline Rpc_result rpc_call(capid_t const thread_id, unsigned rcv_caps)
	{
		return (call(Call_id::RPC_CALL, thread_id, rcv_caps) ==
		        (Call_ret)Rpc_result::OK)
			 ? Rpc_result::OK : Rpc_result::OUT_OF_CAPS;
	}


	/**
	 * Wait for remote-procedure-calls of other threads
	 *
	 * \param rcv_caps number of capabilities willing to accept
	 *
	 * \return Ok or Out_of_caps when not enough capabilities were left
	 */
	inline Rpc_result rpc_wait(unsigned rcv_caps)
	{
		return (call(Call_id::RPC_WAIT, rcv_caps) == (Call_ret)Rpc_result::OK)
			? Rpc_result::OK : Rpc_result::OUT_OF_CAPS;
	}


	/**
	 * Reply to previously received RPC
	 */
	inline void rpc_reply()
	{
		call(Call_id::RPC_REPLY);
	}

	/**
	 * Reply to previously received request message
	 *
	 * \param rcv_caps number of capabilities to accept with the next RPC
	 *
	 * \return Ok or Out_of_caps when not enough capabilities were left
	 */
	inline Rpc_result rpc_reply_and_wait(unsigned rcv_caps)
	{
		return (call(Call_id::RPC_REPLY_AND_WAIT, rcv_caps) ==
		        (Call_ret)Rpc_result::OK) ? Rpc_result::OK
		                                  : Rpc_result::OUT_OF_CAPS;
	}

	/**
	 * Print a character c to the kernel's debug message facility, e.g. serial line
	 */
	inline void print_char(char const c)
	{
		call(Call_id::PRINT, c);
	}


	enum class Signal_result : Call_ret { OK, INVALID };

	/**
	 * Await any context of a receiver and optionally ack a context before
	 *
	 * \param receiver_id  capability ID of the targeted signal receiver
	 *
	 * \retval  0  suceeded
	 * \retval -1  failed
	 *
	 * If this call returns 0, an instance of 'Signal::Data' is located at the
	 * base of the caller's UTCB. Every occurence of a signal is provided
	 * through this function until it gets delivered through this function or
	 * context respectively receiver get destructed. If multiple threads
	 * listen at the same receiver, and/or multiple contexts of the receiver
	 * trigger simultanously, there is no assertion about wich thread
	 * receives, and from wich context. A context that delivered once doesn't
	 * deliver again unless its last delivery has been acknowledged via
	 * ack_signal.
	 */
	inline Signal_result signal_wait(capid_t const receiver_id)
	{
		return (call(Call_id::SIG_WAIT, receiver_id) ==
		        (Call_ret)Signal_result::OK)
			? Signal_result::OK : Signal_result::INVALID;
	}


	/**
	 * Check for any pending signal of a context of a receiver the calling
	 * thread relates to
	 *
	 * \param receiver_id  capability ID of the targeted signal receiver
	 *
	 * \retval  0  suceeded
	 * \retval -1  failed
	 *
	 * If this call returns 0, an instance of 'Signal::Data' is located at the
	 * base of the callers UTCB.
	 */
	inline Signal_result signal_pending(capid_t const receiver_id)
	{
		return (call(Call_id::SIG_PENDING, receiver_id) ==
		        (Call_ret)Signal_result::OK)
			? Signal_result::OK : Signal_result::INVALID;
	}


	/**
	 * Trigger a specific signal context
	 *
	 * \param context  capability ID of the targeted signal context
	 * \param num      how often the context shall be triggered by this call
	 */
	inline void signal_submit(capid_t const context, unsigned const num)
	{
		call(Call_id::SIG_SUBMIT, context, num);
	}


	/**
	 * Acknowledge the processing of the last delivery of a signal context
	 *
	 * \param context  capability ID of the targeted signal context
	 */
	inline void signal_ack(capid_t const context)
	{
		call(Call_id::SIG_ACK, context);
	}


	/**
	 * Halt processing of a signal context synchronously
	 *
	 * \param context  capability ID of the targeted signal context
	 */
	inline void signal_kill(capid_t const context)
	{
		call(Call_id::SIG_KILL, context);
	}

	/**
	 * Acknowledge reception of a capability
	 *
	 * \param cap  capability ID to acknowledge
	 */
	inline void cap_ack(capid_t const cap)
	{
		call(Call_id::CAP_ACK, cap);
	}

	/**
	 * Delete a capability ID
	 *
	 * \param cap  capability ID to delete
	 */
	inline void cap_delete(capid_t const cap)
	{
		call(Call_id::CAP_DESTROY, cap);
	}


	/**
	 * Execute a virtual-machine (again)
	 *
	 * \param vcpu  capability of vcpu kernel object
	 */
	inline void vcpu_run(capid_t const cap)
	{
		call(Call_id::VCPU_RUN, cap);
	}


	/**
	 * Stop execution of a virtual-machine
	 *
	 * \param vcpu  capability of vcpu kernel object
	 */
	inline void vcpu_pause(capid_t const cap)
	{
		call(Call_id::VCPU_PAUSE, cap);
	}
}

#endif /* _INCLUDE__KERNEL__INTERFACE_H_ */
