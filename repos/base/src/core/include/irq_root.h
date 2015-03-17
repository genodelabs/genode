/*
 * \brief  IRQ root interface
 * \author Christian Helmuth
 * \author Alexander Boettcher
 * \date   2007-09-13
 */

/*
 * Copyright (C) 2007-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__IRQ_ROOT_H_
#define _CORE__INCLUDE__IRQ_ROOT_H_

#include <root/component.h>
#include <irq_session_component.h>

namespace Genode { class Irq_root; }

class Genode::Irq_root : public Root_component<Irq_session_component>
{

	private:

		Range_allocator            *_irq_alloc;    /* platform irq allocator */

	protected:

		Irq_session_component *_create_session(const char *args) {
			return new (md_alloc()) Irq_session_component(_irq_alloc, args); }

	public:

		/**
		 * Constructor
		 *
		 * \param session_ep  entry point for managing irq session objects
		 * \param irq_alloc   IRQ range that can be assigned to clients
		 * \param md_alloc    meta-data allocator to be used by root component
		 */
		Irq_root(Rpc_entrypoint *session_ep, Range_allocator *irq_alloc,
		         Allocator *md_alloc)
		:
		  Root_component<Irq_session_component>(session_ep, md_alloc),
		  _irq_alloc(irq_alloc) { }
};

#endif /* _CORE__INCLUDE__IRQ_ROOT_H_ */
