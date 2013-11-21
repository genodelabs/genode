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

/* Genode includes */
#include <kernel/interface_support.h>

namespace Genode
{
	class Native_utcb;
	class Platform_pd;
	class Tlb;
}

namespace Kernel
{
	typedef Genode::Tlb             Tlb;
	typedef Genode::addr_t          addr_t;
	typedef Genode::size_t          size_t;
	typedef Genode::Platform_pd     Platform_pd;
	typedef Genode::Native_utcb     Native_utcb;

	/**
	 * Kernel names of all kernel calls
	 */
	struct Call_id
	{
		enum {
			NEW_THREAD           = 0,
			KILL_THREAD          = 1,
			START_THREAD         = 2,
			PAUSE_THREAD         = 3,
			RESUME_THREAD        = 4,
			YIELD_THREAD         = 5,
			ACCESS_THREAD_REGS   = 6,
			ROUTE_THREAD_EVENT   = 7,
			UPDATE_PD            = 8,
			UPDATE_REGION        = 9,
			NEW_PD               = 10,
			KILL_PD              = 11,
			SEND_REQUEST_MSG     = 12,
			SEND_REPLY_MSG       = 13,
			AWAIT_REQUEST_MSG    = 14,
			NEW_SIGNAL_RECEIVER  = 15,
			NEW_SIGNAL_CONTEXT   = 16,
			KILL_SIGNAL_CONTEXT  = 17,
			KILL_SIGNAL_RECEIVER = 18,
			SUBMIT_SIGNAL        = 19,
			AWAIT_SIGNAL         = 20,
			SIGNAL_PENDING       = 21,
			ACK_SIGNAL           = 22,
			NEW_VM               = 23,
			RUN_VM               = 24,
			PAUSE_VM             = 25,
			PRINT_CHAR           = 26,
		};
	};


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
	 * Virtual range of the mode transition region in every PD
	 */
	addr_t mode_transition_virt_base();
	size_t mode_transition_size();

	/**
	 * Get sizes of the kernel objects
	 */
	size_t thread_size();
	size_t pd_size();
	size_t signal_context_size();
	size_t signal_receiver_size();
	size_t vm_size();

	/**
	 * Get alignment constraints of the kernel objects
	 */
	unsigned kernel_pd_alignm_log2();


	/**
	 * Create a protection domain
	 *
	 * \param p   appropriate memory donation for the kernel object
	 * \param pd  core local Platform_pd object
	 *
	 * \retval >0  kernel name of the new protection domain
	 * \retval  0  failed
	 *
	 * Restricted to core threads. Regaining of the supplied memory is not
	 * supported by now.
	 */
	inline unsigned new_pd(void * const dst, Platform_pd * const pd)
	{
		return call(Call_id::NEW_PD, (Call_arg)dst, (Call_arg)pd);
	}


	/**
	 * Destruct a protection domain
	 *
	 * \param pd_id  kernel name of the targeted protection domain
	 *
	 * \retval  0  succeeded
	 * \retval -1  failed
	 */
	inline int kill_pd(unsigned const pd_id)
	{
		return call(Call_id::KILL_PD, pd_id);
	}


	/**
	 * Propagate changes in PD configuration
	 *
	 * \param pd_id  ID of the PD that has been configured
	 *
	 * It might be, that the kernel and/or the hardware caches parts of PD
	 * configurations such as virtual address translations. This function
	 * ensures that the current configuration of the targeted PD gets fully
	 * applied from the moment it returns to the userland. This function is
	 * inappropriate in case that a PD wants to change its own configuration.
	 * There's no need for this function after a configuration change that
	 * can't affect the kernel- and/or hardware-caches.
	 *
	 * Restricted to core threads.
	 */
	inline void update_pd(unsigned const pd_id)
	{
		call(Call_id::UPDATE_PD, pd_id);
	}


	/**
	 * Propagate memory-updates within a given virtual region
	 *
	 * \param base  virtual base of the region
	 * \param size  size of the region
	 *
	 * If one updates a memory region and must ensure that the update
	 * gets visible directly to other address spaces, this function does
	 * the job.
	 *
	 * Restricted to core threads.
	 */
	inline void update_region(addr_t const base, size_t const size)
	{
		call(Call_id::UPDATE_REGION, (Call_arg)base, (Call_arg)size);
	}


