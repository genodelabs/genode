/*
 * \brief  Parts of the kernel interface that are restricted to core
 * \author Martin stein
 * \date   2014-03-15
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _KERNEL__CORE_INTERFACE_H_
#define _KERNEL__CORE_INTERFACE_H_

/* base-hw includes */
#include <kernel/interface.h>

namespace Kernel
{
	class Pd;
	class Thread;
	class Signal_receiver;
	class Signal_context;
	class Vm;
	class User_irq;

	addr_t   mode_transition_base();
	size_t   mode_transition_size();
	size_t   thread_size();
	size_t   pd_size();
	unsigned pd_alignment_log2();
	size_t   signal_context_size();
	size_t   signal_receiver_size();

	/**
	 * Kernel names of the kernel calls
	 */
	constexpr Call_arg call_id_new_thread()             { return 14; }
	constexpr Call_arg call_id_delete_thread()          { return 15; }
	constexpr Call_arg call_id_start_thread()           { return 16; }
	constexpr Call_arg call_id_resume_thread()          { return 17; }
	constexpr Call_arg call_id_access_thread_regs()     { return 18; }
	constexpr Call_arg call_id_route_thread_event()     { return 19; }
	constexpr Call_arg call_id_update_pd()              { return 20; }
	constexpr Call_arg call_id_new_pd()                 { return 21; }
	constexpr Call_arg call_id_delete_pd()              { return 22; }
	constexpr Call_arg call_id_new_signal_receiver()    { return 23; }
	constexpr Call_arg call_id_new_signal_context()     { return 24; }
	constexpr Call_arg call_id_delete_signal_context()  { return 25; }
	constexpr Call_arg call_id_delete_signal_receiver() { return 26; }
	constexpr Call_arg call_id_new_vm()                 { return 27; }
	constexpr Call_arg call_id_run_vm()                 { return 28; }
	constexpr Call_arg call_id_pause_vm()               { return 29; }
	constexpr Call_arg call_id_pause_thread()           { return 30; }
	constexpr Call_arg call_id_delete_vm()              { return 31; }
	constexpr Call_arg call_id_new_irq()                { return 32; }
	constexpr Call_arg call_id_delete_irq()             { return 33; }
	constexpr Call_arg call_id_thread_quota()           { return 34; }
	constexpr Call_arg call_id_ack_irq()                { return 35; }

	/**
	 * Create a domain
	 *
	 * \param dst  appropriate memory donation for the kernel object
	 * \param pd   core local Platform_pd object
	 *
	 * \retval 0 when successful, otherwise !=0
	 */
	inline int long new_pd(void * const dst, Platform_pd * const pd)
	{
		return call(call_id_new_pd(), (Call_arg)dst, (Call_arg)pd);
	}


	/**
	 * Destruct a domain
	 *
	 * \param pd  pointer to pd kernel object
	 */
	inline void delete_pd(Pd * const pd)
	{
		call(call_id_delete_pd(), (Call_arg)pd);
	}


	/**
	 * Update locally effective domain configuration to in-memory state
	 *
	 * \param pd  pointer to pd kernel object
	 *
	 * Kernel and/or hardware may cache parts of a domain configuration. This
	 * function ensures that the in-memory state of the targeted domain gets
	 * CPU-locally effective.
	 */
	inline void update_pd(Pd * const pd)
	{
		call(call_id_update_pd(), (Call_arg)pd);
	}


	/**
	 * Create a thread
	 *
	 * \param p         memory donation for the new kernel thread object
	 * \param priority  scheduling priority of the new thread
	 * \param quota     CPU quota of the new thread
	 * \param label     debugging label of the new thread
	 *
	 * \retval >0  kernel name of the new thread
	 * \retval  0  failed
	 */
	inline unsigned new_thread(void * const p, unsigned const priority,
	                           size_t const quota, char const * const label)
	{
		return call(call_id_new_thread(), (Call_arg)p, (Call_arg)priority,
		            (Call_arg)quota, (Call_arg)label);
	}


