/*
 * \brief  Lx_emul support for device clocks
 * \author Stefan Kalkowski
 * \date   2021-04-14
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_EMUL__CLOCK_H_
#define _LX_EMUL__CLOCK_H_

#ifdef __cplusplus
extern "C" {
#endif

struct device_node;
struct clk;

struct clk * lx_emul_clock_get(const struct device_node * node,
                               const char               * name);

unsigned long lx_emul_clock_get_rate(struct clk * clk);

#ifdef __cplusplus
}
#endif

#endif /* _LX_EMUL__CLOCK_H_ */
