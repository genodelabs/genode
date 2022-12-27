/*
 * \brief  Shadow copy of linux/of.h
 * \author Norman Feske
 * \date   2021-08-19
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_EMUL__SHADOW__LINUX__OF_H_
#define _LX_EMUL__SHADOW__LINUX__OF_H_

#include_next <linux/of.h>
#include      <lx_emul/init.h>

#undef OF_DECLARE_1
#undef OF_DECLARE_2

/* used to populate __clk_of_table */
#define OF_DECLARE_1(table, name, compat, fn) \
	static void __of_declare_initcall_##fn##name(void)__attribute__((constructor)); \
	static void __of_declare_initcall_##fn##name() { \
			lx_emul_register_of_##table##_initcall(compat, fn); };

/* used to populate __irqchip_of_table */
#define OF_DECLARE_2(table, name, compat, fn) \
	static void __of_declare_initcall_##fn##name(void)__attribute__((constructor)); \
	static void __of_declare_initcall_##fn##name() { \
			lx_emul_register_of_##table##_initcall(compat, fn); };

#endif /* _LX_EMUL__SHADOW__LINUX__OF_H_ */
