/**
 * \brief  Core implementation of the CPU session interface extension
 * \author Stefan Kalkowski
 * \date   2011-04-04
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>

/* Core includes */
#include <cpu_session_component.h>
#include <platform.h>

/* Fiasco.OC includes */
namespace Fiasco {
#include <l4/sys/thread.h>
#include <l4/sys/factory.h>
}


void Genode::Cpu_session_component::enable_vcpu(Genode::Thread_capability thread_cap,
                                                Genode::addr_t vcpu_state)
{
	using namespace Genode;
	using namespace Fiasco;

	Lock::Guard lock_guard(_thread_list_lock);

	Cpu_thread_component *thread = _lookup_thread(thread_cap);
	if (!thread) return;

	Native_thread tid = thread->platform_thread()->thread().local->kcap();

	l4_msgtag_t tag = l4_thread_vcpu_control(tid, vcpu_state);
	if (l4_msgtag_has_error(tag))
		PWRN("l4_thread_vcpu_control failed");
}


Genode::Native_capability
Genode::Cpu_session_component::native_cap(Genode::Thread_capability cap)
{
	using namespace Genode;

	Lock::Guard lock_guard(_thread_list_lock);

	Cpu_thread_component *thread = _lookup_thread(cap);
	if (!thread) return Native_capability();

	return Native_capability(thread->platform_thread()->thread().local);
}


Genode::Native_capability Genode::Cpu_session_component::alloc_irq()
{
	using namespace Fiasco;

	Cap_index* i = cap_map()->insert(platform_specific()->cap_id_alloc()->alloc());
	l4_msgtag_t res = l4_factory_create_irq(L4_BASE_FACTORY_CAP, i->kcap());
	if (l4_error(res))
		PWRN("Allocation of irq object failed!");

	/*
	 * Increment reference-counter, because we don't hold any
	 * reference to IRQ object by now
	 */
	i->inc();
	return Genode::Native_capability(i);
}


void Genode::Cpu_session_component::single_step(Genode::Thread_capability thread_cap, bool enable)
{
	using namespace Genode;

	Lock::Guard lock_guard(_thread_list_lock);

	Cpu_thread_component *thread = _lookup_thread(thread_cap);
	if (!thread) return;

	Native_thread tid = thread->platform_thread()->thread().local->kcap();

	enum { THREAD_SINGLE_STEP = 0x40000 };
	int flags = enable ? THREAD_SINGLE_STEP : 0;

	Fiasco::l4_thread_ex_regs(tid, ~0UL, ~0UL, flags);
}
