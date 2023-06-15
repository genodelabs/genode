/*
 * \brief  Replaces drivers/clk/clk.c
 * \author Stefan Kalkowski
 * \date   2021-03-16
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/clk.h>
#include <lx_emul/clock.h>

unsigned long clk_get_rate(struct clk * clk)
{
	return lx_emul_clock_get_rate(clk);
}


int clk_set_rate(struct clk * clk,unsigned long rate)
{
	if (lx_emul_clock_get_rate(clk) != rate)
		printk("Error: cannot change clock rate dynamically to %ld\n", rate);

	return 0;
}


int clk_prepare(struct clk * clk)
{
	return 0;
}


int clk_enable(struct clk * clk)
{
	return 0;
}


void clk_disable(struct clk * clk) { }


void clk_unprepare(struct clk * clk) { }


struct of_device_id;


void of_clk_init(const struct of_device_id *matches) { }


const char *__clk_get_name(const struct clk *clk)
{
	return "unknown-clk";
}
