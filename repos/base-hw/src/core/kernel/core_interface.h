/*
 * \brief  Parts of the kernel interface that are restricted to core
 * \author Martin stein
 * \author Stefan Kalkowski
 * \date   2014-03-15
 */

/*
 * Copyright (C) 2014-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__KERNEL__CORE_INTERFACE_H_
#define _CORE__KERNEL__CORE_INTERFACE_H_

/* base includes */
#include <cpu/cpu_state.h>

/* base-internal includes */
#include <base/internal/native_utcb.h>

/* base-hw includes */
#include <kernel/interface.h>

namespace Genode { class Native_utcb; }

namespace Kernel {

	class Pd;
	class Thread;
	class Signal_receiver;
	class Signal_context;
	class Vcpu;
	class User_irq;
	using Native_utcb = Genode::Native_utcb;
	using Cpu_state   = Genode::Cpu_state;
	template <typename T> class Core_object_identity;

	/**
	 * Kernel core-only system call IDs
	 */
	enum class Core_call_id : Call_arg {
		FIRST_CALL = 100,
		CPU_SUSPEND = FIRST_CALL,
		IRQ_ACK,
		IRQ_CREATE,
		IRQ_DESTROY,
		OBJECT_CREATE,
		OBJECT_DESTROY,
		PD_CREATE,
		PD_DESTROY,
		PD_INVALIDATE_TLB,
		SIGNAL_CONTEXT_CREATE,
		SIGNAL_CONTEXT_DESTROY,
		SIGNAL_RECEIVER_CREATE,
		SIGNAL_RECEIVER_DESTROY,
		THREAD_CORE_CREATE,
		THREAD_CPU_STATE_GET,
		THREAD_CPU_STATE_SET,
		THREAD_CREATE,
		THREAD_DESTROY,
		THREAD_EXC_STATE_GET,
		THREAD_PAGER_SET,
		THREAD_PAGER_SIGNAL_ACK,
		THREAD_PAUSE,
		THREAD_RESUME,
		THREAD_SINGLE_STEP,
		THREAD_START,
		VCPU_CREATE,
		VCPU_DESTROY,
	};

	auto core_call(Core_call_id id, auto &&... args)
	{
		return arch_call((Call_arg)id, args...);
	}

	/**
	 * Invalidate TLB entries for the `pd` in region `addr`, `sz`
	 */
	inline void pd_invalidate_tlb(Pd &pd, addr_t const addr,
	                              size_t const sz)
	{
		core_call(Core_call_id::PD_INVALIDATE_TLB, (Call_arg)&pd,
		          (Call_arg)addr, (Call_arg)sz);
	}


	/**
	 * Pause execution of a thread until 'resume_thread' is called on it
	 *
	 * \param thread  reference to thread kernel object
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
	inline void thread_pause(Thread &thread)
	{
		core_call(Core_call_id::THREAD_PAUSE, (Call_arg)&thread);
	}


	/**
	 * End blocking of a paused thread
	 *
	 * \param thread  pointer to thread kernel object
	 */
	inline void thread_resume(Thread &thread)
	{
		core_call(Core_call_id::THREAD_RESUME, (Call_arg)&thread);
	}


	/**
	 * Start execution of a thread
	 *
	 * \param thread  pointer to thread kernel object
	 * \param cpu_id  kernel name of the targeted CPU
	 * \param utcb    core local pointer to userland thread-context
	 *
	 * \retval   0  suceeded
	 * \retval !=0  failed
	 */
	inline Rpc_result thread_start(Thread &thread, Native_utcb &utcb)
	{
		return (core_call(Core_call_id::THREAD_START, (Call_arg)&thread,
		                  (Call_arg)&utcb) == (Call_ret)Rpc_result::OK)
			 ? Rpc_result::OK : Rpc_result::OUT_OF_CAPS;
	}


	/**
	 * Set or unset the handler of an event that can be triggered by a thread
	 *
	 * \param thread             reference to thread kernel object
	 * \param pager              reference to pager kernel object
	 * \param signal_context_id  capability id of the page-fault handler
	 */
	inline void thread_pager(Thread &thread,
	                         Thread &pager,
	                         capid_t const signal_context_id)
	{
		core_call(Core_call_id::THREAD_PAGER_SET, (Call_arg)&thread,
		          (Call_arg)&pager, signal_context_id);
	}


	/**
	 * Acknowledge interrupt
	 *
	 * \param irq  pointer to interrupt kernel object
	 */
	inline void irq_ack(User_irq &irq)
	{
		core_call(Core_call_id::IRQ_ACK, (Call_arg) &irq);
	}


	/**
	 * Get CPU state
	 *
	 * \param thread        pointer to thread kernel object
	 * \param thread_state  pointer to result CPU state object
	 */
	inline void thread_cpu_state_get(Thread &thread, Cpu_state &cpu_state)
	{
		core_call(Core_call_id::THREAD_CPU_STATE_GET, (Call_arg)&thread,
		          (Call_arg)&cpu_state);
	}


	/**
	 * Set CPU state
	 *
	 * \param thread        pointer to thread kernel object
	 * \param thread_state  pointer to CPU state object
	 */
	inline void thread_cpu_state_set(Thread &thread, Cpu_state &cpu_state)
	{
		core_call(Core_call_id::THREAD_CPU_STATE_SET, (Call_arg)&thread,
		          (Call_arg)&cpu_state);
	}


	/**
	 * Enable/disable single-stepping
	 *
	 * \param thread  pointer to thread kernel object
	 * \param on      enable or disable
	 */
	inline void thread_single_step(Thread &thread, bool on)
	{
		core_call(Core_call_id::THREAD_SINGLE_STEP, (Call_arg)&thread,
		          (Call_arg)on);
	}

	/**
	 * Acknowledge a signal transmitted to a pager
	 *
	 * \param context   signal context to acknowledge
	 * \param thread    reference to faulting thread kernel object
	 * \param resolved  whether fault got resolved
	 */
	inline void thread_pager_signal_ack(capid_t const context, Thread &thread,
	                                    bool resolved)
	{
		core_call(Core_call_id::THREAD_PAGER_SIGNAL_ACK, context,
		          (Call_arg)&thread, resolved);
	}


	enum class Cpu_suspend_result : Call_arg { OK, FAILED };

	/**
	 * Suspend hardware
	 *
	 * \param sleep_type  The intended sleep state S0 ... S5. The values are
	 *                    read out by an ACPI AML component and are of type
	 *                    TYP_SLPx as described in the ACPI specification,
	 *                    e.g. TYP_SLPa and TYP_SLPb. The values differ
	 *                    between different PC systems/boards.
	 */
	inline Cpu_suspend_result cpu_suspend(unsigned const sleep_type)
	{
		return (core_call(Core_call_id::CPU_SUSPEND, sleep_type)
		        == (Call_ret)Cpu_suspend_result::OK) ? Cpu_suspend_result::OK
		                                             : Cpu_suspend_result::FAILED;
	}
}

#endif /* _CORE__KERNEL__CORE_INTERFACE_H_ */
