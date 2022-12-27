/**
 * \brief  Shadow copy of asm/percpu.h
 * \author Stefan Kalkowski
 * \date   2022-06-29
 */

#ifndef _LX_EMUL__SHADOW__ARCH__X86__INCLUDE__ASM__PERCPU_H_
#define _LX_EMUL__SHADOW__ARCH__X86__INCLUDE__ASM__PERCPU_H_

#include_next <asm/percpu.h>

/*
 * The original implementation uses gs or fs register
 * to hold an cpu offset, which is not set correctly in our use-case
 * where we use one cpu only anyway.
 */
#undef __percpu_prefix
#define __percpu_prefix ""

#endif /* _LX_EMUL__SHADOW__ARCH__X86__INCLUDE__ASM__PERCPU_H_ */

