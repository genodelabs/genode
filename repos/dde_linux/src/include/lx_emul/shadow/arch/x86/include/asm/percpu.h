/**
 * \brief  Shadow copy of asm/percpu.h
 * \author Stefan Kalkowski
 * \date   2022-06-29
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2 or later.
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

/*
 * Undef gs/fs for CONFIG_CC_HAS_NAMED_AS case
 */
#undef __percpu_seg_override
#define __percpu_seg_override

#endif /* _LX_EMUL__SHADOW__ARCH__X86__INCLUDE__ASM__PERCPU_H_ */

