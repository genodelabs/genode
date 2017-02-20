/*
 * \brief   Cpu class implementation specific to Cortex A9 SMP
 * \author  Stefan Kalkowski
 * \date    2015-12-09
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <kernel/perf_counter.h>
#include <kernel/lock.h>
#include <kernel/pd.h>
#include <pic.h>
#include <platform_pd.h>

extern int _mt_begin;
extern int _mt_master_context_begin;


void Kernel::Cpu::init(Kernel::Pic &pic)
{
	pic.init_cpu_local();

	static Hw::Address_space invalid_space(nullptr);
	Cpu_context * c = (Cpu_context*) (Cpu::exception_entry + ((addr_t)&_mt_master_context_begin - (addr_t)&_mt_begin));
	c->cpu_exception = Genode::Cpu::Ttbr0::init((addr_t)invalid_space.translation_table_phys());
	_fpu.init();

	{
		Lock::Guard guard(data_lock());

		/* enable performance counter */
		perf_counter()->enable();

		/* enable timer interrupt */
		pic.unmask(Timer::interrupt_id(id()), id());
	}
}
