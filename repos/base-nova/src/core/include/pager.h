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

/* NOVA includes */
#include <nova/cap_map.h>

/* core-local includes */
#include <ipc_pager.h>
#include <rpc_cap_factory.h>


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

			Lock   _state_lock;

			struct
			{
				struct Thread_state thread;
				addr_t sel_client_ec;
				enum {
					BLOCKED       = 0x1U,
					DEAD          = 0x2U,
					SINGLESTEP    = 0x4U,
					SIGNAL_SM     = 0x8U,
					DISSOLVED     = 0x10U,
					SUBMIT_SIGNAL = 0x20U,
				};
				uint8_t _status;
				bool modified;

				/* convenience function to access pause/recall state */
				inline bool blocked() { return _status & BLOCKED;}
				inline void block()   { _status |= BLOCKED; }
				inline void unblock() { _status &= ~BLOCKED; }

				inline void mark_dead() { _status |= DEAD; }
				inline bool is_dead() { return _status & DEAD; }

				inline bool singlestep() { return _status & SINGLESTEP; }

				inline void mark_signal_sm() { _status |= SIGNAL_SM; }
				inline bool has_signal_sm() { return _status & SIGNAL_SM; }

				inline void mark_dissolved() { _status |= DISSOLVED; }
				inline bool dissolved()      { return _status & DISSOLVED; }

				inline bool to_submit() { return _status & SUBMIT_SIGNAL; }
				inline void submit_signal() { _status |= SUBMIT_SIGNAL; }
				inline void reset_submit() { _status &= ~SUBMIT_SIGNAL; }

			} _state;

			Cpu_session_capability _cpu_session_cap;
			Thread_capability      _thread_cap;
			Exception_handlers     _exceptions;

			addr_t _pd;

			void _copy_state_from_utcb(Nova::Utcb * utcb);
			void _copy_state_to_utcb(Nova::Utcb * utcb);

			uint8_t _unsynchronized_client_recall(bool get_state_and_block);

			addr_t sel_pt_cleanup()     const { return _selectors; }
			addr_t sel_sm_block_pause() const { return _selectors + 1; }
			addr_t sel_sm_block_oom()   const { return _selectors + 2; }
			addr_t sel_oom_portal()     const { return _selectors + 3; }

			__attribute__((regparm(1)))
			static void _page_fault_handler(addr_t pager_obj);

			__attribute__((regparm(1)))
			static void _startup_handler(addr_t pager_obj);

			__attribute__((regparm(1)))
			static void _invoke_handler(addr_t pager_obj);

			__attribute__((regparm(1)))
			static void _recall_handler(addr_t pager_obj);

			__attribute__((regparm(3)))
			static void _oom_handler(addr_t, addr_t, addr_t);

		public:

			const Affinity::Location location;

			Pager_object(Cpu_session_capability cpu_session_cap,
			             Thread_capability thread_cap,
			             unsigned long badge, Affinity::Location location);

			virtual ~Pager_object();

			unsigned long badge() const { return _badge; }
			void reset_badge() { _badge = 0; }

			const char * client_thread() const;
			const char * client_pd() const;

			virtual int pager(Ipc_pager &ps) = 0;

			/**
			 * Assign user-level exception handler for the pager object
			 */
			void exception_handler(Signal_context_capability sigh)
			{
				_exception_sigh = sigh;
			}

			/**
			 * Assign PD selector to PD
			 */
			void assign_pd(addr_t pd_sel) { _pd = pd_sel; }
			addr_t pd_sel() const { return _pd; }

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
			 * Copy thread state of recalled thread.
			 */
			bool copy_thread_state(Thread_state * state_dst)
			{
				Lock::Guard _state_lock_guard(_state_lock);

				if (!state_dst || !_state.blocked())
					return false;

				*state_dst = _state.thread;

				return true;
			}

			/*
			 * Copy thread state to recalled thread.
			 */
			bool copy_thread_state(Thread_state state_src)
			{
				Lock::Guard _state_lock_guard(_state_lock);

				if (!_state.blocked())
					return false;

				_state.thread = state_src;
				_state.modified = true;

				return true;
			}

			/**
			 * Cancel blocking in a lock so that recall exception can take
			 * place.
			 */
			void    client_cancel_blocking();

			uint8_t client_recall(bool get_state_and_block);
			void client_set_ec(addr_t ec) { _state.sel_client_ec = ec; }

			inline void single_step(bool on)
			{
				_state_lock.lock();

				if (_state.is_dead() || !_state.blocked() ||
				    (on && (_state._status & _state.SINGLESTEP)) ||
				    (!on && !(_state._status & _state.SINGLESTEP))) {
				    _state_lock.unlock();
					return;
				}

				if (on)
					_state._status |= _state.SINGLESTEP;
				else
					_state._status &= ~_state.SINGLESTEP;

				_state_lock.unlock();

				/* force client in exit and thereby apply single_step change */
				client_recall(false);
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

			/**
			 * Note in the thread state that an unresolved page
			 * fault occurred.
			 */
			void unresolved_page_fault_occurred()
			{
				Lock::Guard _state_lock_guard(_state_lock);

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

			/**
			 * Portal called by thread that causes a out of memory in kernel.
			 */
			addr_t get_oom_portal();

			enum Policy {
				STOP = 1,
				UPGRADE_CORE_TO_DST = 2,
				UPGRADE_PREFER_SRC_TO_DST = 3,
			};

			enum Oom {
				SEND = 1, REPLY = 2, SELF = 4,
				SRC_CORE_PD = ~0UL, SRC_PD_UNKNOWN = 0,
			};

			/**
			 * Implements policy on how to react on out of memory in kernel.
			 *
			 * Used solely inside core. On Genode core creates all the out
			 * of memory portals per EC. If the PD of a EC runs out of kernel
			 * memory it causes a OOM portal traversal, which is handled
			 * by the pager object of the causing thread.
			 *
			 * /param pd_sel  PD selector from where to transfer kernel memory
			 *                resources. The PD of this pager_object is the
			 *                target PD.
			 * /param pd      debug feature - string of PD (transfer_from)
			 * /param thread  debug feature - string of EC (transfer_from)
			 */
			uint8_t handle_oom(addr_t pd_sel       = SRC_CORE_PD,
			                   const char * pd     = "core",
			                   const char * thread = "unknown",
			                   Policy = Policy::UPGRADE_CORE_TO_DST);
	};

	/**
	 * A 'Pager_activation' processes one page fault of a 'Pager_object' at a time.
	 */
	class Pager_entrypoint;
	class Pager_activation_base: public Thread
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
			Rpc_cap_factory       &_cap_factory;

		public:

			/**
			 * Constructor
			 *
			 * \param cap_factory  factory for creating capabilities
			 *                     for the pager objects managed by this
			 *                     entry point
			 */
			Pager_entrypoint(Rpc_cap_factory &cap_factory);

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
