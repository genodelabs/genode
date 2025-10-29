/*
 * \brief   Kernel backend for virtual machines
 * \author  Stefan Kalkowski
 * \date    2015-02-10
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <kernel/thread.h>
#include <kernel/vcpu.h>
#include <kernel/cpu.h>

Kernel::capid_t
Kernel::Thread::_call_vcpu_create(Core::Kernel_object<Vcpu> &kobj,
                                  Call_arg cpuid, Board::Vcpu_state &state,
                                  Vcpu::Identity &identity, capid_t sig_cap)
{
	_cpu_pool.with_cpu(cpuid,
		[&] (Cpu &cpu) {
			_pd.cap_tree().with<Signal_context>(sig_cap,
				[&] (Signal_context &context) {
					kobj.construct(_core_pd, _user_irq_pool, cpu, state,
					               context, identity); },
				[] { /* ignore failure */ });
		});

	return (kobj.constructed()) ?  kobj->core_capid() : cap_id_invalid();
}


void Kernel::Thread::_call_vcpu_destroy(Core::Kernel_object<Vcpu>& to_delete)
{
	/**
	 * Delete a vcpu immediately if it is assigned to this cpu,
	 * or the assigned cpu is not running it.
	 */
	if (!to_delete->remotely_running()) {
		to_delete.destruct();
		return;
	}

	/**
	 * Construct a cross-cpu work item and send an IPI
	 */
	_vcpu_destroy.construct(*this, to_delete);
	to_delete->_cpu().trigger_ip_interrupt();
}


void Kernel::Thread::_call_vcpu_run(capid_t const id)
{
	_pd.cap_tree().with<Vcpu>(id,
		[] (auto &vcpu) { vcpu.run(); },
		[] () { /* ignore failure */ });
}


void Kernel::Thread::_call_vcpu_pause(capid_t const id)
{
	_pd.cap_tree().with<Vcpu>(id,
		[] (auto &vcpu) { vcpu.pause(); },
		[] () { /* ignore failure */ });
}
