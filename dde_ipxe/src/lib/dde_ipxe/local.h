/*
 * \brief  DDE iPXE local helpers
 * \author Christian Helmuth
 * \date   2010-09-13
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <dde_kit/assert.h>
#include <dde_kit/printf.h>

#define FMT_BUSDEVFN "%02x:%02x.%x"

#define LOG(fmt, ...)                                             \
	do {                                                          \
		dde_kit_log(1, "\033[36m" fmt "\033[0m", ##__VA_ARGS__ ); \
	} while (0)

#define TRACE dde_kit_printf("\033[35m%s not implemented\033[0m\n", __func__)

#define ASSERT(x) dde_kit_assert(x)

extern void slab_init(void);
