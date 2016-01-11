/*
 * \brief   Cpu class implementation specific to ARM SMP
 * \author  Stefan Kalkowski
 * \date    2015-12-09
 */

/*
 * Copyright (C) 2015-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <kernel/lock.h>
#include <kernel/kernel.h>
#include <kernel/cpu.h>

/* base includes */
#include <unmanaged_singleton.h>


/* spin-lock used to synchronize kernel access of different cpus */
Kernel::Lock & Kernel::data_lock() {
	return *unmanaged_singleton<Kernel::Lock>(); }


/**
 * Setup non-boot CPUs
 */
extern "C" void init_kernel_mp()
{
	using namespace Kernel;

	cpu_pool()->cpu(Genode::Cpu::executing_id())->init(*pic(), *core_pd(), board());

	kernel();
}


void Kernel::Cpu_domain_update::_domain_update() {
	cpu_pool()->cpu(Cpu::executing_id())->invalidate_tlb_by_pid(_domain_id); }
