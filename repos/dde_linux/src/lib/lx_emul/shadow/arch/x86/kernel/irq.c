/*
 * \brief  Replaces arch/x86/kernel/irq.c
 * \author Stefan Kalkowski
 * \date   2022-07-20
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2 or later.
 */

#include <linux/irq.h>
#include <asm/hardirq.h>

DEFINE_PER_CPU_SHARED_ALIGNED(irq_cpustat_t, irq_stat);
EXPORT_PER_CPU_SYMBOL(irq_stat);

#if LINUX_VERSION_CODE > KERNEL_VERSION(6,13,0)
DEFINE_PER_CPU_CACHE_HOT(u16, __softirq_pending);
EXPORT_PER_CPU_SYMBOL(__softirq_pending);
#endif
