/*
 * \brief  Backend for IRQ sessions served by core
 * \author Martin Stein
 * \date   2013-10-29
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__IRQ_SESSION_COMPONENT_H_
#define _INCLUDE__IRQ_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/rpc_server.h>
#include <util/list.h>
#include <irq_session/capability.h>

namespace Genode
{
	/**
	 * Backend for IRQ sessions to core
	 */
	class Irq_session_component
	:
		public Rpc_object<Irq_session, Irq_session_component>,
		public List<Irq_session_component>::Element
	{
		private:

			Range_allocator * const _irq_alloc;
			Irq_session_capability  _cap;
			Irq_signal              _signal;

		public:

			/**
			 * Constructor
			 *
			 * \param cap_session  capability session to use
			 * \param irq_alloc    interrupt allocator
			 * \param args         session construction arguments
			 */
			Irq_session_component(Cap_session     * const cap_session,
			                      Range_allocator * const irq_alloc,
			                      const char      * const args);

			/**
			 * Destructor
			 */
			~Irq_session_component();

			/**
			 * Return capability to this session
			 *
			 * If an initialization error occurs, returned _cap is invalid.
			 */
			Irq_session_capability cap() const { return _cap; }


			/*****************
			 ** Irq_session **
			 *****************/

			void wait_for_irq();

			Irq_signal signal();
	};
}

#endif /* _INCLUDE__IRQ_SESSION_COMPONENT_H_ */
