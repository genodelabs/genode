/*
 * \brief  Platform specific part of the Linux API emulation
 * \author Sebastian Sumpf
 * \date   2012-06-18
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _X86_32__PLATFORM__LX_EMUL_
#define _X86_32__PLATFORM__LX_EMUL_

struct platform_device
{
	void *data;
};

/*******************
 ** asm/barrier.h **
 *******************/

#define mb()  asm volatile ("mfence": : :"memory")
#define rmb() asm volatile ("lfence": : :"memory")
#define wmb() asm volatile ("sfence": : :"memory")

/*
 * This is the "safe" implementation as needed for a configuration
 * with SMP enabled.
 */

#define smp_mb()  mb()
#define smp_rmb() barrier()
#define smp_wmb() barrier()

static inline void barrier() { asm volatile ("": : :"memory"); }

#endif /* _X86_32__PLATFORM__LX_EMUL_ */
