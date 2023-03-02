/*
 * \brief  Shadows Linux kernel arch/arm/include/asm/delay.h
 * \author Stefan Kalkowski
 * \date   2023-03-03
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef __ASM_DELAY_H
#define __ASM_DELAY_H

/*
 * We shadow the original arm variant and use the generic version instead,
 * like x86 and arm64. Thereby, we can simply use one and the same backend
 * function for udelay, which is part of our emulation codebase anyway.
 */
#include <asm-generic/delay.h>

#endif

