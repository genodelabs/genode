/*
 * \brief  Shadows Linux kernel asm/irqflags.h
 * \author Stefan Kalkowski
 * \date   2021-04-14
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef __ASM_IRQFLAGS_H
#define __ASM_IRQFLAGS_H

#include <lx_emul/irq.h>

#define arch_local_save_flags arch_local_save_flags
static inline unsigned long arch_local_save_flags(void)
{
	return lx_emul_irq_state();
}

#define arch_local_irq_restore arch_local_irq_restore
static inline void arch_local_irq_restore(unsigned long flags)
{
	lx_emul_irq_restore(flags);
}

#define arch_irqs_disabled_flags arch_irqs_disabled_flags
static inline int arch_irqs_disabled_flags(unsigned long flags)
{
	return flags & 1;
}

#define local_fiq_enable() ({})
#define local_fiq_disable() ({})

#include <asm-generic/irqflags.h>

#endif
