/*
 * \brief  Core-specific instance of the CPU session/thread interfaces
 * \author Christian Helmuth
 * \date   2006-07-17
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__CPU_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__CPU_SESSION_COMPONENT_H_

/* Genode includes */
#include <util/list.h>
#include <base/ram_allocator.h>
#include <base/mutex.h>
#include <base/session_label.h>
#include <base/session_object.h>
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

namespace Core { class Cpu_session_component; }


class Core::Cpu_session_component : public  Session_object<Cpu_session>,
                                    private List<Cpu_session_component>::Element
{
	private:

		using Thread_alloc = Memory::Constrained_obj_allocator<Cpu_thread_component>;

		Rpc_entrypoint            &_session_ep;
		Rpc_entrypoint            &_thread_ep;
		Pager_entrypoint          &_pager_ep;
		Local_rm                  &_local_rm;
		Accounted_ram_allocator    _ram_alloc;
		Sliced_heap                _md_alloc;               /* guarded meta-data allocator */
		Cpu_thread_allocator       _thread_slab;            /* meta-data allocator */
		Thread_alloc               _thread_alloc { _thread_slab };
		Mutex                      _thread_alloc_lock { };  /* protect allocator access */
		List<Cpu_thread_component> _thread_list       { };
		Mutex                      _thread_list_lock  { };  /* protect thread list */
		unsigned                   _priority;               /* priority of threads
		                                                       created with this
		                                                       session */
		Affinity::Location         _location;               /* CPU affinity of this 
		                                                       session */
		Trace::Source_registry    &_trace_sources;
		Trace::Control_area        _trace_control_area;

		Native_cpu_component        _native_cpu;

		friend class Native_cpu_component;
		friend class List<Cpu_session_component>;

		void _deinit_threads();

		/**
		 * Exception handler to be invoked unless overridden by a
		 * thread-specific handler via 'Cpu_thread::exception_sigh'
		 */
		Signal_context_capability _exception_sigh { };

		/**
		 * Raw thread-killing functionality
		 *
		 * This function is called from the 'kill_thread' function and
		 * the destructor. Each these functions grab the list mutex
		 * by themselves and call this function to perform the actual
		 * killing.
		 */
		void _unsynchronized_kill_thread(Thread_capability cap);

		/**
		 * Convert session-local affinity location to physical location
		 */
		Affinity::Location _thread_affinity(Affinity::Location) const;

		/**
		 * Return the UTCB quota size that needs to be accounted per thread
		 */
		size_t _utcb_quota_size();

		/*
		 * Noncopyable
		 */
		Cpu_session_component(Cpu_session_component const &);
		Cpu_session_component &operator = (Cpu_session_component const &);

	public:

		using Constructed = Trace::Control_area::Constructed;

		Constructed const constructed = _trace_control_area.constructed;

		/**
		 * Constructor
		 */
		Cpu_session_component(Rpc_entrypoint         &session_ep,
		                      Resources        const &resources,
		                      Label            const &label,
		                      Diag             const &diag,
		                      Ram_allocator          &ram_alloc,
		                      Local_rm               &local_rm,
		                      Rpc_entrypoint         &thread_ep,
		                      Pager_entrypoint       &pager_ep,
		                      Trace::Source_registry &trace_sources,
		                      const char *args, Affinity const &affinity);

		/**
		 * Destructor
		 */
		~Cpu_session_component();


		/***************************
		 ** CPU session interface **
		 ***************************/

		Create_thread_result create_thread(Capability<Pd_session>, Name const &,
		                                   Affinity::Location, addr_t) override;
		void kill_thread(Thread_capability) override;
		void exception_sigh(Signal_context_capability) override;
		Affinity::Space affinity_space() const override;
		Dataspace_capability trace_control() override;
		Capability<Native_cpu> native_cpu() override { return _native_cpu.cap(); }
};

#endif /* _CORE__INCLUDE__CPU_SESSION_COMPONENT_H_ */
