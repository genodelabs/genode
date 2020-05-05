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
#include <base/heap.h>
#include <vm_session/vm_session.h>

/* Core includes */
#include <cap_mapping.h>
#include <dataspace_component.h>
#include <region_map_component.h>

#include <trace/source_registry.h>

namespace Genode { class Vm_session_component; struct Vcpu; }

struct Genode::Vcpu : Genode::List<Vcpu>::Element
{
	private:

		Constrained_ram_allocator    &_ram_alloc;
		Cap_quota_guard              &_cap_alloc;
		Ram_dataspace_capability      _ds_cap { };
		Vm_session::Vcpu_id const     _id;
		Cap_mapping                   _recall { true };

	public:

		Vcpu(Constrained_ram_allocator &ram_alloc,
		     Cap_quota_guard &cap_alloc, Vm_session::Vcpu_id const id);

		~Vcpu();

		bool match(Vm_session::Vcpu_id const id) const { return id.id == _id.id; }
		Dataspace_capability ds_cap() { return _ds_cap; }
		Cap_mapping &recall_cap() { return _recall; }
};

class Genode::Vm_session_component
:
	private Ram_quota_guard,
	private Cap_quota_guard,
	public Rpc_object<Vm_session, Vm_session_component>,
	public Region_map_detach
{
	private:

		typedef Constrained_ram_allocator Con_ram_allocator;
		typedef Allocator_avl_tpl<Rm_region> Avl_region;

		Rpc_entrypoint    &_ep;
		Con_ram_allocator  _constrained_md_ram_alloc;
		Sliced_heap        _heap;
		Avl_region         _map { &_heap };
		List<Vcpu>         _vcpus { };
		Cap_mapping        _task_vcpu { true };
		unsigned           _id_alloc { 0 };

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

		void _exception_handler(Signal_context_capability, Vcpu_id) { }
		void _run(Vcpu_id) { }
		void _pause(Vcpu_id) { }
		void attach(Dataspace_capability, addr_t, Attach_attr) override;
		void attach_pic(addr_t) override { }
		void detach(addr_t, size_t) override;
		Vcpu_id _create_vcpu(Thread_capability);
};

#endif /* _CORE__VM_SESSION_COMPONENT_H_ */
