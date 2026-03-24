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

static inline bool genode_features_allowed(unsigned long bit)
{
	if      (bit == X86_FEATURE_CLFLUSH) return true;
	else if (bit == X86_FEATURE_CX8)     return true;
	else if (bit == X86_FEATURE_CX16)    return true;

	return false;
}

#define boot_cpu_has(bit) genode_features_allowed(bit)
#define static_cpu_has(bit) boot_cpu_has(bit)
#define static_cpu_has_bug(bit) static_cpu_has((bit))
#define cpu_has(value, bit) ( (void)value, boot_cpu_has(bit) )

#endif /* defined(__KERNEL__) && !defined(__ASSEMBLY__) */

#endif
