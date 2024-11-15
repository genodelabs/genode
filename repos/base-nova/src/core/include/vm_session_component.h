/*
 * \brief  Core-specific instance of the VM session interface
 * \author Alexander Boettcher
 * \author Christian Helmuth
 * \date   2018-08-26
 */

/*
 * Copyright (C) 2018-2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__VM_SESSION_COMPONENT_H_
#define _CORE__VM_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/rpc_server.h>
#include <vm_session/vm_session.h>
#include <trace/control_area.h>
#include <trace/source_registry.h>
#include <nova_native_vcpu/nova_native_vcpu.h>

/* core includes */
#include <types.h>

namespace Core { class Vm_session_component; }


class Core::Vm_session_component
:
	private Ram_quota_guard,
	private Cap_quota_guard,
	public  Rpc_object<Vm_session, Vm_session_component>,
	private Region_map_detach
{
	private:

		using Con_ram_allocator = Constrained_ram_allocator;
		using Avl_region        = Allocator_avl_tpl<Rm_region>;

		class Vcpu : public Rpc_object<Vm_session::Native_vcpu, Vcpu>,
		             public Trace::Source::Info_accessor
		{
			public:

				struct Creation_failed { };

			private:

				Rpc_entrypoint               &_ep;
				Constrained_ram_allocator    &_ram_alloc;
				Cap_quota_guard              &_cap_alloc;
				Trace::Source_registry       &_trace_sources;
				addr_t                        _sel_sm_ec_sc;
				bool                          _alive { false };
				unsigned           const      _id;
				Affinity::Location const      _location;
				unsigned           const      _priority;
				Session_label      const     &_label;
				addr_t             const      _pd_sel;

				struct Trace_control_slot
				{
					unsigned index = 0;
					Trace::Control_area &_trace_control_area;

					Trace_control_slot(Trace::Control_area &trace_control_area)
					: _trace_control_area(trace_control_area)
					{
						if (!_trace_control_area.alloc(index))
							throw Out_of_ram();
					}

					~Trace_control_slot()
					{
						_trace_control_area.free(index);
					}

					Trace::Control &control()
					{
						return *_trace_control_area.at(index);
					}
				};

				Trace_control_slot _trace_control_slot;

				Trace::Source _trace_source { *this, _trace_control_slot.control() };

			public:

				Vcpu(Rpc_entrypoint &,
				     Constrained_ram_allocator &ram_alloc,
				     Cap_quota_guard &cap_alloc,
				     unsigned id,
				     unsigned kernel_id,
				     Affinity::Location,
				     unsigned priority,
				     Session_label const &,
				     addr_t   pd_sel,
				     addr_t   core_pd_sel,
				     addr_t   vmm_pd_sel,
				     Trace::Control_area &,
				     Trace::Source_registry &);

				~Vcpu();

				addr_t sm_sel() const { return _sel_sm_ec_sc + 0; }
				addr_t ec_sel() const { return _sel_sm_ec_sc + 1; }
				addr_t sc_sel() const { return _sel_sm_ec_sc + 2; }

				/*******************************
				 ** Native_vcpu RPC interface **
				 *******************************/

				Capability<Dataspace> state();
				void startup();
				void exit_handler(unsigned, Signal_context_capability);

				/********************************************
				 ** Trace::Source::Info_accessor interface **
				 ********************************************/

				Trace::Source::Info trace_source_info() const override;
		};

		Rpc_entrypoint         &_ep;
		Trace::Control_area     _trace_control_area;
		Trace::Source_registry &_trace_sources;
		Con_ram_allocator       _constrained_md_ram_alloc;
		Sliced_heap             _heap;
		Avl_region              _map { &_heap };
		addr_t                  _pd_sel { 0 };
		unsigned                _next_vcpu_id { 0 };
		unsigned                _priority;
		Session_label const     _session_label;

		Registry<Registered<Vcpu>> _vcpus { };

		/* helpers for vm_session_common.cc */
		void _attach_vm_memory(Dataspace_component &, addr_t, Attach_attr);
		void _detach_vm_memory(addr_t, size_t);
		void _with_region(addr_t, auto const &);

	protected:

		Ram_quota_guard &_ram_quota_guard() { return *this; }
		Cap_quota_guard &_cap_quota_guard() { return *this; }

	public:

		using Ram_quota_guard::upgrade;
		using Cap_quota_guard::upgrade;

		Vm_session_component(Rpc_entrypoint &, Resources, Label const &,
		                     Diag, Ram_allocator &ram, Region_map &, unsigned,
		                     Trace::Source_registry &);
		~Vm_session_component();


		/*********************************
		 ** Region_map_detach interface **
		 *********************************/

		/* used on destruction of attached dataspaces */
		void detach_at(addr_t) override;
		void unmap_region(addr_t, size_t) override;
		void reserve_and_flush(addr_t) override;


		/**************************
		 ** Vm session interface **
		 **************************/

		Capability<Native_vcpu> create_vcpu(Thread_capability) override;
		void attach_pic(addr_t) override { /* unused on NOVA */ }

		void attach(Dataspace_capability, addr_t, Attach_attr) override;
		void detach(addr_t, size_t) override;

};

#endif /* _CORE__VM_SESSION_COMPONENT_H_ */
