/*
 * \brief  Lx_emul initial time (persistent clock)
 * \author Pirmin Duss
 * \date   2023-10-02
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 * Copyright (C) 2023 gapfruit AG
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */


#include <linux/time.h>

static unsigned long long genode_initial_ts_sec = 0;

void lx_emul_time_initial(unsigned long long seconds)
{
	genode_initial_ts_sec = seconds;
}

void read_persistent_clock64(struct timespec64 *ts)
{
	ts->tv_sec  = genode_initial_ts_sec;
	ts->tv_nsec = 0;
}