	/**
	 * Create kernel object that acts as thread that isn't executed initially
	 *
	 * \param p         memory donation for the new kernel thread object
	 * \param priority  scheduling priority of the new thread
	 * \param label     debugging label of the new thread
	 *
	 * \retval >0  kernel name of the new thread
	 * \retval  0  failed
	 *
	 * Restricted to core threads.
	 */
	inline int new_thread(void * const p, unsigned const priority,
	                      char const * const label)
	{
		return call((Call_arg)Call_id::NEW_THREAD, (Call_arg)p, (Call_arg)priority,
		            (Call_arg)label);
	}


	/**
	 * Destruct kernel thread-object
	 *
	 * \param thread_id  kernel name of the targeted thread
	 *
	 * Restricted to core threads.
	 */
	inline void kill_thread(unsigned const thread_id)
	{
		call(Call_id::KILL_THREAD, thread_id);
	}


	/**
	 * Start executing a thread
	 *
	 * \param thread_id  kernel name of targeted thread
	 * \param cpu_id     kernel name of targeted processor
	 * \param pd_id      kernel name of targeted protection domain
	 * \param utcb       core local pointer to userland thread-context
	 *
	 * Restricted to core threads.
	 */
	inline Tlb * start_thread(unsigned const thread_id, unsigned const cpu_id,
	                          unsigned const pd_id, Native_utcb * const utcb)
	{
		return (Tlb *)call(Call_id::START_THREAD, thread_id, cpu_id, pd_id,
		                   (Call_arg)utcb);
	}


	/**
	 * Prevent thread from participating in CPU scheduling
	 *
	 * \param thread_id  kernel name of the targeted thread or 0
	 *
	 * \retval  0  succeeded
	 * \retval -1  the targeted thread does not exist or is still active
	 *
	 * If thread_id is set to 0 the caller targets itself. If the caller
	 * doesn't target itself, the call is restricted to core threads.
	 */
	inline int pause_thread(unsigned const thread_id)
	{
		return call(Call_id::PAUSE_THREAD, thread_id);
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
		return call(Call_id::RESUME_THREAD, thread_id);
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
		call(Call_id::YIELD_THREAD, thread_id);
	}


	/**
	 * Set or unset the handler of an event a kernel thread-object triggers
	 *
	 * \param thread_id          kernel name of the targeted thread
	 * \param event_id           kernel name of the targeted thread event
	 * \param signal_context_id  kernel name of the handlers signal context
	 *
	 * Restricted to core threads.
	 */
	inline int route_thread_event(unsigned const thread_id,
	                              unsigned const event_id,
	                              unsigned const signal_context_id)
	{
		return call(Call_id::ROUTE_THREAD_EVENT, thread_id,
		            event_id, signal_context_id);
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
		return call(Call_id::SEND_REQUEST_MSG, thread_id);
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
		return call(Call_id::AWAIT_REQUEST_MSG);
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
		return call(Call_id::SEND_REPLY_MSG, await_request_msg);
	}


	/**
	 * Print a char 'c' to the kernels serial ouput
	 */
	inline void print_char(char const c)
	{
		call(Call_id::PRINT_CHAR, c);
	}


	/**
	 * Access plain member variables of a kernel thread-object
	 *
	 * \param thread_id     kernel name of the targeted thread
	 * \param reads         amount of read operations
	 * \param writes        amount of write operations
	 * \param read_values   base of value buffer for read operations
	 * \param write_values  base of value buffer for write operations
	 *
	 * \retval  0  all operations done
	 * \retval >0  amount of undone operations
	 * \retval -1  failed to start processing operations
	 *
	 * Restricted to core threads. Operations are processed in order of the
	 * appearance of the register names in the callers UTCB. If reads = 0,
	 * read_values is of no relevance. If writes = 0, write_values is of no
	 * relevance.
	 *
	 * Expected structure at the callers UTCB base:
	 *
	 *                    0 * sizeof(addr_t): read register name #1
	 *                  ...                   ...
	 *          (reads - 1) * sizeof(addr_t): read register name #reads
	 *          (reads - 0) * sizeof(addr_t): write register name #1
	 *                  ...                   ...
	 * (reads + writes - 1) * sizeof(addr_t): write register name #writes
	 *
	 * Expected structure at write_values:
	 *
	 *                    0 * sizeof(addr_t): write value #1
	 *                  ...                   ...
	 *         (writes - 1) * sizeof(addr_t): write value #writes
	 */
	inline int access_thread_regs(unsigned const thread_id,
	                              unsigned const reads,
	                              unsigned const writes,
	                              addr_t * const read_values,
	                              addr_t * const write_values)
	{
		return call(Call_id::ACCESS_THREAD_REGS, thread_id, reads, writes,
		            (Call_arg)read_values, (Call_arg)write_values);
	}


