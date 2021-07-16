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
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/***************
 ** asm/bug.h **
 ***************/

extern void lx_sleep_forever() __attribute__((noreturn));

#define WARN_ON(condition) ({ \
	int ret = !!(condition); \
	if (ret) lx_printf("[%s] WARN_ON(%s) \n", __func__, #condition); \
	ret; })

#define WARN(condition, fmt, arg...) ({ \
	int ret = !!(condition); \
	if (ret) lx_printf("[%s] *WARN* " fmt " \n", __func__ , ##arg); \
	ret; })

#define BUG() do { \
	lx_printf("BUG: failure at %s:%d/%s()!\n", __FILE__, __LINE__, __func__); \
	lx_sleep_forever(); \
} while (0)

#define WARN_ON_ONCE WARN_ON
#define WARN_ONCE    WARN

#define BUG_ON(condition) do { if (condition) BUG(); } while(0)

#define BUILD_BUG_ON_INVALID(e) ((void)(sizeof((__force long)(e))))
#define BUILD_BUG_ON_ZERO(e) (sizeof(struct { int:(-!!(e)); }))

#define BUILD_BUG_ON_MSG(cond,msg) ({ \
		extern int __attribute__((error(msg))) build_bug(); \
		if (cond) { build_bug(); } })

#define BUILD_BUG() BUILD_BUG_ON_MSG(1,"BUILD_BUG failed")

#define BUILD_BUG_ON_NOT_POWER_OF_2(n)          \
	BUG_ON((n) == 0 || (((n) & ((n) - 1)) != 0))
