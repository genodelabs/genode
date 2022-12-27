/*
 * \brief  Replaces kernel/sched/isolation.c
 * \author Stefan Kalkowski
 * \date   2022-07-20
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */


#include <linux/sched/isolation.h>


#ifdef CONFIG_CPU_ISOLATION
const struct cpumask * housekeeping_cpumask(enum hk_flags flags)
{
	static struct cpumask dummy;
	return &dummy;
}


bool housekeeping_enabled(enum hk_flags flags)
{
	return false;
}
#endif
