/*
 * \brief  Signal service on the HW-core
 * \author Martin stein
 * \date   2012-05-05
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__SIGNAL_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__SIGNAL_SESSION_COMPONENT_H_

/* Genode includes */
#include <signal_session/signal_session.h>
#include <base/rpc_server.h>
#include <base/slab.h>
#include <base/allocator_guard.h>
#include <base/object_pool.h>

namespace Genode
{
	/**
	 * Provides the signal service
	 */
	class Signal_session_component : public Rpc_object<Signal_session>
	{
		public:

			enum {
				RECEIVERS_SB_SIZE = 4096,
				CONTEXTS_SB_SIZE  = 4096,
			};

		private:

			/**
			 * Maps a signal-receiver name to related core and kernel resources
			 */
			class Receiver;

			/**
			 * Maps a signal-context name to related core and kernel resources
			 */
			class Context;

			typedef Object_pool<Receiver> Receiver_pool;
			typedef Object_pool<Context>  Context_pool;

			Allocator_guard _md_alloc;
			Slab            _receivers_slab;
			Receiver_pool   _receivers;
			Slab            _contexts_slab;
			Context_pool    _contexts;
			char            _initial_receivers_sb [RECEIVERS_SB_SIZE];
			char            _initial_contexts_sb  [CONTEXTS_SB_SIZE];

		public:

			/**
			 * Constructor
			 *
			 * \param md         Metadata allocator
			 * \param ram_quota  Amount of RAM quota donated to this session
			 */
			Signal_session_component(Allocator * const md,
			                         size_t const ram_quota);

			/**
			 * Destructor
			 */
			~Signal_session_component();

			/**
			 * Raise the quota of this session by 'q'
			 */
			void upgrade_ram_quota(size_t const q) { _md_alloc.upgrade(q); }

			/******************************
			 ** Signal_session interface **
			 ******************************/

			Signal_receiver_capability alloc_receiver();

			Signal_context_capability
			alloc_context(Signal_receiver_capability, unsigned const);

			void free_receiver(Signal_receiver_capability);

			void free_context(Signal_context_capability);
	};
}

class Genode::Signal_session_component::Receiver : public Receiver_pool::Entry
{
	public:

		/**
		 * Constructor
		 */
		Receiver(Untyped_capability cap) : Entry(cap) { }

		/**
		 * Name of signal receiver
		 */
		unsigned id() const { return Receiver_pool::Entry::cap().dst(); }

		/**
		 * Size of SLAB block occupied by resources and this resource info
		 */
		static size_t slab_size()
		{
			return sizeof(Receiver) + Kernel::signal_receiver_size();
		}

		/**
		 * Base of region donated to the kernel
		 */
		static addr_t kernel_donation(void * const slab_addr)
		{
			return ((addr_t)slab_addr + sizeof(Receiver));
		}
};

class Genode::Signal_session_component::Context : public Context_pool::Entry
{
	public:

		/**
		 * Constructor
		 */
		Context(Untyped_capability cap) : Entry(cap) { }

		/**
		 * Name of signal context
		 */
		unsigned id() const { return Context_pool::Entry::cap().dst(); }

		/**
		 * Size of SLAB block occupied by resources and this resource info
		 */
		static size_t slab_size()
		{
			return sizeof(Context) + Kernel::signal_context_size();
		}

		/**
		 * Base of region donated to the kernel
		 */
		static addr_t kernel_donation(void * const slab_addr)
		{
			return ((addr_t)slab_addr + sizeof(Context));
		}
};

#endif /* _CORE__INCLUDE__CAP_SESSION_COMPONENT_H_ */
