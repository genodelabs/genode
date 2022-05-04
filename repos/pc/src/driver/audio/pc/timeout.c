/*
 * \brief  For capture in 'hda_dsp_pcm_pointer' 'usleep_range' is called with
 *         20, 21 us. Since this calls 'schedule_hrtimeout_range' through
 *         'uslseep_range_state' which sleeps for far too long, we implement
 *         'usleep_range_state' with a 'udelay' here.
 * \author Sebastian Sumpf
 * \date   2022-05-28
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul/time.h>
#include <asm-generic/delay.h>

void __wrap_usleep_range_state(unsigned long min, unsigned long max,
                               unsigned int state)
{
	udelay(min);
}
