/*
 * \brief  Replaces kernel/cpu.c
 * \author Stefan Kalkowski
 * \date   2021-03-16
 *
 * We hardcode support for a single cpu only.
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/cpu.h>
#include <linux/cpumask.h>

atomic_t __num_online_cpus = ATOMIC_INIT(1);
struct cpumask __cpu_online_mask   = { .bits[0] = 1 };
struct cpumask __cpu_possible_mask = { .bits[0] = 1 };
struct cpumask __cpu_present_mask  = { .bits[0] = 1 };


#ifdef CONFIG_HOTPLUG_CPU
void cpus_read_lock(void) { }
void cpus_read_unlock(void) { }
void lockdep_assert_cpus_held(void) { }
#endif /* CONFIG_HOTPLUG_CPU */

#define MASK_DECLARE_1(x) [x+1][0] = (1UL << (x))
#define MASK_DECLARE_2(x) MASK_DECLARE_1(x), MASK_DECLARE_1(x+1)
#define MASK_DECLARE_4(x) MASK_DECLARE_2(x), MASK_DECLARE_2(x+2)
#define MASK_DECLARE_8(x) MASK_DECLARE_4(x), MASK_DECLARE_4(x+4)

const unsigned long cpu_bit_bitmap[BITS_PER_LONG+1][BITS_TO_LONGS(NR_CPUS)] = {
	MASK_DECLARE_8(0),  MASK_DECLARE_8(8),
	MASK_DECLARE_8(16), MASK_DECLARE_8(24),
#if BITS_PER_LONG > 32
	MASK_DECLARE_8(32), MASK_DECLARE_8(40),
	MASK_DECLARE_8(48), MASK_DECLARE_8(56),
#endif
};

const DECLARE_BITMAP(cpu_all_bits, NR_CPUS) = CPU_BITS_ALL;
EXPORT_SYMBOL(cpu_all_bits);


/*
 * Provide this init function as a weak symbol so that
 * drivers including 'drivers/base/auxiliary.c' in their
 * source list can override it but all other drivers are
 * indifferent to its existence.
 */
void auxiliary_bus_init(void) __attribute__((weak));
void auxiliary_bus_init(void) { }
