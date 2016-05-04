/*
 * \brief  Core-specific instance of the CPU session/thread interfaces
 * \author Christian Helmuth
 * \date   2006-07-17
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__CPU_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__CPU_SESSION_COMPONENT_H_

/* Genode includes */
#include <util/list.h>
#include <base/allocator_guard.h>
#include <base/lock.h>
#include <base/rpc_server.h>
#include <cpu_session/cpu_session.h>

/* core includes */
#include <pager.h>
#include <cpu_thread_allocator.h>
#include <pd_session_component.h>
#include <platform_thread.h>
#include <trace/control_area.h>
#include <trace/source_registry.h>
#include <native_cpu_component.h>

namespace Genode {

	/**
	 * RPC interface of CPU thread
	 *
	 * We make 'Cpu_thread' a RPC object only to be able to lookup CPU threads
	 * from thread capabilities supplied as arguments to CPU-session functions.
	 * A CPU thread does not provide an actual RPC interface.
	 */
	struct Cpu_thread
	{
		GENODE_RPC_INTERFACE();
	};


	class Cpu_thread_component : public Rpc_object<Cpu_thread>,
	                             public List<Cpu_thread_component>::Element,
	                             public Trace::Source::Info_accessor
	{
		public:

			typedef Trace::Session_label Session_label;
			typedef Trace::Thread_name   Thread_name;

		private:

			Rpc_entrypoint           &_ep;
			Pager_entrypoint         &_pager_ep;
			Capability<Pd_session>    _pd;
			Region_map_component     &_address_space_region_map;
			Cpu_session::Weight const _weight;
			Session_label       const _session_label;
			Thread_name         const _name;
			Platform_thread           _platform_thread;
			bool                const _bound_to_pd;

			bool _bind_to_pd(Pd_session_component &pd)
			{
				if (!pd.bind_thread(_platform_thread))
					throw Cpu_session::Thread_creation_failed();
				return true;
			}

			Signal_context_capability _sigh;             /* exception handler */

			struct Trace_control_slot
			{
				unsigned index = 0;
				Trace::Control_area &trace_control_area;

				Trace_control_slot(Trace::Control_area &trace_control_area)
				: trace_control_area(trace_control_area)
				{
					if (!trace_control_area.alloc(index))
						throw Cpu_session::Out_of_metadata();
				}

				~Trace_control_slot()
				{
					trace_control_area.free(index);
				}

				Trace::Control &control()
				{
					return *trace_control_area.at(index);
				}
			};

			Trace_control_slot _trace_control_slot;

			Trace::Source _trace_source { *this, _trace_control_slot.control() };

			Weak_ptr<Address_space> _address_space = _platform_thread.address_space();

			Rm_client _rm_client;

		public:

			/**
			 * Constructor
			 *
			 * \param ep         entrypoint used for managing the thread RPC
			 *                   object
			 * \param pager_ep   pager entrypoint used for handling the page
			 *                   faults of the thread
			 * \param pd         PD session where the thread is executed
			 * \param weight     scheduling weight relative to the other
			 *                   threads of the same CPU session
			 * \param quota      initial quota counter-value of the weight
			 * \param labal      label of the threads session
			 * \param name       name for the thread
			 * \param priority   scheduling priority
			 * \param utcb       user-local UTCB base
			 * \param sigh       initial exception handler
			 */
			Cpu_thread_component(Cpu_session_capability    cpu_session_cap,
			                     Rpc_entrypoint           &ep,
			                     Pager_entrypoint         &pager_ep,
			                     Pd_session_component     &pd,
			                     Trace::Control_area      &trace_control_area,
			                     Cpu_session::Weight       weight,
			                     size_t                    quota,
			                     Affinity::Location        location,
			                     Session_label      const &label,
			                     Thread_name        const &name,
			                     unsigned                  priority,
			                     addr_t                    utcb,
			                     Signal_context_capability sigh)
			:
				_ep(ep), _pager_ep(pager_ep), _pd(pd.cap()),
				_address_space_region_map(pd.address_space_region_map()),
				_weight(weight),
				_session_label(label), _name(name),
				_platform_thread(quota, name.string(), priority, location, utcb),
				_bound_to_pd(_bind_to_pd(pd)),
				_sigh(sigh),
				_trace_control_slot(trace_control_area),
				_rm_client(cpu_session_cap, _ep.manage(this),
				           &_address_space_region_map,
				           _platform_thread.pager_object_badge(),
				           _address_space, _platform_thread.affinity())
			{
				update_exception_sigh();

				_address_space_region_map.add_client(_rm_client);

				/* acquaint thread with its pager object */
				_pager_ep.manage(&_rm_client);
				_platform_thread.pager(&_rm_client);
			}

			~Cpu_thread_component()
			{
				_pager_ep.dissolve(&_rm_client);
				_ep.dissolve(this);

				_address_space_region_map.remove_client(_rm_client);
			}

			void affinity(Affinity::Location affinity)
			{
				_platform_thread.affinity(affinity);
			}


			/********************************************
			 ** Trace::Source::Info_accessor interface **
			 ********************************************/

			Trace::Source::Info trace_source_info() const
			{
				return { _session_label, _name,
				         _platform_thread.execution_time(),
				         _platform_thread.affinity() };
			}


			/************************
			 ** Accessor functions **
			 ************************/

			Capability<Pd_session> pd() const { return _pd; }

			Platform_thread *platform_thread() { return &_platform_thread; }

			Trace::Source *trace_source() { return &_trace_source; }

			size_t weight() const { return _weight.value; }

			void sigh(Signal_context_capability sigh)
			{
				_sigh = sigh;
				update_exception_sigh();
			}

			/**
			 * Propagate exception handler to platform thread
			 */
			void update_exception_sigh();

			/**
			 * Return index within the CPU-session's trace control area
			 */
			unsigned trace_control_index() const { return _trace_control_slot.index; }
	};


