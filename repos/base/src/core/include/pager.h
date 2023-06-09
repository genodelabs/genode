/*
 * \brief  Paging-server framework
 * \author Norman Feske
 * \author Christian Helmuth
 * \author Stefan Kalkowski
 * \date   2006-04-28
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__PAGER_H_
#define _CORE__INCLUDE__PAGER_H_

/* Genode includes */
#include <base/session_label.h>
#include <base/thread.h>
#include <base/object_pool.h>
#include <pager/capability.h>
#include <cpu_session/cpu_session.h>
#include <ipc_pager.h>

/* core includes */
#include <rpc_cap_factory.h>
#include <pager_object_exception_state.h>

namespace Core {

	typedef Cpu_session::Thread_creation_failed Invalid_thread;

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

	using Pager_capability = Capability<Pager_object>;

	enum { PAGER_EP_STACK_SIZE = sizeof(addr_t) * 2048 };
}


class Core::Pager_object : public Object_pool<Pager_object>::Entry
{
	protected:

		/**
		 * Local name for this pager object
		 */
		unsigned long const _badge;

		Cpu_session_capability _cpu_session_cap;
		Thread_capability      _thread_cap;

		/**
		 * User-level signal handler registered for this pager object via
		 * 'Cpu_session::exception_handler()'.
		 */
		Signal_context_capability _exception_sigh { };

		Session_label             _pd_label;
		Cpu_session::Name         _name;

	public:

		/**
		 * Contains information about exception state of corresponding thread.
		 */
		Pager_object_exception_state state { };

		/**
		 * Constructor
		 *
		 * \param location  affinity of paged thread to physical CPU
		 *
		 * \throw Invalid_thread
		 */
		Pager_object(Cpu_session_capability cpu_sesion,
		             Thread_capability thread,
		             unsigned long badge, Affinity::Location,
		             Session_label const &pd_label,
		             Cpu_session::Name const &name)
		:
			_badge(badge), _cpu_session_cap(cpu_sesion), _thread_cap(thread),
			_pd_label(pd_label), _name(name)
		{ }

		virtual ~Pager_object() { }

		unsigned long badge() const { return _badge; }

		enum class Pager_result { STOP, CONTINUE };

		/**
		 * Interface to be implemented by a derived class
		 *
		 * \param ps  'Ipc_pager' stream
		 *
		 * Returns !0 on error and pagefault will not be answered.
		 */
		virtual Pager_result pager(Ipc_pager &ps) = 0;

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
		 * Return CPU session that was used to created the thread
		 */
		Cpu_session_capability cpu_session_cap() const { return _cpu_session_cap; }

		/**
		 * Return thread capability
		 *
		 * This function enables the destructor of the thread's
		 * address-space region map to kill the thread.
		 */
		Thread_capability thread_cap() const { return _thread_cap; }

		/*
		 * Note in the thread state that an unresolved page
		 * fault occurred.
		 */
		void unresolved_page_fault_occurred();

		/*
		 * Print pager object belonging
		 */
		void print(Output &out) const
		{
			Genode::print(out, "pager_object: pd='", _pd_label,
			                   "' thread='", _name, "'");
		}
};


class Core::Pager_entrypoint : public Object_pool<Pager_object>,
                               public Thread
{
	private:

		Ipc_pager        _pager { };
		Rpc_cap_factory &_cap_factory;

		Untyped_capability _pager_object_cap(unsigned long badge);

	public:

		/**
		 * Constructor
		 *
		 * \param cap_factory  factory for creating capabilities
		 *                     for the pager objects managed by this
		 *                     entry point
		 */
		Pager_entrypoint(Rpc_cap_factory &cap_factory)
		:
			Thread(Weight::DEFAULT_WEIGHT, "pager_ep", PAGER_EP_STACK_SIZE,
			       Type::NORMAL),
			_cap_factory(cap_factory)
		{ start(); }

		/**
		 * Associate Pager_object with the entry point
		 */
		Pager_capability manage(Pager_object &obj);

		/**
		 * Dissolve Pager_object from entry point
		 */
		void dissolve(Pager_object &obj);


		/**********************
		 ** Thread interface **
		 **********************/

		void entry() override;
};

#endif /* _CORE__INCLUDE__PAGER_H_ */
