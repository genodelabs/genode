/*
 * \brief  Signal context for IRQ's
 * \author Josef Soentgen
 * \author Christian Helmuth
 * \author Stefan Kalkowski
 * \date   2014-10-14
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_KIT__IRQ_H_
#define _LX_KIT__IRQ_H_

/* Genode includes */
#include <base/allocator.h>
#include <irq_session/capability.h>

namespace Lx { class Irq; }


class Lx::Irq
{
	public:

		static Irq &irq(Genode::Entrypoint *ep    = nullptr,
		                Genode::Allocator  *alloc = nullptr);

		/**
		 * Request an IRQ
		 */
		virtual void request_irq(Genode::Irq_session_capability cap,
		                         unsigned int irq,
		                         irq_handler_t handler, void *dev_id,
		                         irq_handler_t thread_fn = 0) = 0;

		virtual void inject_irq(unsigned int irq) = 0;

		/**
		 * Disable an IRQ
		 */
		virtual void disable_irq(unsigned int irq) = 0;

		/**
		 * Enable an IRQ
		 */
		virtual void enable_irq(unsigned int irq) = 0;
};

#endif /* _LX_KIT__IRQ_H_ */
