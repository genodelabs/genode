/*
 * \brief  Lx_emul backend for peripheral clocks
 * \author Stefan Kalkowski
 * \date   2021-03-22
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <base/log.h>
#include <lx_kit/env.h>
#include <lx_emul/clock.h>

/*
 * CONFIG_OF is normally disabled on PC thus of_device_is_compatible() should
 * be "static inline" not "extern" (see include/linux/of.h), thus enable this
 * function only if not building for x86.
 */
extern "C" int of_device_is_compatible(const struct device_node * node, const char * compat);
static int _of_device_is_compatible(const struct device_node * node, const char * compat)
{
#if defined(__x86_64__) || defined(__i386__)
	(void)node; (void)compat;
	return 0;
#else
	return of_device_is_compatible(node, compat);
#endif
}

struct clk * lx_emul_clock_get(const struct device_node * node,
                               const char * name)
{
	using namespace Lx_kit;

	struct clk * ret = nullptr;

	env().devices.for_each([&] (Device & d) {
		if (!_of_device_is_compatible(node, d.compatible()))
			return;
		ret = name ? d.clock(name) : d.clock(0U);
		if (!ret) warning("No clock ", name, " found for device ", d.name());
	});

	return ret;
}


unsigned long lx_emul_clock_get_rate(struct clk * clk)
{
	if (!clk)
		return 0;

	return clk->rate;
}
