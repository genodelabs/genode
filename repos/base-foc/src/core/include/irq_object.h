/*
 * \brief  Fiasco.OC-specific core implementation of IRQ sessions
 * \author Christian Helmuth
 * \author Stefan Kalkowski
 * \author Sebastian Sumpf
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

/* Genode includes */
#include <irq_session/irq_session.h>
#include <cap_index.h>

/* core includes */
#include <types.h>

namespace Core { class Irq_object; }


class Core::Irq_object
{
	private:

		/*
		 * Noncopyable
		 */
		Irq_object(Irq_object const &);
		Irq_object &operator = (Irq_object const &);

		Cap_index             *_cap;
		Irq_session::Trigger   _trigger;  /* interrupt trigger */
		Irq_session::Polarity  _polarity; /* interrupt polarity */

		unsigned               _irq;
		uint64_t               _msi_addr;
		addr_t                 _msi_data;

		Signal_context_capability _sig_cap { };

		Foc::l4_cap_idx_t _capability() const { return _cap->kcap(); }

	public:

		Irq_object();
		~Irq_object();

		Irq_session::Trigger  trigger()  const { return _trigger; }
		Irq_session::Polarity polarity() const { return _polarity; }

		uint64_t msi_address() const { return _msi_addr; }
		addr_t   msi_value()   const { return _msi_data; }

		void sigh(Signal_context_capability cap) { _sig_cap = cap; }
		void notify() { Signal_transmitter(_sig_cap).submit(1); }

		bool associate(unsigned irq, bool msi, Irq_session::Trigger,
		               Irq_session::Polarity);
		void ack_irq();
};

#endif /* _CORE__INCLUDE__IRQ_OBJECT_H_ */
