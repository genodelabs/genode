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
#include <base/slab.h>
#include <base/allocator_guard.h>
#include <base/object_pool.h>

/* core includes */
#include <kernel/signal_receiver.h>
#include <util.h>

namespace Genode
{
	/**
	 * Combines kernel data and core data of an object a signal session manages
	 *
	 * \param T  type of the kernel data
	 */
	template <typename T>
	class Signal_session_object;

	typedef Signal_session_object<Kernel::Signal_receiver>
		Signal_session_receiver;

	typedef Signal_session_object<Kernel::Signal_context>
		Signal_session_context;

	/**
	 * Traits that are used in signal session components
	 *
	 * FIXME: This class is merely necessary because GCC 4.7.2 appears to have
	 *        a problem with using a static-constexpr method for the
	 *        dimensioning of a member array within the same class.
	 */
	class Signal_session_traits;

	/**
	 * Server-sided implementation of a signal session
	 */
	class Signal_session_component;
}

template <typename T>
class Genode::Signal_session_object
:
	public Object_pool<Signal_session_object<T> >::Entry
{
	public:

		typedef Object_pool<Signal_session_object<T> > Pool;

		/**
		 * Constructor
		 */
		Signal_session_object(Untyped_capability cap) : Pool::Entry(cap) { }

		/**
		 * Kernel name of the object
		 */
		unsigned id() const { return Pool::Entry::cap().dst(); }

		/**
		 * Size of the data starting at the base of this object
		 */
		static constexpr size_t size()
		{
			return sizeof(Signal_session_object<T>) + sizeof(T);
		}

		/**
		 * Base of the kernel donation associated with a specific SLAB address
		 *
		 * \param slab_addr  SLAB address
		 */
		static constexpr addr_t kernel_donation(void * const slab_addr)
		{
			return (addr_t)slab_addr + sizeof(Signal_session_object<T>);
		}
};

class Genode::Signal_session_traits
{
	private:

		/**
		 * Return the raw size of a slab
		 */
		static constexpr size_t _slab_raw() { return get_page_size(); }

		/**
		 * Return the size of the static buffer for meta data per slab
		 */
		static constexpr size_t _slab_buffer() { return 128; }

		/**
		 * Return the size available for allocations per slab
		 */
		static constexpr size_t _slab_avail() { return _slab_raw() - _slab_buffer(); }

		/**
		 * Return the amount of allocatable slots per slab
		 *
		 * \param T  object type of the slab
		 */
		template <typename T>
		static constexpr size_t _slab_slots() { return _slab_avail() / T::size(); }

	protected:

		/**
		 * Return the size of allocatable space per slab
		 *
		 * \param T  object type of the slab
		 */
		template <typename T>
		static constexpr size_t _slab_size() { return _slab_slots<T>() * T::size(); }
};

class Genode::Signal_session_component
:
	public Rpc_object<Signal_session>,
	public Signal_session_traits
{
	private:

		typedef Signal_session_receiver Receiver;
		typedef Signal_session_context  Context;
		typedef Signal_session_traits   Traits;

		Allocator_guard _allocator;
		Slab            _receivers_slab;
		Receiver::Pool  _receivers;
		Slab            _contexts_slab;
		Context::Pool   _contexts;

		char _first_receivers_slab [Traits::_slab_size<Receiver>()];
		char _first_contexts_slab  [Traits::_slab_size<Context>()];

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
		                         size_t const quota)
		:
			_allocator(allocator, quota),
			_receivers_slab(Receiver::size(), Traits::_slab_size<Receiver>(),
			                (Slab_block *)&_first_receivers_slab, &_allocator),
			_contexts_slab(Context::size(), Traits::_slab_size<Context>(),
			               (Slab_block *)&_first_contexts_slab, &_allocator)
		{ }

		/**
		 * Destructor
		 */
		~Signal_session_component()
		{
			while (1) {
				Context * const c = _contexts.first_locked();
				if (!c) { break; }
				_destruct_context(c);
			}
			while (1) {
				Receiver * const r = _receivers.first_locked();
				if (!r) { break; }
				_destruct_receiver(r);
			}
		}

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