	class Cpu_session_component : public Rpc_object<Cpu_session>,
	                              public List<Cpu_session_component>::Element
	{
		public:

			typedef Cpu_thread_component::Session_label Session_label;

		private:

			Session_label              _label;
			Rpc_entrypoint * const     _session_ep;
			Rpc_entrypoint            *_thread_ep;
			Pager_entrypoint          *_pager_ep;
			Allocator_guard            _md_alloc;          /* guarded meta-data allocator */
			Cpu_thread_allocator       _thread_alloc;      /* meta-data allocator */
			Lock                       _thread_alloc_lock; /* protect allocator access */
			List<Cpu_thread_component> _thread_list;
			Lock                       _thread_list_lock;  /* protect thread list */
			unsigned                   _priority;          /* priority of threads
			                                                  created with this
			                                                  session */
			Affinity::Location         _location;          /* CPU affinity of this 
			                                                  session */
			Trace::Source_registry    &_trace_sources;
			Trace::Control_area        _trace_control_area;

			/*
			 * Members for quota accounting
			 */

			size_t                      _weight;
			size_t                      _quota;
			Cpu_session_component *     _ref;
			List<Cpu_session_component> _ref_members;
			Lock                        _ref_members_lock;

			Native_cpu_component        _native_cpu;

			friend class Native_cpu_component;

			/*
			 * Utilities for quota accounting
			 */

			void _incr_weight(size_t const weight);
			void _decr_weight(size_t const weight);
			size_t _weight_to_quota(size_t const weight) const;
			void _decr_quota(size_t const quota);
			void _incr_quota(size_t const quota);
			void _update_thread_quota(Cpu_thread_component *) const;
			void _update_each_thread_quota();
			void _transfer_quota(Cpu_session_component * const dst,
			                     size_t const quota);

			void _insert_ref_member(Cpu_session_component * const s)
			{
				Lock::Guard lock_guard(_ref_members_lock);
				_ref_members.insert(s);
				s->_ref = this;
			}

			void _unsync_remove_ref_member(Cpu_session_component * const s)
			{
				s->_ref = 0;
				_ref_members.remove(s);
			}

			void _remove_ref_member(Cpu_session_component * const s)
			{
				Lock::Guard lock_guard(_ref_members_lock);
				_unsync_remove_ref_member(s);
			}

			void _deinit_ref_account();
			void _deinit_threads();

			/**
			 * Exception handler that will be invoked unless overridden by a
			 * call of 'Cpu_session::exception_handler'.
			 */
			Signal_context_capability _default_exception_handler;

			/**
			 * Raw thread-killing functionality
			 *
			 * This function is called from the 'kill_thread' function and
			 * the destructor. Each these functions grab the list lock
			 * by themselves and call this function to perform the actual
			 * killing.
			 */
			void _unsynchronized_kill_thread(Thread_capability cap);

			/**
			 * Convert session-local affinity location to physical location
			 */
			Affinity::Location _thread_affinity(Affinity::Location) const;

		public:

			/**
			 * Constructor
			 */
			Cpu_session_component(Rpc_entrypoint         *session_ep,
			                      Rpc_entrypoint         *thread_ep,
			                      Pager_entrypoint       *pager_ep,
			                      Allocator              *md_alloc,
			                      Trace::Source_registry &trace_sources,
			                      const char *args, Affinity const &affinity,
			                      size_t quota);

			/**
			 * Destructor
			 */
			~Cpu_session_component();

			/**
			 * Register quota donation at allocator guard
			 */
			void upgrade_ram_quota(size_t ram_quota) { _md_alloc.upgrade(ram_quota); }


			/***************************
			 ** CPU session interface **
			 ***************************/

			Thread_capability create_thread(Capability<Pd_session>, Name const &,
			                                Affinity::Location, Weight, addr_t) override;
			Ram_dataspace_capability utcb(Thread_capability thread) override;
			void kill_thread(Thread_capability) override;
			int start(Thread_capability, addr_t, addr_t) override;
			void pause(Thread_capability thread_cap) override;
			void resume(Thread_capability thread_cap) override;
			void single_step(Thread_capability thread_cap, bool enable) override;
			void cancel_blocking(Thread_capability) override;
			Thread_state state(Thread_capability) override;
			void state(Thread_capability, Thread_state const &) override;
			void exception_handler(Thread_capability, Signal_context_capability) override;
			Affinity::Space affinity_space() const override;
			void affinity(Thread_capability, Affinity::Location) override;
			Dataspace_capability trace_control() override;
			unsigned trace_control_index(Thread_capability) override;
			Dataspace_capability trace_buffer(Thread_capability) override;
			Dataspace_capability trace_policy(Thread_capability) override;
			int ref_account(Cpu_session_capability c) override;
			int transfer_quota(Cpu_session_capability, size_t) override;
			Quota quota() override;

			Capability<Native_cpu> native_cpu() override { return _native_cpu.cap(); }
	};
}

#endif /* _CORE__INCLUDE__CPU_SESSION_COMPONENT_H_ */
