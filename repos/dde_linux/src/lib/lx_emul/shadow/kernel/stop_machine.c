/*
 * \brief  Replaces kernel/stop_machine.c
 * \author Stefan Kalkowski
 * \date   2021-03-16
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/stop_machine.h>

int stop_machine(cpu_stop_fn_t fn,void * data,const struct cpumask * cpus)
{
	return (*fn)(data);
}
