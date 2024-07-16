/*
 * \brief  Replaces arch/arm64/kernel/cacheinfo.c
 * \author Christian Helmuth
 * \date   2024-07-16
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <asm/cache.h>

int cache_line_size(void)
{
	return ARCH_DMA_MINALIGN;
}
