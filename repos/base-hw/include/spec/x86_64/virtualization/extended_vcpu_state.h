/*
 * \brief   Extended vCPU state
 * \author  Benjamin Lamowski
 * \date    2023-05-25
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__X86_64_VIRTUALIZATION__EXTENDED_VCPU_STATE_H
#define _INCLUDE__SPEC__X86_64_VIRTUALIZATION__EXTENDED_VCPU_STATE_H

/* x86 CPU state */
#include <cpu/vcpu_state.h>
#include <cpu/atomic.h>

namespace Genode {
	struct Vm_state;
	struct Vcpu_run_state;
}

/*
 * Run state of the Vcpu to sync between the VMM library and the
 * kernel.
 */
class Genode::Vcpu_run_state : Noncopyable
{
	public:

		enum Value : int {
			/*
			 * vCPU isn't initialized yet. Needed for initialization in
			 * Vm::exception() and to block premature pause requests.
			 */
			STARTUP = 0,

			/*
			 * The vCPU is runable but not yet running. Used in pause() to
			 * make the vCPU run once (RUN_ONCE)
			 */
			RUNNABLE = 1,

			/*
			 * The vCPU hasn't run yet, but a pause has been requested.
			 * Run the vCPU once, dispatch the result and then issue a pause
			 * request.
			 */
			RUN_ONCE = 2,

			/*
			 * The vCPU is running. Used in pause() to force an exit only when the
			 * vCPU is actually running.
			 */
			RUNNING = 3,

			/*
			 * vCPU has exited because of an external interrupt and could run
			 * without state syncing. Needed to skip state syncing in Vm::proceed
			 * and to request updating the state from the vCPU in case of a
			 * Vcpu::pause() (SYNC_FROM_VCPU)
			 */
			INTERRUPTIBLE = 4,

			/*
			 * vCPU is running and is being forced out by a thread on a remote core
			 * by signaling the vCPU's handler. Causes a state writeback and
			 * Vm::pause() after an external interrupt VM exit.
			 */
			EXITING = 5,

			/*
			 * A Vcpu::pause() request was issued while the vCPU was INTERRUPTIBLE.
			 * Skips the next run in Vm::proceed() and causes a full pause exit in
			 * the subsequent Vm::exception().
			 */
			SYNC_FROM_VCPU = 6,

			/*
			 * The vCPU is dispatching a signal to the handler in the VMM. Needed to
			 * distinguish between a dispatch from the vCPU and a dispatch from an
			 * assynchronous pause request.
			 */
			DISPATCHING = 7,

			/*
			 * The vCPU needs to first dispatch an exit in the VMM, and a pause
			 * request needs to be injected right after.
			 */
			DISPATCHING_PAUSED = 8,

			/*
			 * An exit has been dispatched to the VMM. Needed to let
			 * an assynchrous pause request dispatch a new signal.
			 */
			DISPATCHED = 9,

			/*
			 * The vCPU was RUNNABLE, or DISPATCHED but a pause has been requested.
			 * Used to create a pause exit in the wrapper.
			 */
			PAUSING = 10,

			/*
			 * The vCPU handler in the VMM is dispatching and a pause
			 * signal has been issued. Needed to skip more pause requests.
			 * FIXME
			 */
			PAUSED = 11
		};

	private:

		int _value { }; /* base type of Vcpu_run_state */

	public:

		Value value() const { return Value(_value); }

		void set(Value const &value)
		{
			_value = value;
		}

		bool cas(Value cmp_val, Value new_val)
		{
			return cmpxchg(&_value, cmp_val, new_val);
		}
};

struct Genode::Vm_state : Genode::Vcpu_state
{
	Vcpu_run_state run_state;
};

#endif /* _INCLUDE__SPEC__X86_64_VIRTUALIZATION__EXTENDED_VCPU_STATE_H */