	/**
	 * Configure the CPU quota of a thread
	 *
	 * \param thread  kernel object of the targeted thread
	 * \param quota   new CPU quota value
	 */
	inline void thread_quota(Kernel::Thread * const thread, size_t const quota)
	{
		call(call_id_thread_quota(), (Call_arg)thread, (Call_arg)quota);
	}


	/**
	 * Pause execution of a specific thread
	 *
	 * \param thread  pointer to thread kernel object
	 */
	inline void pause_thread(Thread * const thread)
	{
		call(call_id_pause_thread(), (Call_arg)thread);
	}


	/**
	 * Destruct a thread
	 *
	 * \param thread  pointer to thread kernel object
	 */
	inline void delete_thread(Thread * const thread)
	{
		call(call_id_delete_thread(), (Call_arg)thread);
	}


	/**
	 * Start execution of a thread
	 *
	 * \param thread  pointer to thread kernel object
	 * \param cpu_id  kernel name of the targeted CPU
	 * \param pd      pointer to pd kernel object
	 * \param utcb    core local pointer to userland thread-context
	 *
	 * \retval   0  suceeded
	 * \retval !=0  failed
	 */
	inline int start_thread(Thread * const thread, unsigned const cpu_id,
	                        Pd * const pd, Native_utcb * const utcb)
	{
		return call(call_id_start_thread(), (Call_arg)thread, cpu_id,
		            (Call_arg)pd, (Call_arg)utcb);
	}


	/**
	 * Cancel blocking of a thread if possible
	 *
	 * \param thread  pointer to thread kernel object
	 *
	 * \return  wether thread was in a cancelable blocking beforehand
	 */
	inline bool resume_thread(Thread * const thread)
	{
		return call(call_id_resume_thread(), (Call_arg)thread);
	}


	/**
	 * Set or unset the handler of an event that can be triggered by a thread
	 *
	 * \param thread             pointer to thread kernel object
	 * \param event_id           kernel name of the targeted thread event
	 * \param signal_context_id  kernel name of the handlers signal context
	 *
	 * \retval  0  succeeded
	 * \retval -1  failed
	 */
	inline int route_thread_event(Thread * const thread,
	                              unsigned const event_id,
	                              unsigned const signal_context_id)
	{
		return call(call_id_route_thread_event(), (Call_arg)thread,
		            event_id, signal_context_id);
	}


	/**
	 * Access plain member variables of a kernel thread-object
	 *
	 * \param thread     pointer to thread kernel object
	 * \param reads      amount of read operations
	 * \param writes     amount of write operations
	 * \param values     base of the value buffer for all operations
	 *
	 * \return  amount of undone operations according to the execution order
	 *
	 * Operations are executed in order of the appearance of the register names
	 * in the callers UTCB. If reads = 0, read_values is of no relevance. If
	 * writes = 0, write_values is of no relevance.
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
	 * Expected structure at values:
	 *
	 *                    0 * sizeof(addr_t): read destination #1
	 *                  ...                   ...
	 *          (reads - 1) * sizeof(addr_t): read destination #reads
	 *          (reads - 0) * sizeof(addr_t): write value #1
	 *                  ...                   ...
	 * (reads + writes - 1) * sizeof(addr_t): write value #writes
	 */
	inline unsigned access_thread_regs(Thread * const thread,
	                                   unsigned const reads,
	                                   unsigned const writes,
	                                   addr_t * const values)
	{
		return call(call_id_access_thread_regs(), (Call_arg)thread,
		            reads, writes, (Call_arg)values);
	}


	/**
	 * Create a signal receiver
	 *
	 * \param p  memory donation for the kernel signal-receiver object
	 *
	 * \retval >0  kernel name of the new signal receiver
	 * \retval  0  failed
	 */
	inline unsigned new_signal_receiver(addr_t const p)
	{
		return call(call_id_new_signal_receiver(), p);
	}


