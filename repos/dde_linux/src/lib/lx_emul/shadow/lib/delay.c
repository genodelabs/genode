/*
 * \brief  Supplement for emulation for arch-specific lib/delay.c
 * \author Stefan Kalkowski
 * \date   2023-03-02
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */


#include <asm-generic/delay.h>
#include <asm/vdso/processor.h>
#include <linux/delay.h>

#include <lx_emul.h>
#include <lx_emul/time.h>


void __const_udelay(unsigned long xloops)
{
	unsigned long const usecs = xloops / 0x10C7UL;
	__udelay(usecs);
}


void __udelay(unsigned long usecs)
{
	/*
	 * if interrupts are open, jiffies get updated implicitely
	 * by call of cpu_relax()
	 */
	unsigned long long end = lx_emul_time_counter() + usecs;
	while (lx_emul_time_counter() < end) cpu_relax();
}
