/*
 * \brief  Interface between kernel and userland
 * \author Martin stein
 * \date   2011-11-30
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _KERNEL__INTERFACE_H_
#define _KERNEL__INTERFACE_H_

/* base-hw includes */
#include <kernel/interface_support.h>

namespace Genode
{
	class Native_utcb;
	class Platform_pd;
	class Tlb;
}

namespace Kernel
{
	typedef Genode::Tlb          Tlb;
	typedef Genode::addr_t       addr_t;
	typedef Genode::size_t       size_t;
	typedef Genode::Platform_pd  Platform_pd;
	typedef Genode::Native_utcb  Native_utcb;

	/**
	 * Kernel names of the kernel calls
	 */
	constexpr Call_arg call_id_pause_current_thread() { return 0; }
	constexpr Call_arg call_id_resume_thread()        { return 1; }
	constexpr Call_arg call_id_yield_thread()         { return 2; }
	constexpr Call_arg call_id_send_request_msg()     { return 3; }
	constexpr Call_arg call_id_send_reply_msg()       { return 4; }
	constexpr Call_arg call_id_await_request_msg()    { return 5; }
	constexpr Call_arg call_id_kill_signal_context()  { return 6; }
	constexpr Call_arg call_id_submit_signal()        { return 7; }
	constexpr Call_arg call_id_await_signal()         { return 8; }
	constexpr Call_arg call_id_signal_pending()       { return 9; }
	constexpr Call_arg call_id_ack_signal()           { return 10; }
	constexpr Call_arg call_id_print_char()           { return 11; }


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
	 * Pause execution of calling thread
	 */
	inline void pause_current_thread()
	{
		call(call_id_pause_current_thread());
	}


	/**
	 * Let an already started thread participate in CPU scheduling
	 *
	 * \param thread_id  kernel name of the targeted thread
	 *
	 * \retval  0  succeeded and thread was paused beforehand
	 * \retval  1  succeeded and thread was active beforehand
	 * \retval -1  failed
	 *
	 * If the targeted thread blocks for any event except a 'start_thread'
	 * call this call cancels the blocking.
	 */
	inline int resume_thread(unsigned const thread_id)
	{
		return call(call_id_resume_thread(), thread_id);
	}


	/**
	 * Let the current thread give up its remaining timeslice
	 *
	 * \param thread_id  kernel name of the benefited thread
	 *
	 * If thread_id is valid the call will resume the targeted thread
	 * additionally.
	 */
	inline void yield_thread(unsigned const thread_id)
	{
		call(call_id_yield_thread(), thread_id);
	}


	/**
	 * Send request message and await receipt of corresponding reply message
	 *
	 * \param thread_id  kernel name of targeted thread
	 *
	 * \retval  0  succeeded
	 * \retval -1  failed
	 *
	 * If the call returns successful, the received message is located at the
	 * base of the callers userland thread-context.
	 */
	inline int send_request_msg(unsigned const thread_id)
	{
		return call(call_id_send_request_msg(), thread_id);
	}


	/**
	 * Await receipt of request message
	 *
	 * \retval  0  succeeded
	 * \retval -1  failed
	 *
	 * If the call returns successful, the received message is located at the
	 * base of the callers userland thread-context.
	 */
	inline int await_request_msg()
	{
		return call(call_id_await_request_msg());
	}


	/**
	 * Reply to lastly received request message
	 *
	 * \param await_request_msg  wether the call shall await a request message
	 *
	 * \retval  0  await_request_msg == 0 or request-message receipt succeeded
	 * \retval -1  await_request_msg == 1 and request-message receipt failed
	 *
	 * If the call returns successful and await_request_msg == 1, the received
	 * message is located at the base of the callers userland thread-context.
	 */
	inline int send_reply_msg(bool const await_request_msg)
	{
		return call(call_id_send_reply_msg(), await_request_msg);
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
	 * \param receiver_id  kernel name of the targeted signal receiver
	 * \param context_id   kernel name of a context that shall be acknowledged
	 *
	 * \retval  0  suceeded
	 * \retval -1  failed
	 *
	 * If context is set to 0, the call doesn't acknowledge any context.
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
	inline int await_signal(unsigned const receiver_id,
	                        unsigned const context_id)
	{
		return call(call_id_await_signal(), receiver_id, context_id);
	}


	/**
	 * Return wether any context of a receiver is pending
	 *
	 * \param receiver  kernel name of the targeted signal receiver
	 *
	 * \retval 0  none of the contexts is pending or the receiver doesn't exist
	 * \retval 1  a context of the signal receiver is pending
	 */
	inline bool signal_pending(unsigned const receiver)
	{
		return call(call_id_signal_pending(), receiver);
	}


	/**
	 * Trigger a specific signal context
	 *
	 * \param context  kernel name of the targeted signal context
	 * \param num      how often the context shall be triggered by this call
	 *
	 * \retval  0  suceeded
	 * \retval -1  failed
	 */
	inline int submit_signal(unsigned const context, unsigned const num)
	{
		return call(call_id_submit_signal(), context, num);
	}


	/**
	 * Acknowledge the processing of the last delivery of a signal context
	 *
	 * \param context  kernel name of the targeted signal context
	 */
	inline void ack_signal(unsigned const context)
	{
		call(call_id_ack_signal(), context);
	}


	/**
	 * Halt processing of a signal context synchronously
	 *
	 * \param context  kernel name of the targeted signal context
	 *
	 * \retval  0  suceeded
	 * \retval -1  failed
	 */
	inline int kill_signal_context(unsigned const context)
	{
		return call(call_id_kill_signal_context(), context);
	}
}

#endif /* _KERNEL__INTERFACE_H_ */
