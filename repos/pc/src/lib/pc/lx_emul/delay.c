/*
 * \brief  Supplement for emulation for linux/include/asm-generic/delay.h
 * \author Josef Soentgen
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

#include <lx_emul.h>


void __const_udelay(unsigned long xloops)
{
       unsigned long usecs = xloops / 0x10C7UL;
       if (usecs < 100)
               lx_emul_time_udelay(usecs);
       else
               usleep_range(usecs, usecs * 10);
}


void __udelay(unsigned long usecs)
{
	lx_emul_time_udelay(usecs);
}
