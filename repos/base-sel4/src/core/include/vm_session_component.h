/*
 * \brief  Core-specific instance of the VM session interface
 * \author Alexander Boettcher
 * \date   2018-08-26
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__VM_SESSION_COMPONENT_H_
#define _CORE__VM_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/rpc_server.h>
#include <vm_session/vm_session.h>

#include <trace/source_registry.h>

namespace Genode { class Vm_session_component; }

class Genode::Vm_session_component
:
	private Ram_quota_guard,
	private Cap_quota_guard,
	public Rpc_object<Vm_session, Vm_session_component>,
	public Region_map_detach
{
	private:

		class Vcpu : public Genode::List<Vcpu>::Element
		{
			private:

				Constrained_ram_allocator &_ram_alloc;
				Ram_dataspace_capability   _ds_cap;
				Cap_sel                    _notification { 0 };
				Vm_session::Vcpu_id        _vcpu_id;

				void _free_up();

			public:

				Vcpu(Constrained_ram_allocator &, Cap_quota_guard &, Vcpu_id,
				     seL4_Untyped);
				~Vcpu() { _free_up(); }

				Dataspace_capability ds_cap() const { return _ds_cap; }
				bool match(Vcpu_id id) const { return id.id == _vcpu_id.id; }
				void signal() const { seL4_Signal(_notification.value()); }
				Cap_sel notification_cap() const { return _notification; }
		};

		typedef Allocator_avl_tpl<Rm_region> Avl_region;

		Rpc_entrypoint            &_ep;
		Constrained_ram_allocator  _constrained_md_ram_alloc;
		Heap                       _heap;
		Avl_region                 _map { &_heap };
		List<Vcpu>                 _vcpus { };
		unsigned                   _id_alloc { 0 };
		unsigned                   _pd_id    { 0 };
		Cap_sel                    _vm_page_table;
		Page_table_registry        _page_table_registry { _heap };
		Vm_space                   _vm_space;
		struct {
			addr_t       _phys;
			seL4_Untyped _service;
		}                          _ept { 0, 0 };
		struct {
			addr_t       _phys;
			seL4_Untyped _service;
		}                          _notifications { 0, 0 };

		Vcpu * _lookup(Vcpu_id const vcpu_id)
		{
			for (Vcpu * vcpu = _vcpus.first(); vcpu; vcpu = vcpu->next())
				if (vcpu->match(vcpu_id)) return vcpu;

			return nullptr;
		}

		void _attach_vm_memory(Dataspace_component &, addr_t, Attach_attr);
		void _detach_vm_memory(addr_t, size_t);

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

		void detach(Region_map::Local_addr) override;
		void unmap_region(addr_t, size_t) override;

		/**************************
		 ** Vm session interface **
		 **************************/

		Dataspace_capability _cpu_state(Vcpu_id);

		void _exception_handler(Signal_context_capability, Vcpu_id) {}
		void _run(Vcpu_id) {}
		void _pause(Vcpu_id);
		void attach(Dataspace_capability, addr_t, Attach_attr) override;
		void attach_pic(addr_t) override {}
		void detach(addr_t, size_t) override;
		Vcpu_id _create_vcpu(Thread_capability);
};

#endif /* _CORE__VM_SESSION_COMPONENT_H_ */
