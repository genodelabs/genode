/*
 * \brief  IRQ root interface
 * \author Christian Helmuth
 * \author Alexander Boettcher
 * \author Stefan Kalkowski
 * \date   2007-09-13
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__IRQ_ROOT_H_
#define _CORE__INCLUDE__IRQ_ROOT_H_

#include <root/component.h>

/* core includes */
#include <irq_session_component.h>
#include <revoke.h>
#include <platform.h>

namespace Core { class Irq_root; }


class Core::Irq_root : public Root_component<Irq_session_component>,
                       public Revoke
{

	private:

		/*
		 * Use a dedicated entrypoint for IRQ session to decouple the interrupt
		 * handling from other core services. If we used the same entrypoint, a
		 * long-running operation (like allocating and clearing a dataspace
		 * from the RAM service) would delay the response to time-critical
		 * calls of the 'Irq_session::ack_irq' function.
		 */
		Rpc_entrypoint _session_ep;

		Range_allocator &_irq_alloc;    /* platform irq allocator */

		Registry<Irq_session_component> _registry {};

	protected:

		Create_result _create_session(const char *args) override
		{
			return _alloc_obj(_registry, _irq_alloc, args);
		}

	public:

		/**
		 * Constructor
		 *
		 * \param irq_alloc    IRQ range that can be assigned to clients
		 * \param md_alloc     meta-data allocator to be used by root component
		 */
		Irq_root(Runtime &runtime, Range_allocator &irq_alloc,
		         Allocator &md_alloc)
		:
			Root_component<Irq_session_component>(&_session_ep, &md_alloc),
			_session_ep(runtime, "irq", Thread::Stack_size { 8*1024 },
			            Affinity::Location()),
			_irq_alloc(irq_alloc)
		{
			platform_specific().revoke.irq_root = this;
		}

		void revoke_signal_context(Signal_context_capability cap) override
		{
			_registry.for_each([&] (Irq_session_component &component) {
				component.revoke_signal_context(cap); });
		}
};

#endif /* _CORE__INCLUDE__IRQ_ROOT_H_ */
