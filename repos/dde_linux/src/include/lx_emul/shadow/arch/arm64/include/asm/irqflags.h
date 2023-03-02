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

static inline void arch_local_irq_enable(void)
{
	lx_emul_irq_enable();
}

static inline void arch_local_irq_disable(void)
{
	lx_emul_irq_disable();
}

static inline unsigned long arch_local_save_flags(void)
{
	return lx_emul_irq_state();
}

static inline int arch_irqs_disabled_flags(unsigned long flags)
{
	return flags & 1;
}

static inline unsigned long arch_local_irq_save(void)
{
	return lx_emul_irq_disable();
}

static inline void arch_local_irq_restore(unsigned long flags)
{
	lx_emul_irq_restore(flags);
}

#endif
