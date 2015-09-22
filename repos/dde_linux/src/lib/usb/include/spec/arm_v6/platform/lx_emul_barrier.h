/*
 * \brief  ARMv6-specific part of the Linux API emulation
 * \author Christian Prochaska
 * \date   2014-05-28
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */
#ifndef _ARMV6__PLATFORM__LX_EMUL_BARRIER_H_
#define _ARMV6__PLATFORM__LX_EMUL_BARRIER_H_


/*******************
 ** asm/barrier.h **
 *******************/

#define mb()  asm volatile ("": : :"memory")
#define rmb() mb()
#define wmb() asm volatile ("": : :"memory")

#define smp_mb()  asm volatile ("": : :"memory")
#define smp_rmb() smp_mb()
#define smp_wmb() asm volatile ("": : :"memory")

static inline void barrier() { asm volatile ("": : :"memory"); }


#endif /* _ARMV6__PLATFORM__LX_EMUL_BARRIER_H_ */