	/**
	 * Create a kernel object that acts as a signal receiver
	 *
	 * \param p  memory donation for the kernel signal-receiver object
	 *
	 * \retval >0  kernel name of the new signal receiver
	 * \retval  0  failed
	 *
	 * Restricted to core threads.
	 */
	inline unsigned new_signal_receiver(addr_t const p)
	{
		return call(Call_id::NEW_SIGNAL_RECEIVER, p);
	}


	/**
	 * Create kernel object that acts as a signal context and assign it
	 *
	 * \param p         memory donation for the kernel signal-context object
	 * \param receiver  kernel name of targeted signal receiver
	 * \param imprint   user label of the signal context
	 *
	 * \retval >0  kernel name of the new signal context
	 * \retval  0  failed
	 *
	 * Restricted to core threads.
	 */
	inline unsigned new_signal_context(addr_t const   p,
	                                   unsigned const receiver,
	                                   unsigned const imprint)
	{
		return call(Call_id::NEW_SIGNAL_CONTEXT, p, receiver, imprint);
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
		return call(Call_id::AWAIT_SIGNAL, receiver_id, context_id);
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
		return call(Call_id::SIGNAL_PENDING, receiver);
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
		return call(Call_id::SUBMIT_SIGNAL, context, num);
	}


	/**
	 * Acknowledge the processing of the last delivery of a signal context
	 *
	 * \param context  kernel name of the targeted signal context
	 */
	inline void ack_signal(unsigned const context)
	{
		call(Call_id::ACK_SIGNAL, context);
	}


	/**
	 * Destruct a signal context
	 *
	 * \param context  kernel name of the targeted signal context
	 *
	 * \retval  0  suceeded
	 * \retval -1  failed
	 *
	 * Restricted to core threads.
	 */
	inline int kill_signal_context(unsigned const context)
	{
		return call(Call_id::KILL_SIGNAL_CONTEXT, context);
	}


	/**
	 * Destruct a signal receiver
	 *
	 * \param receiver  kernel name of the targeted signal receiver
	 *
	 * \retval  0  suceeded
	 * \retval -1  failed
	 *
	 * Restricted to core threads.
	 */
	inline int kill_signal_receiver(unsigned const receiver)
	{
		return call(Call_id::KILL_SIGNAL_RECEIVER, receiver);
	}


	/**
	 * Create a virtual machine that is stopped initially
	 *
	 * \param dst                memory donation for the kernel VM-object
	 * \param state              location of the CPU state of the VM
	 * \param signal_context_id  kernel name of the signal context for VM events
	 *
	 * \retval >0  kernel name of the new VM
	 * \retval  0  failed
	 *
	 * Restricted to core threads. Regaining of the supplied memory is not
	 * supported by now.
	 */
	inline int new_vm(void * const dst, void * const state,
	                  unsigned const signal_context_id)
	{
		return call(Call_id::NEW_VM, (Call_arg)dst, (Call_arg)state,
		            signal_context_id);
	}


	/**
	 * Execute a virtual-machine (again)
	 *
	 * \param vm_id  kernel name of the targeted VM
	 *
	 * Restricted to core threads.
	 */
	inline void run_vm(unsigned const vm_id)
	{
		call(Call_id::RUN_VM, vm_id);
	}


	/**
	 * Stop execution of a virtual-machine
	 *
	 * \param vm_id  kernel name of the targeted VM
	 *
	 * Restricted to core threads.
	 */
	inline void pause_vm(unsigned const vm_id)
	{
		call(Call_id::PAUSE_VM, vm_id);
	}
}

#endif /* _KERNEL__INTERFACE_H_ */

