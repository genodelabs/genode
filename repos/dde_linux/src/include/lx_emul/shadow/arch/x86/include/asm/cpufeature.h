/**
 * \brief  Shadow copy of asm/cpufeature.h
 * \author Josef Soentgen
 * \date   2022-01-14
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _ASM__CPUFEATURE_H_
#define _ASM__CPUFEATURE_H_

#include_next <asm/cpufeature.h>

#if defined(__KERNEL__) && !defined(__ASSEMBLY__)

#undef boot_cpu_has
#undef static_cpu_has
#undef static_cpu_has_bug
#undef cpu_has

#define boot_cpu_has(bit) 0
#define static_cpu_has(bit) boot_cpu_has(bit)
#define static_cpu_has_bug(bit) static_cpu_has((bit))
#define cpu_has(value, bit) ( (void)value, 0 )

#endif /* defined(__KERNEL__) && !defined(__ASSEMBLY__) */

#endif
