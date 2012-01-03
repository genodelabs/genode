/*
 * \brief  Support for initializers (i.e., constructors)
 * \author Christian Helmuth
 * \date   2008-08-15
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__DDE_KIT__INITCALL_H_
#define _INCLUDE__DDE_KIT__INITCALL_H_

/**
 * Mark function as DDE kit initcall
 *
 * \param fn     function name
 * \param id     identifier to distinguish multiple calls to 'fn'
 *
 * The initcall function must comply 'int func(void)'.
 * The marked function is exported via a non-static symbol called
 * "dde_kit_initcall_<id>_<fn>". On driver startup, the driver environment has
 * to explicitly call these functions.
 *
 * This is the right mechanism to mark, e.g., Linux module_init() functions.
 */
#define DDE_KIT_INITCALL(fn,id) \
	int (*dde_kit_initcall_##id##_##fn)(void) = fn

#endif /* _INCLUDE__DDE_KIT__INITCALL_H_ */
