/*
 * \brief  Interface between kernel and userland
 * \author Martin stein
 * \date   2011-11-30
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__KERNEL__INTERFACE_H_
#define _INCLUDE__KERNEL__INTERFACE_H_

/* base-hw includes */
#include <kernel/types.h>
#include <kernel/interface_support.h>

namespace Kernel
{
	/**
	 * Kernel names of the kernel calls
	 */
	constexpr Call_arg call_id_stop_thread()              { return  0; }
	constexpr Call_arg call_id_restart_thread()           { return  1; }
	constexpr Call_arg call_id_yield_thread()             { return  2; }
	constexpr Call_arg call_id_send_request_msg()         { return  3; }
	constexpr Call_arg call_id_send_reply_msg()           { return  4; }
	constexpr Call_arg call_id_await_request_msg()        { return  5; }
	constexpr Call_arg call_id_kill_signal_context()      { return  6; }
	constexpr Call_arg call_id_submit_signal()            { return  7; }
	constexpr Call_arg call_id_await_signal()             { return  8; }
	constexpr Call_arg call_id_cancel_next_await_signal() { return  9; }
	constexpr Call_arg call_id_ack_signal()               { return 10; }
	constexpr Call_arg call_id_print_char()               { return 11; }
	constexpr Call_arg call_id_update_data_region()       { return 12; }
	constexpr Call_arg call_id_update_instr_region()      { return 13; }
	constexpr Call_arg call_id_ack_cap()                  { return 14; }
	constexpr Call_arg call_id_delete_cap()               { return 15; }
	constexpr Call_arg call_id_timeout()                  { return 16; }
	constexpr Call_arg call_id_timeout_age_us()           { return 17; }
	constexpr Call_arg call_id_timeout_max_us()           { return 18; }


	/*****************************************************************
	 ** Kernel call with 1 to 6 arguments                           **
	 **                                                             **
	 ** These functions must not be inline to ensure that objects,  **
	 ** wich are referenced by arguments, are tagged as "used" even **
	 ** though only the pointer gets handled in here.               **
	 *****************************************************************/

	Call_ret call(Call_arg arg_0);

	Call_ret call(Call_arg arg_0,
	              Call_arg arg_1);

	Call_ret call(Call_arg arg_0,
	              Call_arg arg_1,
	              Call_arg arg_2);

	Call_ret call(Call_arg arg_0,
	              Call_arg arg_1,
	              Call_arg arg_2,
	              Call_arg arg_3);

	Call_ret call(Call_arg arg_0,
	              Call_arg arg_1,
	              Call_arg arg_2,
	              Call_arg arg_3,
	              Call_arg arg_4);

	Call_ret call(Call_arg arg_0,
	              Call_arg arg_1,
	              Call_arg arg_2,
	              Call_arg arg_3,
	              Call_arg arg_4,
	              Call_arg arg_5);


	/**
	 * Install timeout for calling thread
	 *
	 * \param  duration_us  timeout duration in microseconds
	 * \param  sigid        local name of signal context to trigger
	 *
	 * This call always overwrites the last timeout installed by the thread
	 * if any.
	 */
	inline int timeout(time_t const duration_us, capid_t const sigid)
	{
		return call(call_id_timeout(), duration_us, sigid);
	}


	/**
	 * Return time in microseconds since the caller installed its last timeout
	 *
	 * Must not be called if the installation is older than 'timeout_max_us'.
	 */
	inline time_t timeout_age_us()
	{
		return call(call_id_timeout_age_us());
	}


	/**
	 * Return the constant maximum installable timeout in microseconds
	 *
	 * The return value is also the maximum delay to call 'timeout_age_us'
	 * for a timeout after its installation.
	 */
	inline time_t timeout_max_us()
	{
		return call(call_id_timeout_max_us());
	}


	/**
	 * Wait for a user event signaled by a 'restart_thread' syscall
	 *
	 * The stop syscall always targets the calling thread that, therefore must
	 * be in the 'active' thread state. The thread then switches to the
	 * 'stopped' thread state in wich it waits for a restart. The restart
	 * syscall can only be used on a thread in the 'stopped' or the 'active'
	 * thread state. The thread then switches back to the 'active' thread
	 * state and the syscall returns whether the thread was stopped. Both
	 * syscalls are not core-restricted. In contrast to the 'stop' syscall,
	 * 'restart' may target any thread in the same PD as the caller. Thread
	 * state and UTCB content of a thread don't change while in the 'stopped'
	 * state. The 'stop/restart' feature is used when an active thread wants
	 * to wait for an event that is not known to the kernel. Actually, the
	 * syscalls are used when waiting for locks and when doing infinite
	 * waiting on thread exit.
	 */
	inline void stop_thread()
	{
		call(call_id_stop_thread());
	}


	/**
	 * End blocking of a stopped thread
	 *
	 * \param thread_id  capability id of the targeted thread
	 *
	 * \return  wether the thread was stopped beforehand
	 *
	 * For details see the 'stop_thread' syscall.
	 */
	inline bool restart_thread(capid_t const thread_id)
	{
		return call(call_id_restart_thread(), thread_id);
	}


	/**
	 * Yield the callers remaining CPU time for this super period
	 *
	 * Does its best that the caller is scheduled as few as possible in the
	 * current scheduling super-period without touching the thread or pause
	 * state of the thread. In the next superperiod, however, the thread is
	 * scheduled 'normal' again. The syscall is not core-restricted and always
	 * targets the caller. It is actually used in locks to help another thread
	 * reach a desired point in execution by releasing pressure from the CPU.
	 */
	inline void yield_thread()
	{
		call(call_id_yield_thread());
	}

