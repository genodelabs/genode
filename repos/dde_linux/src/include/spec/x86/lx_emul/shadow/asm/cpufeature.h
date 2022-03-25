/**
 * \brief  Shadow copy of asm/cpufeature.h
 * \author Josef Soentgen
 * \date   2022-01-14
 */

#ifndef _ASM__CPUFEATURE_H_
#define _ASM__CPUFEATURE_H_

#include <asm/processor.h>

#if defined(__KERNEL__) && !defined(__ASSEMBLY__)

#include <asm/asm.h>
#include <linux/bitops.h>
#include <asm/alternative.h>

#define boot_cpu_has(bit) 0
#define static_cpu_has(bit) boot_cpu_has(bit)
#define static_cpu_has_bug(bit) static_cpu_has((bit))
#define cpu_has(value, bit) ( (void)value, 0 )

#endif /* defined(__KERNEL__) && !defined(__ASSEMBLY__) */

#endif
