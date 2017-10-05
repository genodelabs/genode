/*
 * \brief  Backend for IRQ sessions served by core
 * \author Martin Stein
 * \date   2013-10-29
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__IRQ_SESSION_COMPONENT_H_
#define _CORE__IRQ_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/rpc_server.h>
#include <util/list.h>
#include <irq_session/capability.h>

#include <kernel/irq.h>

namespace Genode {
	class Irq_session_component;
}

class Genode::Irq_session_component : public Rpc_object<Irq_session>,
	public List<Irq_session_component>::Element
{
	private:

		unsigned         _irq_number;
		Range_allocator *_irq_alloc;
		Genode::uint8_t  _kernel_object[sizeof(Kernel::User_irq)];
		bool             _is_msi;
		addr_t           _address, _value;

		Signal_context_capability _sig_cap;

		unsigned _find_irq_number(const char * const args);

	public:

		/**
		 * Constructor
		 *
		 * \param irq_alloc    platform-dependent IRQ allocator
		 * \param args         session construction arguments
		 */
		Irq_session_component(Range_allocator *irq_alloc,
		                      const char      *args);

		/**
		 * Destructor
		 */
		~Irq_session_component();

		/***************************
		 ** Irq session interface **
		 ***************************/

		void ack_irq() override;
		void sigh(Signal_context_capability) override;

		Info info() override
		{
			if (!_is_msi) {
				return { .type = Info::Type::INVALID };
			}
			return { .type    = Info::Type::MSI,
			         .address = _address,
			         .value   = _value };
		}
};

#endif /* _CORE__IRQ_SESSION_COMPONENT_H_ */
