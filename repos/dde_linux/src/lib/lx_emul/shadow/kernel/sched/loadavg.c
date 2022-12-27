/*
 * \brief  Replaces kernel/sched/loadavg.c
 * \author Stefan Kalkowski
 * \date   2022-07-20
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/sched.h>


void calc_load_nohz_start(void) { }


void calc_load_nohz_stop(void) { }
