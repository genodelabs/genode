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

#ifndef _CORE__INCLUDE__IRQ_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__IRQ_SESSION_COMPONENT_H_

#include <base/lock.h>
#include <base/rpc_server.h>
#include <base/rpc_client.h>
#include <util/list.h>
#include <irq_session/irq_session.h>
#include <irq_session/capability.h>

#include <irq_object.h>

namespace Genode { class Irq_session_component; }

class Genode::Irq_session_component : public  Rpc_object<Irq_session>,
                                      private List<Irq_session_component>::Element
{
	private:

		friend class List<Irq_session_component>;

		unsigned         _irq_number;
		Range_allocator &_irq_alloc;
		Irq_object       _irq_object;

	public:

		/**
		 * Constructor
		 *
		 * \param irq_alloc    platform-dependent IRQ allocator
		 * \param args         session construction arguments
		 */
		Irq_session_component(Range_allocator &irq_alloc, const char *args);

		/**
		 * Destructor
		 */
		~Irq_session_component();


		/***************************
		 ** Irq session interface **
		 ***************************/

		void ack_irq() override;
		void sigh(Signal_context_capability) override;
		Info info() override;
};

#endif /* _CORE__INCLUDE__IRQ_SESSION_COMPONENT_H_ */
