/*
 * \brief  Paging-server framework
 * \author Norman Feske
 * \date   2006-04-28
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__PAGER_H_
#define _CORE__INCLUDE__PAGER_H_

/* Genode includes */
#include <base/thread.h>
#include <base/object_pool.h>
#include <base/capability.h>
#include <cap_session/cap_session.h>
#include <pager/capability.h>

/* Core includes */
#include <ipc_pager.h>


namespace Genode {

	class Pager_entrypoint;
	class Pager_object;

	class Exception_handlers
	{
		private:

			template <uint8_t EV>
			__attribute__((regparm(1))) static void _handler(addr_t);

		public:

			Exception_handlers(Pager_object *);

			template <uint8_t EV>
			void register_handler(Pager_object *, Nova::Mtd,
			                      void (__attribute__((regparm(1)))*)(addr_t) = nullptr);
	};


	class Pager_object : public Object_pool<Pager_object>::Entry
	{
		private:

			unsigned long _badge;  /* used for debugging */

			/**
			 * User-level signal handler registered for this pager object via
			 * 'Cpu_session::exception_handler()'.
			 */
			Signal_context_capability _exception_sigh;

			/**
			 * selectors for
			 * - cleanup portal
			 * - semaphore used by caller used to notify paused state
			 * - semaphore used to block during page fault handling or pausing
			 */
			addr_t _selectors;

			addr_t _initial_esp;
			addr_t _initial_eip;
			addr_t _client_exc_pt_sel;
			addr_t _client_exc_vcpu;

			struct
			{
				struct Thread_state thread;
				addr_t sel_client_ec;
				enum {
					BLOCKED        = 0x1U,
					DEAD           = 0x2U,
					SINGLESTEP     = 0x4U,
					NOTIFY_REQUEST = 0x8U,
					SIGNAL_SM      = 0x10U,
					DISSOLVED      = 0x20U,
					SUBMIT_SIGNAL  = 0x40U,
					SKIP_EXCEPTION = 0x80U,
				};
				uint8_t _status;

				/* convenience function to access pause/recall state */
				inline bool blocked() { return _status & BLOCKED;}
				inline void block()   { _status |= BLOCKED; }
				inline void unblock() { _status &= ~BLOCKED; }

				inline void mark_dead() { _status |= DEAD; }
				inline bool is_dead() { return _status & DEAD; }

				inline bool singlestep() { return _status & SINGLESTEP; }

				inline void notify_request()   { _status |= NOTIFY_REQUEST; }
				inline bool notify_requested() { return _status & NOTIFY_REQUEST; }
				inline void notify_cancel()    { _status &= ~NOTIFY_REQUEST; }

				inline void mark_signal_sm() { _status |= SIGNAL_SM; }
				inline bool has_signal_sm() { return _status & SIGNAL_SM; }

				inline void mark_dissolved() { _status |= DISSOLVED; }
				inline bool dissolved()      { return _status & DISSOLVED; }

				inline bool to_submit() { return _status & SUBMIT_SIGNAL; }
				inline void submit_signal() { _status |= SUBMIT_SIGNAL; }
				inline void reset_submit() { _status &= ~SUBMIT_SIGNAL; }

				inline bool skip_requested() { return _status & SKIP_EXCEPTION; }
				inline void skip_request() { _status |= SKIP_EXCEPTION; }
				inline void skip_reset() { _status &= ~SKIP_EXCEPTION; }
			} _state;

			Thread_capability  _thread_cap;
			Exception_handlers _exceptions;

			void _copy_state(Nova::Utcb * utcb);

			addr_t sel_pt_cleanup() { return _selectors; }
			addr_t sel_sm_notify()  { return _selectors + 1; }
			addr_t sel_sm_block()   { return _selectors + 2; }

			__attribute__((regparm(1)))
			static void _page_fault_handler(addr_t pager_obj);

			__attribute__((regparm(1)))
			static void _startup_handler(addr_t pager_obj);

			__attribute__((regparm(1)))
			static void _invoke_handler(addr_t pager_obj);

			__attribute__((regparm(1)))
			static void _recall_handler(addr_t pager_obj);

		public:

			const Affinity::Location location;

			Pager_object(unsigned long badge, Affinity::Location location);

			virtual ~Pager_object();

			unsigned long badge() const { return _badge; }
			void reset_badge() { _badge = 0; }

			virtual int pager(Ipc_pager &ps) = 0;

			/**
			 * Assign user-level exception handler for the pager object
			 */
			void exception_handler(Signal_context_capability sigh)
			{
				_exception_sigh = sigh;
			}

			void exception(uint8_t exit_id);

			/**
			 * Return base of initial portal window
			 */
			addr_t exc_pt_sel_client() { return _client_exc_pt_sel; }
			addr_t exc_pt_vcpu() { return _client_exc_vcpu; }

			/**
			 * Set initial stack pointer used by the startup handler
			 */
			addr_t initial_esp() { return _initial_esp; }
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

				_state.reset_submit();

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
				if (_state.blocked() || _state.is_dead())
					return Native_capability();

				_state.notify_request();

				return Native_capability(sel_sm_notify());
			}

			/**
			 * Copy thread state of recalled thread.
			 */
			bool copy_thread_state(Thread_state * state_dst)
			{
				if (!state_dst || !_state.blocked())
					return false;

				*state_dst = _state.thread;

				return true;
			}

			/**
			 * Cancel blocking in a lock so that recall exception can take
			 * place.
			 */
			void    client_cancel_blocking();

			uint8_t client_recall();
			void    client_set_ec(addr_t ec) { _state.sel_client_ec = ec; }

			inline Native_capability single_step(bool on)
			{
				if (_state.is_dead() ||
				    (on && (_state._status & _state.SINGLESTEP)) ||
				    (!on && !(_state._status & _state.SINGLESTEP)))
					return Native_capability();

				if (on)
					_state._status |= _state.SINGLESTEP;
				else
					_state._status &= ~_state.SINGLESTEP;

				/* we want to be notified if state change is done */
				_state.notify_request();
				/* the first single step exit ignore when switching it on */
				if (on && _state.blocked())
					_state.skip_request();

				/* force client in exit and thereby apply single_step change */
				client_recall();
				/* single_step mode changes don't apply if blocked - wake up */
				if (_state.blocked())
					wake_up();

				return Native_capability(sel_sm_notify());
			}

			/**
			 * Remember thread cap so that rm_session can tell thread that
			 * rm_client is gone.
			 */
			Thread_capability thread_cap() { return _thread_cap; } const
			void thread_cap(Thread_capability cap) { _thread_cap = cap; }

			/**
			 * Note in the thread state that an unresolved page
			 * fault occurred.
			 */
			void unresolved_page_fault_occurred()
			{
				_state.thread.unresolved_page_fault = true;
			}

			/**
			 * Make sure nobody is in the handler anymore by doing an IPC to a
			 * local cap pointing to same serving thread (if not running in the
			 * context of the serving thread). When the call returns
			 * we know that nobody is handled by this object anymore, because
			 * all remotely available portals had been revoked beforehand.
			 */
			void cleanup_call();

			/**
			 * Open receive window for initial portals for vCPU.
			 */
			void prepare_vCPU_portals()
			{
				_client_exc_vcpu = cap_map()->insert(Nova::NUM_INITIAL_VCPU_PT_LOG2);
			}
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

			/**
			 * Constructor
			 *
			 * \param name        name of the new thread
			 * \param stack_size  stack size of the new thread
			 */
			Pager_activation_base(char const * const name,
			                      size_t const stack_size);

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
			{ }
	};
}

#endif /* _CORE__INCLUDE__PAGER_H_ */
