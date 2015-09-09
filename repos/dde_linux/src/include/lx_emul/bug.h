/*
 * \brief  Linux kernel API
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \date   2014-08-21
 *
 * Based on the prototypes found in the Linux kernel's 'include/'.
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/***************
 ** asm/bug.h **
 ***************/

#define WARN_ON(condition) ({ \
	int ret = !!(condition); \
	if (ret) lx_printf("[%s] WARN_ON(" #condition ") ", __func__); \
	ret; })

#define WARN(condition, fmt, arg...) ({ \
	int ret = !!(condition); \
	if (ret) lx_printf("[%s] *WARN* " fmt , __func__ , ##arg); \
	ret; })

#define BUG() do { \
	lx_printf("BUG: failure at %s:%d/%s()!\n", __FILE__, __LINE__, __func__); \
	while (1); \
} while (0)

#define WARN_ON_ONCE WARN_ON
#define WARN_ONCE    WARN

#define BUG_ON(condition) do { if (condition) BUG(); } while(0)
