/*
 * \brief  Paging-server framework
 * \author Norman Feske
 * \date   2006-04-28
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__PAGER_H_
#define _INCLUDE__BASE__PAGER_H_

#include <base/thread.h>
#include <base/object_pool.h>
#include <base/ipc_pager.h>
#include <base/capability.h>
#include <cap_session/cap_session.h>
#include <pager/capability.h>

namespace Genode {

	class Pager_entrypoint;

	/*
	 * On NOVA, each pager object is an EC that corresponds to one user thread.
	 */
	class Pager_object : public Object_pool<Pager_object>::Entry,
	                     Thread_base
	{
		private:

			void entry() { }
			void start() { }

			unsigned long _badge;  /* used for debugging */

			/**
			 * User-level signal handler registered for this pager object via
			 * 'Cpu_session::exception_handler()'.
			 */
			Signal_context_capability _exception_sigh;

  			/**
			 * Portal selector for object cleanup/destruction
			 */
			addr_t _pt_cleanup;

 			/**
			 * Semaphore selector to synchronize pause/state/resume operations
			 */
			addr_t _sm_state_notify;

			addr_t _initial_esp;
			addr_t _initial_eip;

			struct {
				struct Thread_state thread;
				addr_t sel_client_ec;
				bool valid;
				bool dead;
				bool singlestep;
			} _state;

			void _copy_state(Nova::Utcb * utcb);

			static void _page_fault_handler();
			static void _startup_handler();
			static void _invoke_handler();
			static void _recall_handler();

			__attribute__((regparm(1)))
			static void _exception_handler(addr_t portal_id);

			static Nova::Utcb * _check_handler(Thread_base *&, Pager_object *&);
		public:

			Pager_object(unsigned long badge);

			virtual ~Pager_object();

			unsigned long badge() const { return _badge; }

			virtual int pager(Ipc_pager &ps) = 0;

			/**
			 * Assign user-level exception handler for the pager object
			 */
			void exception_handler(Signal_context_capability sigh)
			{
				_exception_sigh = sigh;
			}

			/**
			 * Return base of initial portal window
			 */
			addr_t ec_sel() { return _tid.ec_sel; }

			/**
			 * Return base of initial portal window
			 */
			addr_t exc_pt_sel() { return _tid.exc_pt_sel; }

			/**
			 * Set initial stack pointer used by the startup handler
			 */
			void initial_esp(addr_t esp) { _initial_esp = esp; }

			/**
			 * Set initial instruction pointer used by the startup handler
			 */
			void initial_eip(addr_t eip) { _initial_eip = eip; }

			/**
			 * Continue execution of pager object
			 */
			void wake_up();

			/**
			 * Notify exception handler about the occurrence of an exception
			 */
			bool submit_exception_signal()
			{
				if (!_exception_sigh.valid()) return false;

				Signal_transmitter transmitter(_exception_sigh);
				transmitter.submit();

				return true;
			}

			/**
			 * Return entry point address
			 */
			addr_t handler_address()
			{
				return reinterpret_cast<addr_t>(_invoke_handler);
			}

			/**
			 * Return semaphore to block on until state of a recall is
			 * available.
			 */
			Native_capability notify_sm()
			{
				if (_state.valid)
					return Native_capability::invalid_cap();
				if (_state.dead)
					return Native_capability::invalid_cap();

				return Native_capability(_sm_state_notify);
			}

			/**
			 * Copy thread state of recalled thread.
			 */
			int copy_thread_state(Thread_state * state_dst)
			{
				if (!state_dst || !_state.valid) return -1;

				*state_dst = _state.thread;

				return 0;
			}

			/**
			 * Cancel blocking in a lock so that recall exception can take
			 * place.
			 */
			void    client_cancel_blocking();

			uint8_t client_recall();
			void    client_set_ec(addr_t ec) { _state.sel_client_ec = ec; }

			void single_step(bool on) { _state.singlestep = on; }
	};


	/**
	 * Dummy pager activation
	 *
	 * Because on NOVA each pager object can be invoked separately,
	 * there is no central pager activation.
	 */
	class Pager_activation_base { };


	template <unsigned STACK_SIZE>
	class Pager_activation : public Pager_activation_base
	{ };


	/**
	 * Dummy pager entrypoint
	 */
	class Pager_entrypoint : public Object_pool<Pager_object>
	{
		private:

			Cap_session *_cap_session;

		public:

			Pager_entrypoint(Cap_session *cap_session,
			                 Pager_activation_base *a = 0)
			: _cap_session(cap_session) { }

			/**
			 * Return capability for 'Pager_object'
			 */
			Pager_capability manage(Pager_object *obj);

			/**
			 * Dissolve 'Pager_object' from entry point
			 */
			void dissolve(Pager_object *obj);
	};
}

#endif /* _INCLUDE__BASE__PAGER_H_ */
