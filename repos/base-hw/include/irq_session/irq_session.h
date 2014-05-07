/*
 * \brief  IRQ session interface
 * \author Martin Stein
 * \date   2013-10-24
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _IRQ_SESSION__IRQ_SESSION_H_
#define _IRQ_SESSION__IRQ_SESSION_H_

/* Genode includes */
#include <base/capability.h>
#include <session/session.h>

namespace Genode
{
	/**
	 * Information that enables a user to await and ack an IRQ directly
	 */
	struct Irq_signal
	{
		unsigned receiver_id;
		unsigned context_id;
	};

	/**
	 * IRQ session interface
	 */
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

		/**
		 * Destructor
		 */
		virtual ~Irq_session() { }

		/**
		 * Await the next occurence of the interrupt of this session
		 */
		virtual void wait_for_irq() = 0;

		/**
		 * Get information for direct interrupt handling
		 *
		 * FIXME: This is used only client-internal and could thus be protected.
		 */
		virtual Irq_signal signal() = 0;


		/*************
		 ** Session **
		 *************/

		static const char * service_name() { return "IRQ"; }


		/*********************
		 ** RPC declaration **
		 *********************/

		GENODE_RPC(Rpc_wait_for_irq, void, wait_for_irq);
		GENODE_RPC(Rpc_signal, Irq_signal, signal);
		GENODE_RPC_INTERFACE(Rpc_wait_for_irq, Rpc_signal);
	};
}

#endif /* _IRQ_SESSION__IRQ_SESSION_H_ */
