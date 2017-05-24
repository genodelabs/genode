/*
 * \brief  IRQ session interface
 * \author Christian Helmuth
 * \author Martin Stein
 * \date   2007-09-13
 *
 * An open IRQ session represents a valid IRQ attachment/association.
 * Initially, the interrupt is masked and will only occur if enabled. This is
 * done by calling ack_irq().
 *
 * Disassociation from an IRQ is done by closing the session.
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__IRQ_SESSION__IRQ_SESSION_H_
#define _INCLUDE__IRQ_SESSION__IRQ_SESSION_H_

#include <base/signal.h>
#include <base/capability.h>
#include <session/session.h>

namespace Genode {
	struct Irq_session;
}


struct Genode::Irq_session : Session
{
	struct Info {
		enum Type { INVALID, MSI } type;
		unsigned long address;
		unsigned long value;
	};

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
	 * Acknowledge handling of last interrupt - re-enables interrupt reception
	 */
	virtual void ack_irq() = 0;

	/**
	 * Register irq signal handler
	 */
	virtual void sigh(Genode::Signal_context_capability sigh) = 0;

	/**
	 * Request information about IRQ, e.g. on x86 request MSI address and
	 * MSI value to be programmed to device specific PCI registers.
	 */
	virtual Info info() = 0;

	/*************
	 ** Session **
	 *************/

	/**
	 * \noapi
	 */
	static const char * service_name() { return "IRQ"; }

	enum { CAP_QUOTA = 3 };


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC(Rpc_ack_irq, void, ack_irq);
	GENODE_RPC(Rpc_sigh, void, sigh, Genode::Signal_context_capability);
	GENODE_RPC(Rpc_info, Info, info);
	GENODE_RPC_INTERFACE(Rpc_ack_irq, Rpc_sigh, Rpc_info);
};


namespace Genode {

	static inline void print(Output &out, Irq_session::Trigger value)
	{
		switch (value) {
		case Irq_session::TRIGGER_UNCHANGED: print(out, "UNCHANGED"); break;
		case Irq_session::TRIGGER_LEVEL:     print(out, "LEVEL");     break;
		case Irq_session::TRIGGER_EDGE:      print(out, "EDGE");      break;
		}
	}

	static inline void print(Output &out, Irq_session::Polarity value)
	{
		switch (value) {
		case Irq_session::POLARITY_UNCHANGED: print(out, "UNCHANGED"); break;
		case Irq_session::POLARITY_HIGH:      print(out, "HIGH");      break;
		case Irq_session::POLARITY_LOW:       print(out, "LOW");       break;
		}
	}
}

#endif /* _INCLUDE__IRQ_SESSION__IRQ_SESSION_H_ */
