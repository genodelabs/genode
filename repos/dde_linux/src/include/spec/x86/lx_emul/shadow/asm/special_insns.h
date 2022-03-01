/*
 * \brief  Shadows Linux kernel arch/x86/include/asm/special_insns.h
 * \author Alexander Boettcher
 * \date   2022-03-01
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _ASM_X86_SPECIAL_INSNS_H
#define _ASM_X86_SPECIAL_INSNS_H

#ifdef __KERNEL__

static inline unsigned long __native_read_cr3(void)
{
	lx_emul_trace_and_stop(__func__);
}

static inline void native_write_cr3(unsigned long val)
{
	lx_emul_trace_and_stop(__func__);
}

#endif

static inline void wbinvd(void)
{
	printk("%s - not implemented\n", __func__);
}

static inline unsigned long __read_cr4(void)
{
	lx_emul_trace_and_stop(__func__);
}

static inline unsigned long __read_cr3(void)
{
	lx_emul_trace_and_stop(__func__);
}

static inline void write_cr3(unsigned long x)
{
	lx_emul_trace_and_stop(__func__);
}

static inline void clflush(volatile void *__p)
{
	asm volatile("clflush %0" : "+m" (*(volatile char __force *)__p));
}

static inline void clflushopt(volatile void *__p)
{
	alternative_io(".byte 0x3e; clflush %P0",
		       ".byte 0x66; clflush %P0",
		       X86_FEATURE_CLFLUSHOPT,
		       "+m" (*(volatile char __force *)__p));
}

/* The dst parameter must be 64-bytes aligned */
static inline void movdir64b(void __iomem *dst, const void *src)
{
	const struct { char _[64]; } *__src = src;
	struct { char _[64]; } __iomem *__dst = dst;

	/*
	 * MOVDIR64B %(rdx), rax.
	 *
	 * Both __src and __dst must be memory constraints in order to tell the
	 * compiler that no other memory accesses should be reordered around
	 * this one.
	 *
	 * Also, both must be supplied as lvalues because this tells
	 * the compiler what the object is (its size) the instruction accesses.
	 * I.e., not the pointers but what they point to, thus the deref'ing '*'.
	 */
	asm volatile(".byte 0x66, 0x0f, 0x38, 0xf8, 0x02"
		     : "+m" (*__dst)
		     :  "m" (*__src), "a" (__dst), "d" (__src));
}

#endif /* _ASM_X86_SPECIAL_INSNS_H */
