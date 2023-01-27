/**
 * \brief  Architecture-specific dummy definitions of Linux Kernel functions
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2022-05-09
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>


#include <linux/clocksource.h>

void clocksource_arch_init(struct clocksource * cs)
{
	lx_emul_trace(__func__);
}


struct cpuinfo_x86 boot_cpu_data __read_mostly;

