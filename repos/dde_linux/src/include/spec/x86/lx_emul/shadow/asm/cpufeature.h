/**
 * \brief  Shadow copy of asm/cpufeature.h
 * \author Josef Soentgen
 * \date   2022-01-14
 */

#ifndef _ASM__CPUFEATURE_H_
#define _ASM__CPUFEATURE_H_

#define boot_cpu_has(bit) 0
#define static_cpu_has(bit) boot_cpu_has(bit)

#endif
