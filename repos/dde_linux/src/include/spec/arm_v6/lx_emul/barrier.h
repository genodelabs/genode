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

/*******************
 ** asm/barrier.h **
 *******************/

#define mb()  asm volatile ("": : :"memory")
#define rmb() mb()
#define wmb() asm volatile ("": : :"memory")

/*
 * This is the "safe" implementation as needed for a configuration
 * with SMP enabled.
 */

#define smp_mb()  asm volatile ("": : :"memory")
#define smp_rmb() smp_mb()
#define smp_wmb() asm volatile ("": : :"memory")

static inline void barrier() { asm volatile ("": : :"memory"); }
