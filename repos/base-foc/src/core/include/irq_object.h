/*
 * \brief  Fiasco.OC-specific core implementation of IRQ sessions
 * \author Christian Helmuth
 * \author Stefan Kalkowski
 * \author Sebastian Sumpf
 * \date   2007-09-13
 */

/*
 * Copyright (C) 2007-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__IRQ_OBJECT_H_
#define _CORE__INCLUDE__IRQ_OBJECT_H_

/* Genode includes */
#include <irq_session/irq_session.h>
#include <cap_index.h>

namespace Genode { class Irq_object; }


class Genode::Irq_object
{
	private:

		Cap_index             *_cap;
		Irq_session::Trigger   _trigger;  /* interrupt trigger */
		Irq_session::Polarity  _polarity; /* interrupt polarity */

		unsigned               _irq;
		Genode::addr_t         _msi_addr;
		Genode::addr_t         _msi_data;

		Signal_context_capability _sig_cap;

		Fiasco::l4_cap_idx_t _capability() const { return _cap->kcap(); }

	public:

		Irq_object();
		~Irq_object();

		Irq_session::Trigger  trigger()  const { return _trigger; }
		Irq_session::Polarity polarity() const { return _polarity; }

		Genode::addr_t msi_address() const { return _msi_addr; }
		Genode::addr_t msi_value()   const { return _msi_data; }

		void sigh(Genode::Signal_context_capability cap) { _sig_cap = cap; }
		void notify() { Genode::Signal_transmitter(_sig_cap).submit(1); }

		bool associate(unsigned irq, bool msi, Irq_session::Trigger,
		               Irq_session::Polarity);
		void ack_irq();
};

#endif /* _CORE__INCLUDE__IRQ_OBJECT_H_ */
