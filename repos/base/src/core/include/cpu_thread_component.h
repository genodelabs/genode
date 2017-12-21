/*
 * \brief  CPU thread RPC object
 * \author Norman Feske
 * \date   2016-05-10
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__CPU_THREAD_COMPONENT_H_
#define _CORE__INCLUDE__CPU_THREAD_COMPONENT_H_

/* Genode includes */
#include <util/list.h>
#include <base/rpc_server.h>
#include <cpu_thread/cpu_thread.h>
#include <base/session_label.h>

/* core includes */
#include <pager.h>
#include <cpu_thread_allocator.h>
#include <pd_session_component.h>
#include <platform_thread.h>
#include <trace/control_area.h>
#include <trace/source_registry.h>

namespace Genode { class Cpu_thread_component; }


class Genode::Cpu_thread_component : public  Rpc_object<Cpu_thread>,
                                     private List<Cpu_thread_component>::Element,
                                     public  Trace::Source::Info_accessor
{
	public:

		typedef Trace::Thread_name Thread_name;

	private:

		friend class List<Cpu_thread_component>;
		friend class Cpu_session_component;

		Rpc_entrypoint           &_ep;
		Pager_entrypoint         &_pager_ep;
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

		/**
		 * Exception handler as defined by 'Cpu_session::exception_sigh'
		 */
		Signal_context_capability _session_sigh { };

		/**
		 * Exception handler as defined by 'Cpu_thread::exception_sigh'
		 */
		Signal_context_capability _thread_sigh { };

		struct Trace_control_slot
		{
			unsigned index = 0;
			Trace::Control_area &trace_control_area;

			Trace_control_slot(Trace::Control_area &trace_control_area)
			: trace_control_area(trace_control_area)
			{
				if (!trace_control_area.alloc(index))
					throw Out_of_ram();
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

		Trace::Source_registry &_trace_sources;

		Rm_client _rm_client;

		/**
		 * Propagate exception handler to platform thread
		 */
		void _update_exception_sigh();

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
		 */
		Cpu_thread_component(Cpu_session_capability     cpu_session_cap,
		                     Rpc_entrypoint            &ep,
		                     Pager_entrypoint          &pager_ep,
		                     Pd_session_component      &pd,
		                     Trace::Control_area       &trace_control_area,
		                     Trace::Source_registry    &trace_sources,
		                     Cpu_session::Weight        weight,
		                     size_t                     quota,
		                     Affinity::Location         location,
		                     Session_label       const &label,
		                     Thread_name         const &name,
		                     unsigned                   priority,
		                     addr_t                     utcb)
		:
			_ep(ep), _pager_ep(pager_ep),
			_address_space_region_map(pd.address_space_region_map()),
			_weight(weight),
			_session_label(label), _name(name),
			_platform_thread(quota, name.string(), priority, location, utcb),
			_bound_to_pd(_bind_to_pd(pd)),
			_trace_control_slot(trace_control_area),
			_trace_sources(trace_sources),
			_rm_client(cpu_session_cap, _ep.manage(this),
			           &_address_space_region_map,
			           _platform_thread.pager_object_badge(),
			           _platform_thread.affinity(),
			           pd.label(), name)
		{
			_address_space_region_map.add_client(_rm_client);

			/*
			 * Acquaint thread with its pager object, caution on some base platforms
			 * this may raise an 'Out_of_ram' exception, which causes the
			 * destructor of this object to not being called. Catch it and remove this
			 * object from the object pool
			 */
			try {
					_pager_ep.manage(&_rm_client);
			} catch (...) {
				_ep.dissolve(this);
				throw;
			}

			_platform_thread.pager(&_rm_client);
			_trace_sources.insert(&_trace_source);
		}

		~Cpu_thread_component()
		{
			_trace_sources.remove(&_trace_source);
			_pager_ep.dissolve(&_rm_client);
			_ep.dissolve(this);

			_address_space_region_map.remove_client(_rm_client);
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

		/**
		 * Define default exception handler installed for the CPU session
		 */
		void session_exception_sigh(Signal_context_capability);

		void quota(size_t);

		/*
		 * Called by platform-specific 'Native_cpu' operations
		 */
		Platform_thread &platform_thread() { return _platform_thread; }

		size_t weight() const { return _weight.value; }


		/**************************
		 ** CPU thread interface **
		 *************************/

		Dataspace_capability utcb() override;
		void start(addr_t, addr_t) override;
		void pause() override;
		void resume() override;
		void single_step(bool) override;
		void cancel_blocking() override;
		Thread_state state() override;
		void state(Thread_state const &) override;
		void exception_sigh(Signal_context_capability) override;
		void affinity(Affinity::Location) override;
		unsigned trace_control_index() override;
		Dataspace_capability trace_buffer() override;
		Dataspace_capability trace_policy() override;
};

#endif /* _CORE__INCLUDE__CPU_THREAD_COMPONENT_H_ */
