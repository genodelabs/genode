/*
 * \brief  Paging-server framework
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2006-04-28
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__PAGER_H_
#define _INCLUDE__BASE__PAGER_H_

#include <base/thread.h>
#include <base/thread_state.h>
#include <base/errno.h>
#include <base/ipc_pager.h>
#include <base/printf.h>
#include <base/object_pool.h>
#include <base/signal.h>
#include <cap_session/cap_session.h>
#include <pager/capability.h>

namespace Genode {

	/**
	 * Special server object for paging
	 *
	 * A 'Pager_object' is very similar to a 'Rpc_object'. It is just a
	 * special implementation for page-fault handling, which does not allow to
	 * define a "badge" for pager capabilities.
	 */
	class Pager_object : public Object_pool<Pager_object>::Entry
	{
		protected:

			/**
			 * Local name for this pager object
			 */
			unsigned long _badge;

			Thread_capability _thread_cap;

			/**
			 * User-level signal handler registered for this pager object via
			 * 'Cpu_session::exception_handler()'.
			 */
			Signal_context_capability _exception_sigh;

		public:

			/**
			 * Contains information about exception state of corresponding thread.
			 */
			Thread_state state;

			/**
			 * Constructor
			 *
			 * \param location  affinity of paged thread to physical CPU
			 */
			Pager_object(unsigned long badge, Affinity::Location location)
			: _badge(badge) { }

			virtual ~Pager_object() { }

			unsigned long badge() const { return _badge; }

			/**
			 * Interface to be implemented by a derived class
			 *
			 * \param ps  'Ipc_pager' stream
			 *
			 * Returns !0 on error and pagefault will not be answered.
			 */
			virtual int pager(Ipc_pager &ps) = 0;

			/**
			 * Wake up the faulter
			 */
			void wake_up();

			/**
			 * Assign user-level exception handler for the pager object
			 */
			void exception_handler(Signal_context_capability sigh)
			{
				_exception_sigh = sigh;
			}

			/**
			 * Notify exception handler about the occurrence of an exception
			 */
			void submit_exception_signal()
			{
				if (!_exception_sigh.valid()) return;

				Signal_transmitter transmitter(_exception_sigh);
				transmitter.submit();
			}

			/**
			 * Remember thread cap so that rm_session can tell thread that
			 * rm_client is gone.
			 */
			Thread_capability thread_cap() { return _thread_cap; } const
			void thread_cap(Thread_capability cap) { _thread_cap = cap; }
	};

	/**
	 * A 'Pager_activation' processes one page fault of a 'Pager_object' at a time.
	 */
	class Pager_entrypoint;
	class Pager_activation_base: public Thread_base
	{
		private:

			Native_capability _cap;
			Pager_entrypoint *_ep;       /* entry point to which the
			                                activation belongs */
			/**
			 * Lock used for blocking until '_cap' is initialized
			 */
			Lock _cap_valid;

		public:

			Pager_activation_base(const char *name, size_t stack_size) :
				Thread_base(name, stack_size),
				_cap(Native_capability()), _ep(0), _cap_valid(Lock::LOCKED) { }

			/**
			 * Set entry point, which the activation serves
			 *
			 * This function is only called by the 'Pager_entrypoint'
			 * constructor.
			 */
			void ep(Pager_entrypoint *ep) { _ep = ep; }

			/**
			 * Thread interface
			 */
			void entry();

			/**
			 * Return capability to this activation
			 *
			 * This function should only be called from 'Pager_entrypoint'
			 */
			Native_capability cap()
			{
				/* ensure that the initialization of our 'Ipc_pager' is done */
				if (!_cap.valid())
					_cap_valid.lock();
				return _cap;
			}
	};


	/**
	 * Paging entry point
	 *
	 * For a paging entry point can hold only one activation. So, paging is
	 * strictly serialized for one entry point.
	 */
	class Pager_entrypoint : public Object_pool<Pager_object>
	{
		private:

			Pager_activation_base *_activation;
			Cap_session           *_cap_session;

		public:

			/**
			 * Constructor
			 *
			 * \param cap_session  Cap_session for creating capabilities
			 *                     for the pager objects managed by this
			 *                     entry point
			 * \param a            initial activation
			 */
			Pager_entrypoint(Cap_session *cap_session, Pager_activation_base *a = 0);

			/**
			 * Associate Pager_object with the entry point
			 */
			Pager_capability manage(Pager_object *obj);

			/**
			 * Dissolve Pager_object from entry point
			 */
			void dissolve(Pager_object *obj);
	};


	template <int STACK_SIZE>
	class Pager_activation : public Pager_activation_base
	{
		public:

			Pager_activation() : Pager_activation_base("pager", STACK_SIZE)
			{ start(); }
	};
}

#endif /* _INCLUDE__BASE__PAGER_H_ */
