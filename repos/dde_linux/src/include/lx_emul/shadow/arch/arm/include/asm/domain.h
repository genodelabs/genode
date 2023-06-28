/*
 * \brief  Shadows Linux kernel arch/arm/include/asm/domain.h
 * \author Stefan Kalkowski
 * \date   2023-06-27
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/*
 * Undefine MMU support for domain accesses, although the configuration
 * would allow it, to prevent system accesses.
 */
#ifdef  CONFIG_CPU_CP15_MMU
#undef  CONFIG_CPU_CP15_MMU
#include_next <asm/domain.h>
#define CONFIG_CPU_CP15_MMU
#else
#include_next <asm/domain.h>
#endif
