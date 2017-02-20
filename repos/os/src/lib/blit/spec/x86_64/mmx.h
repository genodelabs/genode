/*
 * \brief  MMX-based blitting support for x86_64
 * \author Sebastian Sumpf
 * \date   2009-10-26
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIB__BLIT__SPEC__X86_64__MMX_H_
#define _LIB__BLIT__SPEC__X86_64__MMX_H_

/**
 * Copy 32byte chunks via MMX
 */
static inline void copy_32byte_chunks(void const *src, void *dst, int size)
{
	asm volatile (
		"emms                             \n\t"
		"xor    %%rcx,%%rcx               \n\t"
		".align 16                        \n\t"
		"0:                               \n\t"
		"movq   (%%rsi,%%rcx,8),%%mm0     \n\t"
		"movq   8(%%rsi,%%rcx,8),%%mm1    \n\t"
		"movq   16(%%rsi,%%rcx,8),%%mm2   \n\t"
		"movq   24(%%rsi,%%rcx,8),%%mm3   \n\t"
		"movntq %%mm0,(%%rdi,%%rcx,8)     \n\t"
		"movntq %%mm1,8(%%rdi,%%rcx,8)    \n\t"
		"movntq %%mm2,16(%%rdi,%%rcx,8)   \n\t"
		"movntq %%mm3,24(%%rdi,%%rcx,8)   \n\t"
		"add    $0x4, %%rcx               \n\t"
		"dec    %3                        \n\t"
		"jnz    0b                        \n\t"
		"sfence                           \n\t"
		"emms                             \n\t"  
		: "=r"(size)
		: "S" (src), "D" (dst), "0" (size)
		: "rax", "rcx", "memory"
	);
}

#endif /* _LIB__BLIT__SPEC__X86_64__MMX_H_ */
