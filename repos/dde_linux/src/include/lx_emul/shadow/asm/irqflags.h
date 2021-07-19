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

static inline void arch_local_irq_enable(void)
{
}

static inline void arch_local_irq_disable(void)
{
}

static inline unsigned long arch_local_save_flags(void)
{
	return 1;
}

static inline int arch_irqs_disabled_flags(unsigned long flags)
{
	return flags;
}

static inline unsigned long arch_local_irq_save(void)
{
	return 1;
}

static inline void arch_local_irq_restore(unsigned long flags)
{
}

#endif
