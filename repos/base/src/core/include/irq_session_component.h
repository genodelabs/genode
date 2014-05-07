/*
 * \brief  Core-specific instance of the IRQ session interface
 * \author Christian Helmuth
 * \date   2007-09-13
 */

/*
 * Copyright (C) 2007-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__IRQ_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__IRQ_SESSION_COMPONENT_H_

#include <base/lock.h>
#include <base/rpc_server.h>
#include <base/rpc_client.h>
#include <util/list.h>
#include <irq_session/irq_session.h>
#include <irq_session/capability.h>

/* XXX Notes
 *
 * - each H/W IRQ is an irq thread
 * - each irq thread has an Rpc_entrypoint
 * -> irq thread is special Server_activation
 * -> IRQ session is Rpc_object
 *
 * - session("IRQ", "irq_num=<num>") -> Native_capability(irq_thread, cap)
 *   - cap must be generated at CAP
 *   - cap must be managed by irq_thread-local Rpc_entrypoint
 *
 * - irq thread states
 *   1. wait_for_client --[ client calls wait_for_irq     ]--> 2.
 *   2. wait_for_irq    --[ kernel signals irq            ]--> 3.
 *   3. irq_occured     --[ inform client about occurence ]--> 1.
 *
 * - irq thread roles
 *   - Irq_server (Ipc_server) for client
 *   - Fiasco_irq_client (Ipc_client) at kernel
 */

namespace Genode {

	class Irq_session_component : public Rpc_object<Irq_session>,
	                              public List<Irq_session_component>::Element
	{
		private:

			struct Irq_control
			{
				GENODE_RPC(Rpc_associate_to_irq, bool, associate_to_irq, unsigned);
				GENODE_RPC_INTERFACE(Rpc_associate_to_irq);
			};

			struct Irq_control_client : Rpc_client<Irq_control>
			{
					Irq_control_client(Capability<Irq_control> cap)
					: Rpc_client<Irq_control>(cap) { }

					bool associate_to_irq(unsigned irq_number) {
						return call<Rpc_associate_to_irq>(irq_number); }
			};

			struct Irq_control_component : Rpc_object<Irq_control,
			                                          Irq_control_component>
			{
				/**
				 * Associate to IRQ at Fiasco
				 *
				 * This is executed by the IRQ server activation itself.
				 */
				bool associate_to_irq(unsigned irq_number);
			};

			unsigned         _irq_number;
			Range_allocator *_irq_alloc;

			enum { STACK_SIZE = 2048 };
			Rpc_entrypoint _ep;

			/*
			 * On Pistachio, an IRQ is unmasked right after attaching.
			 * Hence, the kernel may send an IRQ IPC when the IRQ hander is
			 * not explicitly waiting for an IRQ but when it is waiting for
			 * a client's 'wait_for_irq' RPC call. To avoid this conflict, we
			 * lazily associate to the IRQ when calling the 'wait_for_irq'
			 * function for the first time. We use the '_irq_attached' flag
			 * for detecting the first call. On other kernels, this variable
			 * may be unused.
			 */
			unsigned _irq_attached;  /* true if IRQ is already attached */


			/********************************************
			 ** IRQ control server (internal use only) **
			 ********************************************/

			Irq_control_component   _control_component;  /* ctrl component */
			Capability<Irq_control> _control_cap;        /* capability for ctrl server */
			Irq_control_client      _control_client;     /* ctrl client */
			Capability<Irq_session> _irq_cap;            /* capability for IRQ */

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
			 * If an initialization error occurs, returned _cap is invalid.
			 */
			Capability<Irq_session> cap() const { return _irq_cap; }


			/***************************
			 ** Irq session interface **
			 ***************************/

			void wait_for_irq();
	};
}

#endif /* _CORE__INCLUDE__IRQ_SESSION_COMPONENT_H_ */
