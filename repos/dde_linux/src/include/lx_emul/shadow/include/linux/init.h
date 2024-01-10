/*
 * \brief  Shadow copy of linux/init.h
 * \author Stefan Kalkowski
 * \date   2021-03-10
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_EMUL__SHADOW__LINUX__INIT_H_
#define _LX_EMUL__SHADOW__LINUX__INIT_H_

#include_next <linux/init.h>
#include      <lx_emul/init.h>

/**
 * We have to re-define certain initcall macros, because the original function
 * puts all initcalls into the .init section that is not exported by our
 * linker script.
 * Instead, we define ctor functions that register the initcalls and their
 * priority in our lx_emul environment.
 */
#undef ___define_initcall
#undef __define_initcall

/*
 * Define local register function and global pointer to function in the form
 * '__initptr_<function name>_<id>_<counter>_<line number of macro>'
 */
#define __define_initcall(fn, id)                    \
	static void __initcall_##fn##id(void) {            \
		lx_emul_register_initcall(fn, __func__); }       \
	                                                   \
	void * __PASTE(__initptr_##fn##id##_,              \
	       __PASTE(__COUNTER__,                        \
	       __PASTE(_,__LINE__))) = __initcall_##fn##id;

#endif /* _LX_EMUL__SHADOW__LINUX__INIT_H_ */
