/*
 * \brief  Replaces kernel/sched/cputime.c
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


void account_idle_ticks(unsigned long ticks) { }
