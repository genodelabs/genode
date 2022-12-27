/*
 * \brief  Supplement for emulation for linux/include/asm-generic/delay.h
 * \author Josef Soentgen
 * \author Alexander Boettcher
 * \date   2022-05-05
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */


#include <asm-generic/delay.h>
#include <linux/delay.h>
#include <linux/jiffies.h>

#include <lx_emul.h>


void __const_udelay(unsigned long xloops)
{
	unsigned long const usecs = xloops / 0x10C7UL;
	__udelay(usecs);
}


void __udelay(unsigned long usecs)
{
	/*
	 * Account for delays summing up to equivalent of 1 jiffie during
	 * jiffies_64 stays same. When 1 jiffie is reached in sum,
	 * increase jiffie_64 to break endless loops, seen in
	 *  * intel_fb - cpu_relax(void) emulation used by
	 *               busy loop of slchi() in drivers/i2c/algos/i2c-algo-bit.c
	 *  * wifi_drv - net/wireless/intel/iwlwif* code during error code handling
	 */
	static uint64_t      last_jiffie = 0;
	static unsigned long delayed_sum = 0;

	if (jiffies_64 == last_jiffie) {
		delayed_sum += usecs;
	} else {
		last_jiffie = jiffies_64;
		delayed_sum = usecs;
	}

	if (usecs < 100)
		lx_emul_time_udelay(usecs);
	else {
		unsigned long slept = 0;
		while (slept < usecs) {
			lx_emul_time_udelay(100);
			slept += 100;
		}
	}

	/*
	 * When delays sum up to the equivalent of 1 jiffie,
	 * increase it to break endless loops.
	 */
	if (delayed_sum >= jiffies_to_usecs(1)) {
		jiffies_64 ++;
		delayed_sum = 0;
	}
}