	/**
	 * Create a signal context and assign it to a signal receiver
	 *
	 * \param p         memory donation for the kernel signal-context object
	 * \param receiver  pointer to signal receiver kernel object
	 * \param imprint   user label of the signal context
	 *
	 * \retval >0  kernel name of the new signal context
	 * \retval  0  failed
	 */
	inline unsigned new_signal_context(addr_t const   p,
	                                   Signal_receiver * const receiver,
	                                   unsigned const imprint)
	{
		return call(call_id_new_signal_context(), p,
		            (Call_arg)receiver, imprint);
	}


	/**
	 * Destruct a signal context
	 *
	 * \param context  pointer to signal context kernel object
	 */
	inline void delete_signal_context(Signal_context * const context)
	{
		call(call_id_delete_signal_context(), (Call_arg)context);
	}


	/**
	 * Destruct a signal receiver
	 *
	 * \param receiver  pointer to signal receiver kernel object
	 *
	 * \retval  0  suceeded
	 * \retval -1  failed
	 */
	inline void delete_signal_receiver(Signal_receiver * const receiver)
	{
		call(call_id_delete_signal_receiver(), (Call_arg)receiver);
	}


	/**
	 * Create a virtual machine that is stopped initially
	 *
	 * \param dst                memory donation for the VM object
	 * \param state              location of the CPU state of the VM
	 * \param signal_context_id  kernel name of the signal context for VM events
	 * \param table              guest-physical to host-physical translation
	 *                           table pointer
	 *
	 * \retval 0 when successful, otherwise !=0
	 *
	 * Regaining of the supplied memory is not supported by now.
	 */
	inline int new_vm(void * const dst, void * const state,
	                  unsigned const signal_context_id,
	                  void * const table)
	{
		return call(call_id_new_vm(), (Call_arg)dst, (Call_arg)state,
		            (Call_arg)table, signal_context_id);
	}


	/**
	 * Execute a virtual-machine (again)
	 *
	 * \param vm  pointer to vm kernel object
	 *
	 * \retval 0 when successful, otherwise !=0
	 */
	inline int run_vm(Vm * const vm)
	{
		return call(call_id_run_vm(), (Call_arg) vm);
	}


	/**
	 * Destruct a virtual-machine
	 *
	 * \param vm  pointer to vm kernel object
	 *
	 * \retval 0 when successful, otherwise !=0
	 */
	inline int delete_vm(Vm * const vm)
	{
		return call(call_id_delete_vm(), (Call_arg) vm);
	}


	/**
	 * Stop execution of a virtual-machine
	 *
	 * \param vm  pointer to vm kernel object
	 *
	 * \retval 0 when successful, otherwise !=0
	 */
	inline int pause_vm(Vm * const vm)
	{
		return call(call_id_pause_vm(), (Call_arg) vm);
	}

	/**
	 * Create an interrupt object
	 *
	 * \param p                 memory donation for the irq object
	 * \param irq_nr            interrupt number
	 * \param signal_context_id kernel name of the signal context
	 */
	inline int new_irq(addr_t const p, unsigned irq_nr,
	                    unsigned signal_context_id)
	{
		return call(call_id_new_irq(), (Call_arg) p, irq_nr, signal_context_id);
	}

	/**
	 * Acknowledge interrupt
	 *
	 * \param irq  pointer to interrupt kernel object
	 */
	inline void ack_irq(User_irq * const irq)
	{
		call(call_id_ack_irq(), (Call_arg) irq);
	}

	/**
	 * Destruct an interrupt object
	 *
	 * \param irq  pointer to interrupt kernel object
	 */
	inline void delete_irq(User_irq * const irq)
	{
		call(call_id_delete_irq(), (Call_arg) irq);
	}
}

#endif /* _KERNEL__CORE_INTERFACE_H_ */
