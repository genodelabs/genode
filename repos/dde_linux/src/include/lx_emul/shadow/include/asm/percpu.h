/*
 * \brief  Shadows Linux kernel asm/percpu.h
 * \author Stefan Kalkowski
 * \date   2021-04-14
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_EMUL__SHADOW__ASM__PERCPU_H_
#define _LX_EMUL__SHADOW__ASM__PERCPU_H_

#include_next <asm/percpu.h>

#ifndef __ASSEMBLY__

static inline unsigned long __dummy_cpu_offset(void)
{
	return 0;
}

#undef  __my_cpu_offset
#define __my_cpu_offset __dummy_cpu_offset()

#ifdef this_cpu_cmpxchg_double
#undef this_cpu_cmpxchg_double
#define this_cpu_cmpxchg_double this_cpu_generic_cmpxchg_double
#endif

#endif /* __ASSEMBLY__ */

#endif /* _LX_EMUL__SHADOW__ASM__PERCPU_H_ */
