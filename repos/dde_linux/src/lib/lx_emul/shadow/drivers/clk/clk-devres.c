/*
 * \brief  Replaces drivers/clk/clk-devres.c
 * \author Christian Helmuth
 * \date   2023-06-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/clk.h>


struct clk *devm_clk_get(struct device *dev, const char *id)
{
	return clk_get(dev, id);
}


struct clk *devm_clk_get_optional(struct device *dev,const char *id)
{
	return devm_clk_get(dev, id);
}
