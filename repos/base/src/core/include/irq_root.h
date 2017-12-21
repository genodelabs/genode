/*
 * \brief  IRQ root interface
 * \author Christian Helmuth
 * \author Alexander Boettcher
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
#include <irq_session_component.h>

namespace Genode { class Irq_root; }

class Genode::Irq_root : public Root_component<Irq_session_component>
{

	private:

		/*
		 * Use a dedicated entrypoint for IRQ session to decouple the interrupt
		 * handling from other core services. If we used the same entrypoint, a
		 * long-running operation (like allocating and clearing a dataspace
		 * from the RAM service) would delay the response to time-critical
		 * calls of the 'Irq_session::ack_irq' function.
		 */
		enum { STACK_SIZE = sizeof(long)*1024 };
		Rpc_entrypoint _session_ep;

		Range_allocator *_irq_alloc;    /* platform irq allocator */

		/*
		 * Noncopyable
		 */
		Irq_root(Irq_root const &);
		Irq_root &operator = (Irq_root const &);

	protected:

		Irq_session_component *_create_session(const char *args) {
			return new (md_alloc()) Irq_session_component(_irq_alloc, args); }

	public:

		/**
		 * Constructor
		 *
		 * \param pd_session   capability allocator
		 * \param irq_alloc    IRQ range that can be assigned to clients
		 * \param md_alloc     meta-data allocator to be used by root component
		 */
		Irq_root(Pd_session *pd_session,
		         Range_allocator *irq_alloc, Allocator *md_alloc)
		:
			Root_component<Irq_session_component>(&_session_ep, md_alloc),
			_session_ep(pd_session, STACK_SIZE, "irq"),
			_irq_alloc(irq_alloc)
		{ }
};

#endif /* _CORE__INCLUDE__IRQ_ROOT_H_ */
