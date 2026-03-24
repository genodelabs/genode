/**
 * \brief  Architecture-specific dummy definitions of Linux Kernel functions
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2022-05-09
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2 or later.
 */

#include <lx_emul.h>


#include <linux/clocksource.h>

void clocksource_arch_init(struct clocksource * cs)
{
	lx_emul_trace(__func__);
}


/*
 * Early_identify_cpu() in linux sets this up normally, used by drm_cache,
 * arch/x86/lib/delay.c, and slub allocator.
 */
struct cpuinfo_x86 boot_cpu_data =
{
    .x86_clflush_size    = (sizeof(void*) == 8) ? 64 : 32,
    .x86_cache_alignment = (sizeof(void*) == 8) ? 64 : 32,
    .x86_phys_bits       = (sizeof(void*) == 8) ? 36 : 32,
    .x86_virt_bits       = (sizeof(void*) == 8) ? 48 : 32
};


/*
 * Normally this is only called as ALTERNATIVE on CPUs that do not
 * support cx8/cx16 that, pratically speaking, will not be used with
 * Genode and is here to prevent a undefined reference linking
 * error.
 */

void this_cpu_cmpxchg8b_emu(void)
{
	printk("%s: this function should never be called\n", __func__);
	lx_emul_trace_and_stop(__func__);
}

void this_cpu_cmpxchg16b_emu(void)
{
	printk("%s: this function should never be called\n", __func__);
	lx_emul_trace_and_stop(__func__);
}
