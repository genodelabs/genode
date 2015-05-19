/*
 * \brief  Server-sided implementation of a signal session
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
#include <base/tslab.h>
#include <base/allocator_guard.h>
#include <base/object_pool.h>

/* core includes */
#include <object.h>
#include <kernel/signal_receiver.h>
#include <util.h>

namespace Genode
{
	/**
	 * Server-side implementation of a signal session
	 */
	class Signal_session_component;
}



class Genode::Signal_session_component : public Rpc_object<Signal_session>
{
	private:

		struct Receiver : Kernel_object<Kernel::Signal_receiver>,
		                  Object_pool<Receiver>::Entry
		{
			using Pool = Object_pool<Receiver>;

			Receiver();
		};


		struct Context : Kernel_object<Kernel::Signal_context>,
		                 Object_pool<Context>::Entry
		{
			using Pool = Object_pool<Context>;

			Context(Receiver &rcv, unsigned const imprint);
		};

		template <typename T, size_t BLOCK_SIZE = get_page_size()>
		class Slab : public Tslab<T, BLOCK_SIZE>
		{
			private:

				uint8_t _first_block[BLOCK_SIZE];

			public:

				Slab(Allocator * const allocator)
				: Tslab<T, BLOCK_SIZE>(allocator,
				                       (Slab_block*)&_first_block) { }
		};


		Allocator_guard  _allocator;
		Slab<Receiver>   _receivers_slab;
		Receiver::Pool   _receivers;
		Slab<Context>    _contexts_slab;
		Context::Pool    _contexts;

		/**
		 * Destruct receiver 'r'
		 */
		void _destruct_receiver(Receiver * const r);

		/**
		 * Destruct context 'c'
		 */
		void _destruct_context(Context * const c);

	public:

		/**
		 * Constructor
		 *
		 * \param allocator  RAM allocator for meta data
		 * \param quota      amount of RAM quota donated to this session
		 */
		Signal_session_component(Allocator * const allocator,
		                         size_t const quota);

		~Signal_session_component();

		/**
		 * Raise the quota of this session by 'q'
		 */
		void upgrade_ram_quota(size_t const q) { _allocator.upgrade(q); }


		/******************************
		 ** Signal_session interface **
		 ******************************/

		Signal_receiver_capability alloc_receiver();

		Signal_context_capability
		alloc_context(Signal_receiver_capability, unsigned const);

		void free_receiver(Signal_receiver_capability);

		void free_context(Signal_context_capability);
};

#endif /* _CORE__INCLUDE__SIGNAL_SESSION_COMPONENT_H_ */
