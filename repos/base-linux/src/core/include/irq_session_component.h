/*
 * \brief  Core-specific instance of the IRQ session interface for Linux
 * \author Christian Helmuth
 * \date   2007-09-13
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__IRQ_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__IRQ_SESSION_COMPONENT_H_

#include <util/list.h>
#include <base/rpc_server.h>
#include <irq_session/irq_session.h>

namespace Genode {
	class Irq_session_component;
}

class Genode::Irq_session_component : public Rpc_object<Irq_session>,
                                      public List<Irq_session_component>::Element
{
	public:

		/**
		 * Constructor
		 *
		 * \param irq_alloc    platform-dependent IRQ allocator
		 * \param args         session construction arguments
		 */
		Irq_session_component(Range_allocator *irq_alloc,
		                      const char      *args) { }

		/**
		 * Destructor
		 */
		~Irq_session_component() { }

		/***************************
		 ** Irq session interface **
		 ***************************/

		void ack_irq() override { }
		void sigh(Signal_context_capability) override { }
		Info info() override { 
			return { .type = Genode::Irq_session::Info::Type::INVALID }; }
};

#endif /* _CORE__INCLUDE__IRQ_SESSION_COMPONENT_H_ */
