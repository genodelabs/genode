/*
 * \brief  ARMv6-specific part of the Linux API emulation
 * \author Christian Prochaska
 * \date   2014-05-28
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/*******************************************
 ** source/arch/arm/include/asm/barrier.h **
 *******************************************/

#define dsb() asm volatile ("mcr p15, 0, %0, c7, c10, 4": : "r" (0) : "memory")
#define dmb() asm volatile ("mcr p15, 0, %0, c7, c10, 5": : "r" (0) : "memory")

#define mb()  dsb()
#define rmb() mb()
#define wmb() dsb()

#define dma_wmb() dmb()
#define dma_rmb() dmb()

/*
 * This is the "safe" implementation as needed for a configuration
 * with SMP enabled.
 */

#define smp_mb()  dmb()
#define smp_rmb() smp_mb()
#define smp_wmb() dmb()

static inline void barrier() { asm volatile ("": : :"memory"); }
