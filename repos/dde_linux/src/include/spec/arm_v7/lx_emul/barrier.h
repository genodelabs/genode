/*
 * \brief  ARMv7-specific part of the Linux API emulation
 * \author Christian Prochaska
 * \date   2014-05-28
 */

/*
 * Copyright (C) 2014-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/*******************
 ** asm/barrier.h **
 *******************/

#define mb()  asm volatile ("dsb": : :"memory")
#define rmb() mb()
#define wmb() asm volatile ("dsb st": : :"memory")

/*
 * This is the "safe" implementation as needed for a configuration
 * with bufferable DMA memory and SMP enabled.
 */

#define smp_mb()  asm volatile ("dmb ish": : :"memory")
#define smp_rmb() smp_mb()
#define smp_wmb() asm volatile ("dmb ishst": : :"memory")

static inline void barrier() { asm volatile ("": : :"memory"); }
