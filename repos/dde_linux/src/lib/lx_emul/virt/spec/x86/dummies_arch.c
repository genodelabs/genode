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
 * version 2.
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
