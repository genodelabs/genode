/*
 * \brief  Replaces arch/arm/kernel/cacheinfo.c
 * \author Josef Soentgen
 * \date   2026-01-14
 */

/*
 * Copyright (C) 2026 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2 or later.
 */

#include <asm/cache.h>

int cache_line_size(void)
{
	return ARCH_DMA_MINALIGN;
}
