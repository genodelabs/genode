/*
 * \brief  Core-specific instance of the IRQ session interface for Linux
 * \author Christian Helmuth
 * \date   2007-09-13
 */

/*
 * Copyright (C) 2007-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__LINUX__IRQ_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__LINUX__IRQ_SESSION_COMPONENT_H_

#include <util/list.h>
#include <base/lock.h>
#include <base/rpc_server.h>
#include <irq_session/irq_session.h>

namespace Genode {

	class Irq_session_component : public List<Irq_session_component>::Element
	{
		public:

			/**
			 * Constructor
			 *
			 * \param cap_session  capability session to use
			 * \param irq_alloc    platform-dependent IRQ allocator
			 * \param args         session construction arguments
			 */
			Irq_session_component(Cap_session     *cap_session,
			                      Range_allocator *irq_alloc,
			                      const char      *args) { }

			/**
			 * Destructor
			 */
			~Irq_session_component() { }

			/**
			 * Return capability to this session
			 *
			 * Capability is always invalid under Linux.
			 */
			Session_capability cap() const { return Session_capability(); }


			/***************************
			 ** Irq session interface **
			 ***************************/

			void wait_for_irq() { }
	};
}

#endif /* _CORE__INCLUDE__LINUX__IRQ_SESSION_COMPONENT_H_ */
