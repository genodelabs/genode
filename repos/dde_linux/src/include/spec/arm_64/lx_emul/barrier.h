/*
 * \brief  ARMv8-specific part of the Linux API emulation
 * \author Christian Prochaska
 * \date   2014-05-28
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/*******************
 ** asm/barrier.h **
 *******************/

#define mb()  asm volatile ("dsb ld": : :"memory")
#define rmb() mb()
#define wmb() asm volatile ("dsb st": : :"memory")

#define dma_wmb() __asm__ __volatile__ ("dmb oshst" : : : "memory")
#define dma_rmb() __asm__ __volatile__ ("dmb oshld" : : : "memory")

/*
 * This is the "safe" implementation as needed for a configuration
 * with bufferable DMA memory and SMP enabled.
 */

#define smp_mb()  asm volatile ("dmb ish": : :"memory")
#define smp_rmb() smp_mb()
#define smp_wmb() asm volatile ("dmb ishst": : :"memory")

static inline void barrier() { asm volatile ("": : :"memory"); }
