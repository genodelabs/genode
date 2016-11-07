/*
 * \brief  Class for kernel data that is needed to manage a specific CPU
 * \author Reto Buerki
 * \author Stefan Kalkowski
 * \date   2015-02-09
 */

/*
 * Copyright (C) 2015-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/log.h>

/* base-internal includes */
#include <base/internal/globals.h>  /* init_log() */

/* core includes */
#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <kernel/pd.h>

using namespace Kernel;


Cpu_idle::Cpu_idle(Cpu * const cpu) : Cpu_job(Cpu_priority::MIN, 0)
{
	Cpu_job::cpu(cpu);
	ip = (addr_t)&_main;
	sp = (addr_t)&_stack[stack_size];
	init((addr_t)core_pd()->translation_table(), true);
}


void Kernel::Cpu::init(Pic &pic, Kernel::Pd &core_pd, Genode::Board&)
{
	Timer::disable_pit();

	fpu().init();

	Genode::init_log();

	/*
	 * Please do not remove the log(), because the serial constructor requires
	 * access to the Bios Data Area, which is available in the initial
	 * translation table set, but not in the final tables used after
	 * Cr3::write().
	 */
	Genode::log("Switch to core's final translation table");

	Cr3::write(Cr3::init((addr_t)core_pd.translation_table()));

	/* enable timer interrupt */
	unsigned const cpu = Cpu::executing_id();
	pic.unmask(Timer::interrupt_id(cpu), cpu);
}


void Cpu_domain_update::_domain_update() { }
