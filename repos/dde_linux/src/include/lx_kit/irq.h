/*
 * \brief  Signal context for IRQ's
 * \author Josef Soentgen
 * \author Christian Helmuth
 * \author Stefan Kalkowski
 * \date   2014-10-14
 */

/*
 * Copyright (C) 2014-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LX_KIT__IRQ_H_
#define _LX_KIT__IRQ_H_

/* Genode includes */
#include <os/server.h>
#include <platform_device/platform_device.h>


namespace Lx { class Irq; }

class Lx::Irq
{
	public:

		static Irq &irq(Server::Entrypoint *ep    = nullptr,
		                Genode::Allocator  *alloc = nullptr);

		/**
		 * Request an IRQ
		 */
		virtual void request_irq(Platform::Device &dev, irq_handler_t handler,
		                         void *dev_id, irq_handler_t thread_fn = 0) = 0;

		virtual void inject_irq(Platform::Device &dev) = 0;
};

#endif /* _LX_KIT__IRQ_H_ */