	/**
	 * Globally apply writes to a data region in the current domain
	 *
	 * \param base  base of the region within the current domain
	 * \param size  size of the region
	 */
	inline void update_data_region(addr_t const base, size_t const size)
	{
		call(call_id_update_data_region(), (Call_arg)base, (Call_arg)size);
	}

	/**
	 * Globally apply writes to an instruction region in the current domain
	 *
	 * \param base  base of the region within the current domain
	 * \param size  size of the region
	 */
	inline void update_instr_region(addr_t const base, size_t const size)
	{
		call(call_id_update_instr_region(), (Call_arg)base, (Call_arg)size);
	}

	/**
	 * Send request message and await receipt of corresponding reply message
	 *
	 * \param thread_id  capability id of targeted thread
	 *
	 * \retval  0  succeeded
	 * \retval -1  failed
	 * \retval -2  failed due to out-of-memory for capability reception
	 *
	 * If the call returns successful, the received message is located at the
	 * base of the callers userland thread-context.
	 */
	inline int send_request_msg(capid_t const thread_id, unsigned rcv_caps)
	{
		return call(call_id_send_request_msg(), thread_id, rcv_caps);
	}


	/**
	 * Await receipt of request message
	 *
	 * \param rcv_caps number of capabilities willing to accept
	 *
	 * \retval  0  succeeded
	 * \retval -1  canceled
	 * \retval -2  failed due to out-of-memory for capability reception
	 *
	 * If the call returns successful, the received message is located at the
	 * base of the callers userland thread-context.
	 */
	inline int await_request_msg(unsigned rcv_caps)
	{
		return call(call_id_await_request_msg(), rcv_caps);
	}


	/**
	 * Reply to lastly received request message
	 *
	 * \param rcv_caps number of capabilities to accept when awaiting again
	 * \param await_request_msg  wether the call shall await a request message
	 *
	 * \retval  0  await_request_msg == 0 or request-message receipt succeeded
	 * \retval -1  await_request_msg == 1 and request-message receipt failed
	 *
	 * If the call returns successful and await_request_msg == 1, the received
	 * message is located at the base of the callers userland thread-context.
	 */
	inline int send_reply_msg(unsigned rcv_caps, bool const await_request_msg)
	{
		return call(call_id_send_reply_msg(), rcv_caps, await_request_msg);
	}


	/**
	 * Print a char c to the kernels serial ouput
	 *
	 * If c is set to 0 the kernel prints a table of all threads and their
	 * current activities to the serial output.
	 */
	inline void print_char(char const c)
	{
		call(call_id_print_char(), c);
	}


	/**
	 * Await any context of a receiver and optionally ack a context before
	 *
	 * \param receiver_id  capability id of the targeted signal receiver
	 *
	 * \retval  0  suceeded
	 * \retval -1  failed
	 *
	 * If this call returns 0, an instance of 'Signal::Data' is located at the
	 * base of the callers UTCB. Every occurence of a signal is provided
	 * through this function until it gets delivered through this function or
	 * context respectively receiver get destructed. If multiple threads
	 * listen at the same receiver, and/or multiple contexts of the receiver
	 * trigger simultanously, there is no assertion about wich thread
	 * receives, and from wich context. A context that delivered once doesn't
	 * deliver again unless its last delivery has been acknowledged via
	 * ack_signal.
	 */
	inline int await_signal(capid_t const receiver_id)
	{
		return call(call_id_await_signal(), receiver_id);
	}

	/**
	 * Request to cancel the next signal blocking of a local thread
	 *
	 * \param thread_id  capability id of the targeted thread
	 *
	 * Does not block. Targeted thread must be in the same PD as the caller.
	 * If the targeted thread is in a signal blocking, cancels the blocking
	 * directly. Otherwise, stores the request and avoids the next signal
	 * blocking of the targeted thread as if it was immediately cancelled.
	 * If the target thread already holds a request, further ones get ignored.
	 */
	inline void cancel_next_await_signal(capid_t const thread_id)
	{
		call(call_id_cancel_next_await_signal(), thread_id);
	}


	/**
	 * Trigger a specific signal context
	 *
	 * \param context  capability id of the targeted signal context
	 * \param num      how often the context shall be triggered by this call
	 *
	 * \retval  0  suceeded
	 * \retval -1  failed
	 */
	inline int submit_signal(capid_t const context, unsigned const num)
	{
		return call(call_id_submit_signal(), context, num);
	}


	/**
	 * Acknowledge the processing of the last delivery of a signal context
	 *
	 * \param context  capability id of the targeted signal context
	 */
	inline void ack_signal(capid_t const context)
	{
		call(call_id_ack_signal(), context);
	}


	/**
	 * Halt processing of a signal context synchronously
	 *
	 * \param context  capability id of the targeted signal context
	 *
	 * \retval  0  suceeded
	 * \retval -1  failed
	 */
	inline int kill_signal_context(capid_t const context)
	{
		return call(call_id_kill_signal_context(), context);
	}

	/**
	 * Acknowledge reception of a capability
	 *
	 * \param cap  capability id to acknowledge
	 */
	inline void ack_cap(capid_t const cap)
	{
		call(call_id_ack_cap(), cap);
	}

	/**
	 * Delete a capability id
	 *
	 * \param cap  capability id to delete
	 */
	inline void delete_cap(capid_t const cap)
	{
		call(call_id_delete_cap(), cap);
	}
}

#endif /* _INCLUDE__KERNEL__INTERFACE_H_ */
