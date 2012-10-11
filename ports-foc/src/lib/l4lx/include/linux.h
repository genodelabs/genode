/*
 * \brief  Linux kernel functions
 * \author Stefan Kalkowski
 * \date   2011-04-29
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _L4LX__LINUX_H_
#define _L4LX__LINUX_H_

#include <base/printf.h>

#include <genode/linkage.h>

#ifdef __cplusplus
extern "C" {
#endif

FASTCALL void l4x_irq_save(unsigned long *flags);
FASTCALL void l4x_irq_restore(unsigned long flags);
FASTCALL void l4x_migrate_lock(unsigned long *flags);
FASTCALL void l4x_migrate_unlock(unsigned long flags);
FASTCALL unsigned long l4x_hz();
FASTCALL int l4x_nr_irqs(void);

FASTCALL unsigned l4x_cpu_physmap_get_id(unsigned);
FASTCALL unsigned l4x_target_cpu(const struct cpumask*);
FASTCALL void     l4x_cpumask_copy(struct irq_data*, const struct cpumask*);

#define IRQ_SAFE(x) do { \
	unsigned long flags = 0; \
	l4x_irq_save(&flags); \
	x; \
	l4x_irq_restore(flags); \
} while(0)

#define NOT_IMPLEMENTED IRQ_SAFE(PWRN("%s: not implemented yet!", __func__););

#ifdef __cplusplus
}
#endif

namespace Linux {

	class Irq_guard
	{
		private:

			unsigned long _flags;

		public:

			Irq_guard() : _flags(0) { l4x_irq_save(&_flags);   }
			~Irq_guard()            { l4x_irq_restore(_flags); }
	};
}
#endif /* _L4LX__LINUX_H_ */
