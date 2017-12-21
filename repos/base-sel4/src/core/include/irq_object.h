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
#include <base/internal/capability_space_sel4.h>
#include <irq_session/irq_session.h>

namespace Genode { class Irq_object; }

class Genode::Irq_object : public Thread_deprecated<4096> {

	private:

		Signal_context_capability _sig_cap { };
		Lock                      _sync_bootup;
		unsigned                  _irq;
		Cap_sel                   _kernel_irq_sel;
		Cap_sel                   _kernel_notify_sel;

		void _wait_for_irq();

		void entry() override;

		long _associate(Irq_session::Trigger const &irq_trigger,
		                Irq_session::Polarity const &irq_polarity);

	public:

		Irq_object(unsigned irq);

		void sigh(Signal_context_capability cap) { _sig_cap = cap; }
		void notify() { Genode::Signal_transmitter(_sig_cap).submit(1); }
		void ack_irq();

		void start() override;
		bool associate(Irq_session::Trigger const, Irq_session::Polarity const);
};

#endif /* _CORE__INCLUDE__IRQ_OBJECT_H_ */
