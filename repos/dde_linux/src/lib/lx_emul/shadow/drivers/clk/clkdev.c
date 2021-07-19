/*
 * \brief  Replaces drivers/clk/clkdev.c
 * \author Stefan Kalkowski
 * \date   2021-03-16
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/clkdev.h>
#include <linux/device.h>
#include <lx_emul/clock.h>

struct clk *clk_get(struct device *dev, const char *con_id)
{
	if (!dev || !dev->of_node)
		return NULL;

	return lx_emul_clock_get(dev->of_node, con_id);
}


void clk_put(struct clk *clk) {}
