/*
 * \brief  Linux Kernel initialization
 * \author Stefan Kalkowski
 * \date   2022-01-08
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul/init.h>
#include <lx_emul/time.h>

#include <linux/delay.h>
#include <linux/sched/clock.h>

unsigned long long sched_clock(void)
{
	return lx_emul_time_counter() * 1000;
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,4,0)
unsigned long long sched_clock_noinstr(void)
{
	return sched_clock();
}
#endif


void time_init(void)
{
	lx_emul_time_init(); /* replaces timer_probe() */
}


#include <asm/pgtable.h>

/*
 * Note that empty_zero_page lands in the BSS section and is therefore
 * automatically zeroed at program startup.
 */
unsigned long empty_zero_page[PAGE_SIZE / sizeof(unsigned long)]
__attribute__((aligned(PAGE_SIZE)));

void lx_emul_setup_arch(void *dtb) { }
