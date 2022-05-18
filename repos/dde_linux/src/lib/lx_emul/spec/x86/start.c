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


void time_init(void)
{
	lx_emul_time_init(); /* replaces timer_probe() */
	lpj_fine = 1000000 / HZ;
}


#include <asm/pgtable.h>

unsigned long empty_zero_page[PAGE_SIZE / sizeof(unsigned long)]
__attribute__((aligned(PAGE_SIZE)));

void lx_emul_setup_arch(void *dtb)
{
	/* fill zero page */
	memset(empty_zero_page, 0, PAGE_SIZE);
}
