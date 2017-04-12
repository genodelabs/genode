/*
 * \brief  Parts of the kernel interface that are restricted to core
 * \author Martin stein
 * \date   2014-03-15
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__KERNEL__CORE_INTERFACE_H_
#define _CORE__KERNEL__CORE_INTERFACE_H_

/* base-internal includes */
#include <base/internal/native_utcb.h>

/* base-hw includes */
#include <kernel/interface.h>

namespace Genode { class Native_utcb; }

namespace Kernel
{
	class Pd;
	class Thread;
	class Signal_receiver;
	class Signal_context;
	class Vm;
	class User_irq;
	using Native_utcb = Genode::Native_utcb;

	/**
	 * Kernel names of the kernel calls
	 */
	constexpr Call_arg call_id_new_thread()             { return 100; }
	constexpr Call_arg call_id_delete_thread()          { return 101; }
	constexpr Call_arg call_id_start_thread()           { return 102; }
	constexpr Call_arg call_id_pause_thread()           { return 103; }
	constexpr Call_arg call_id_resume_thread()          { return 104; }
	constexpr Call_arg call_id_thread_pager()           { return 105; }
	constexpr Call_arg call_id_thread_quota()           { return 106; }
	constexpr Call_arg call_id_update_pd()              { return 107; }
	constexpr Call_arg call_id_new_pd()                 { return 108; }
	constexpr Call_arg call_id_delete_pd()              { return 109; }
	constexpr Call_arg call_id_new_signal_receiver()    { return 110; }
	constexpr Call_arg call_id_new_signal_context()     { return 111; }
	constexpr Call_arg call_id_delete_signal_context()  { return 112; }
	constexpr Call_arg call_id_delete_signal_receiver() { return 113; }
	constexpr Call_arg call_id_new_vm()                 { return 114; }
	constexpr Call_arg call_id_run_vm()                 { return 115; }
	constexpr Call_arg call_id_pause_vm()               { return 116; }
	constexpr Call_arg call_id_delete_vm()              { return 117; }
	constexpr Call_arg call_id_new_irq()                { return 118; }
	constexpr Call_arg call_id_delete_irq()             { return 119; }
	constexpr Call_arg call_id_ack_irq()                { return 120; }
	constexpr Call_arg call_id_new_obj()                { return 121; }
	constexpr Call_arg call_id_delete_obj()             { return 122; }
	constexpr Call_arg call_id_cancel_thread_blocking() { return 123; }

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
	 * Pause execution of a thread until 'resume_thread' is called on it
	 *
	 * \param thread  pointer to thread kernel object
	 *
	 * This doesn't affect the state of the thread (IPC, signalling, etc.) but
	 * merely wether the thread is allowed for scheduling or not. The pause
	 * state simply masks the thread state when it comes to scheduling. In
	 * contrast to the 'stopped' thread state, which is described in the
	 * documentation of the 'stop_thread/resume_thread' syscalls, the pause
	 * state doesn't freeze the thread state and the UTCB content of a thread.
	 * However, the register state of a thread doesn't change while paused.
	 * The 'pause' and 'resume' syscalls are both core-restricted and may
	 * target any thread. They are used as back end for the CPU session calls
	 * 'pause' and 'resume'. The 'pause/resume' feature is made for
	 * applications like the GDB monitor that transparently want to stop and
	 * continue the execution of a thread no matter what state the thread is
	 * in.
	 */
	inline void pause_thread(Thread * const thread)
	{
		call(call_id_pause_thread(), (Call_arg)thread);
	}


	/**
	 * End blocking of a paused thread
	 *
	 * \param thread  pointer to thread kernel object
	 */
	inline void resume_thread(Thread * const thread)
	{
		call(call_id_resume_thread(), (Call_arg)thread);
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
	 * Cancel blocking of a thread if it is in a cancelable blocking state
	 *
	 * \param thread  pointer to thread kernel object
	 *
	 * Does cleanly cancel a cancelable blocking thread state (IPC, signalling,
	 * stopped). The thread whose blocking was cancelled goes back to the
	 * 'active' thread state. If needed, it receives a syscall return value
	 * that reflects the cancellation. This syscall doesn't affect the pause
	 * state of the thread (see the 'pause_thread' syscall) which means that
	 * the thread may still be not allowed for scheduling. The syscall is
	 * core-restricted and may target any thread. It is actually used to
	 * limit the time a parent waits for a server when closing a session
	 * of one of its children.
	 */
	inline void cancel_thread_blocking(Thread * const thread)
	{
		call(call_id_cancel_thread_blocking(), (Call_arg)thread);
	}


	/**
	 * Set or unset the handler of an event that can be triggered by a thread
	 *
	 * \param thread             pointer to thread kernel object
	 * \param signal_context_id  capability id of the page-fault handler
	 */
	inline void thread_pager(Thread * const thread,
	                         capid_t  const signal_context_id)
	{
		call(call_id_thread_pager(), (Call_arg)thread, signal_context_id);
	}


	/**
	 * Execute a virtual-machine (again)
	 *
	 * \param vm  pointer to vm kernel object
	 */
	inline void run_vm(Vm * const vm)
	{
		call(call_id_run_vm(), (Call_arg) vm);
	}


	/**
	 * Stop execution of a virtual-machine
	 *
	 * \param vm  pointer to vm kernel object
	 */
	inline void pause_vm(Vm * const vm)
	{
		call(call_id_pause_vm(), (Call_arg) vm);
	}

	/**
	 * Create an interrupt object
	 *
	 * \param p                 memory donation for the irq object
	 * \param irq_nr            interrupt number
	 * \param signal_context_id capability id of the signal context
	 */
	inline int new_irq(addr_t const p, unsigned irq_nr,
	                   capid_t signal_context_id)
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

	/**
	 * Create a new object identity for a thread
	 *
	 * \param dst  memory donation for the new object
	 * \param cap  capability id of the targeted thread
	 */
	inline capid_t new_obj(void * const dst, capid_t const cap)
	{
		return call(call_id_new_obj(), (Call_arg)dst, (Call_arg)cap);
	}

	/**
	 * Destroy an object identity
	 *
	 * \param dst pointer to the object identity object
	 */
	inline void delete_obj(void * const dst)
	{
		call(call_id_delete_obj(), (Call_arg)dst);
	}
}

#endif /* _CORE__KERNEL__CORE_INTERFACE_H_ */
