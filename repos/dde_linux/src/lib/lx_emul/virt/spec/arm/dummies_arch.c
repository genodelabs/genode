/**
 * \brief  Architecture-specific dummy definitions of Linux Kernel functions
 * \author Sebastian Sumpf
 * \date   2023-07-01
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>
#include <linux/of_fdt.h>

bool __init early_init_dt_scan(void * params)
{
	lx_emul_trace(__func__);
	return false;
}


#include <linux/of_fdt.h>

void __init unflatten_device_tree(void)
{
	lx_emul_trace(__func__);
}


#include <linux/of_clk.h>

void __init of_clk_init(const struct of_device_id * matches)
{
	lx_emul_trace(__func__);
}


#include <linux/of.h>

void __init of_core_init(void)
{
	lx_emul_trace(__func__);
}


#include <linux/irqchip.h>

void __init irqchip_init(void)
{
	lx_emul_trace(__func__);
}


#include <linux/sched_clock.h>

void __init sched_clock_register(u64 (* read)(void),int bits,unsigned long rate)
{
	lx_emul_trace(__func__);
}


#include <linux/sched_clock.h>

void __init generic_sched_clock_init(void)
{
	lx_emul_trace(__func__);
}


