/*
 * \brief  Core-specific instance of the CPU session/thread interfaces
 * \author Christian Helmuth
 * \date   2006-07-17
 */

/*
 * Copyright (C) 2006-2016 Genode Labs GmbH
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
#include <base/session_label.h>
#include <base/rpc_server.h>
#include <cpu_session/cpu_session.h>

/* core includes */
#include <pager.h>
#include <cpu_thread_component.h>
#include <cpu_thread_allocator.h>
#include <pd_session_component.h>
#include <platform_thread.h>
#include <trace/control_area.h>
#include <trace/source_registry.h>
#include <native_cpu_component.h>

namespace Genode { class Cpu_session_component; }


class Genode::Cpu_session_component : public Rpc_object<Cpu_session>,
                                      public List<Cpu_session_component>::Element
{
	private:

		Session_label    const     _label;
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
		void _update_thread_quota(Cpu_thread_component &) const;
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
		 * Exception handler to be invoked unless overridden by a
		 * thread-specific handler via 'Cpu_thread::exception_sigh'
		 */
		Signal_context_capability _exception_sigh;

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
		void kill_thread(Thread_capability) override;
		void exception_sigh(Signal_context_capability) override;
		Affinity::Space affinity_space() const override;
		Dataspace_capability trace_control() override;
		int ref_account(Cpu_session_capability c) override;
		int transfer_quota(Cpu_session_capability, size_t) override;
		Quota quota() override;

		Capability<Native_cpu> native_cpu() override { return _native_cpu.cap(); }
};

#endif /* _CORE__INCLUDE__CPU_SESSION_COMPONENT_H_ */
