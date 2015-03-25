/*
 * \brief  IRQ session interface
 * \author Alexander Boettcher
 * \date   2015-03-25
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#pragma once

#include <base/rpc_server.h>

#include <util/list.h>

#include <irq_session/irq_session.h>
#include <irq_session/capability.h>

/* PCI local includes */
#include <platform/irq_proxy.h>


namespace Pci { class Irq_session_component; }


class Pci::Irq_session_component : public Genode::Rpc_object<Genode::Irq_session>,
                                   public Genode::List<Irq_session_component>::Element
{
	private:

		unsigned         _gsi;
		Genode::Irq_sigh _irq_sigh;

	public:

		Irq_session_component(unsigned);
		~Irq_session_component();

		/***************************
		 ** Irq session interface **
		 ***************************/

		void               ack_irq() override;
		void               sigh(Genode::Signal_context_capability) override;
};
