/*
 * \brief  IRQ session interface for Fiasco.OC
 * \author Stefan Kalkowski
 * \date   2011-01-28
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__IRQ_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__IRQ_SESSION_COMPONENT_H_

#include <base/lock.h>
#include <base/semaphore.h>
#include <base/rpc_server.h>
#include <util/list.h>

#include <irq_session/capability.h>

namespace Genode {

	class Irq_proxy_component;

	class Irq_session_component : public Rpc_object<Irq_session>,
	                              public List<Irq_session_component>::Element
	{
		private:


			/*
			 * Each IRQ session uses a dedicated server activation
			 */
			enum { STACK_SIZE = 2048 };
			Rpc_entrypoint           _ep;

			Irq_session_capability   _irq_cap;
			Irq_proxy_component     *_proxy;

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
			                      const char      *args);

			/**
			 * Destructor
			 */
			~Irq_session_component();

			/**
			 * Return capability to this session
			 *
			 * If an initialization error occurs, returned capability is invalid.
			 */
			Irq_session_capability cap() const { return _irq_cap; }


			/***************************
			 ** Irq session interface **
			 ***************************/

			void wait_for_irq();
	};
}

#endif /* _CORE__INCLUDE__IRQ_SESSION_COMPONENT_H_ */
