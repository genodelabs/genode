/*
 * \brief  Paging-server framework
 * \author Norman Feske
 * \author Christian Helmuth
 * \author Stefan Kalkowski
 * \date   2006-04-28
 */

/*
 * Copyright (C) 2006-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__PAGER_H_
#define _CORE__INCLUDE__PAGER_H_

#include <base/thread.h>
#include <base/object_pool.h>
#include <cap_session/cap_session.h>
#include <pager/capability.h>
#include <ipc_pager.h>

namespace Genode {

	/**
	 * Special server object for paging
	 *
	 * A 'Pager_object' is very similar to a 'Rpc_object'. It is just a
	 * special implementation for page-fault handling, which does not allow to
	 * define a "badge" for pager capabilities.
	 */
	class Pager_object;

	/**
	 * Paging entry point
	 */
	class Pager_entrypoint;

	enum { PAGER_EP_STACK_SIZE = sizeof(addr_t) * 2048 };
}


class Genode::Pager_object : public Object_pool<Pager_object>::Entry
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

		/*
		 * Note in the thread state that an unresolved page
		 * fault occurred.
		 */
		void unresolved_page_fault_occurred();
};


class Genode::Pager_entrypoint : public Object_pool<Pager_object>,
                                 public Thread<PAGER_EP_STACK_SIZE>
{
	private:

		Ipc_pager    _pager;
		Cap_session *_cap_session;

		Untyped_capability _pager_object_cap(unsigned long badge);

	public:

		/**
		 * Constructor
		 *
		 * \param cap_session  Cap_session for creating capabilities
		 *                     for the pager objects managed by this
		 *                     entry point
		 */
		Pager_entrypoint(Cap_session *cap_session)
		: Thread<PAGER_EP_STACK_SIZE>("pager_ep"),
		  _cap_session(cap_session) { start(); }

		/**
		 * Associate Pager_object with the entry point
		 */
		Pager_capability manage(Pager_object *obj);

		/**
		 * Dissolve Pager_object from entry point
		 */
		void dissolve(Pager_object *obj);


		/**********************
		 ** Thread interface **
		 **********************/

		void entry();
};

#endif /* _CORE__INCLUDE__PAGER_H_ */
