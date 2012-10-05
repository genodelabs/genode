/*
 * \brief  IRQ session interface
 * \author Christian Helmuth
 * \date   2007-09-13
 *
 * An open IRQ session represents a valid IRQ attachment/association.
 * Initially, the interrupt is masked and will only occur if enabled. This is
 * done by calling wait_for_irq(). When the interrupt is delivered to the
 * client, it was acknowledged and masked at the interrupt controller before.
 *
 * Disassociation from an IRQ is done by closing the session.
 */

/*
 * Copyright (C) 2007-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__IRQ_SESSION__IRQ_SESSION_H_
#define _INCLUDE__IRQ_SESSION__IRQ_SESSION_H_

#include <base/capability.h>
#include <session/session.h>

namespace Genode {

	struct Irq_session : Session
	{

		/**
		 * Interrupt trigger
		 */
		enum Trigger { TRIGGER_UNCHANGED = 0, TRIGGER_LEVEL, TRIGGER_EDGE };

		/**
		 * Interrupt trigger polarity
		 */
		enum Polarity { POLARITY_UNCHANGED = 0, POLARITY_HIGH, POLARITY_LOW };

		static const char *service_name() { return "IRQ"; }

		virtual ~Irq_session() { }

		virtual void wait_for_irq() = 0;


		/*********************
		 ** RPC declaration **
		 *********************/

		GENODE_RPC(Rpc_wait_for_irq, void, wait_for_irq);
		GENODE_RPC_INTERFACE(Rpc_wait_for_irq);
	};
}

#endif /* _INCLUDE__IRQ_SESSION__IRQ_SESSION_H_ */
