/*
 * \brief  Core-specific instance of the IRQ session interface
 * \author Christian Helmuth
 * \date   2007-09-13
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__IRQ_OBJECT_H_
#define _CORE__INCLUDE__IRQ_OBJECT_H_

#include <base/thread.h>

namespace Genode { class Irq_object; }

class Genode::Irq_object : public Thread_deprecated<4096> {

	private:

		Signal_context_capability _sig_cap { };
		Lock                      _sync_ack;
		Lock                      _sync_bootup;
		unsigned                  _irq;

		bool _associate();
		void _wait_for_irq();

		void entry() override;

	public:

		Irq_object(unsigned irq);

		void sigh(Signal_context_capability cap) { _sig_cap = cap; }
		void ack_irq() { _sync_ack.unlock(); }

		void start() override;
};

#endif /* _CORE__INCLUDE__IRQ_OBJECT_H_ */
