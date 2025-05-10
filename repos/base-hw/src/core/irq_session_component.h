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
#include <base/registry.h>
#include <base/rpc_server.h>
#include <irq_session/capability.h>

/* core includes */
#include <irq_args.h>
#include <object.h>
#include <revoke.h>
#include <kernel/irq.h>

namespace Core { class Irq_session_component; }


class Core::Irq_session_component
:
	public Rpc_object<Irq_session>,
	public Revoke

{
	private:

		Registry<Irq_session_component>::Element _elem;

		Irq_args const _args;

		Kernel_object<Kernel::User_irq> _kobj { };

		struct Msi
		{
			addr_t address { }, value { };
			bool const allocated { };

			Msi(Irq_args const &args);
			~Msi();

		} const _msi { _args };

		Signal_context_capability  _sig_cap { };

		Range_allocator::Result const _irq_number;

		Range_allocator::Result _allocate(Range_allocator &irq_alloc) const;

	public:

		/**
		 * Constructor
		 *
		 * \param irq_alloc    platform-dependent IRQ allocator
		 * \param args         session construction arguments
		 */
		Irq_session_component(Registry<Irq_session_component> &registry,
		                      Range_allocator &irq_alloc,
		                      const char *args)
		:
			_elem(registry, *this), _args(args), _irq_number(_allocate(irq_alloc))
		{
			if (_irq_number.failed())
				error("unavailable interrupt ", _args.irq_number(), " requested");
		}

		void revoke_signal_context(Signal_context_capability cap) override;


		/***************************
		 ** Irq session interface **
		 ***************************/

		void ack_irq() override;
		void sigh(Signal_context_capability) override;

		Info info() override
		{
			return _msi.allocated ? Info { .type    = Info::Type::MSI,
			                               .address = _msi.address,
			                               .value   = _msi.value }
			                      : Info { };
		}
};

#endif /* _CORE__IRQ_SESSION_COMPONENT_H_ */
