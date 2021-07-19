/*
 * \brief  Replaces kernel/smp.c
 * \author Stefan Kalkowski
 * \date   2021-03-16
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/cpumask.h>

unsigned int nr_cpu_ids = 1;

unsigned long irq_err_count = 0;
