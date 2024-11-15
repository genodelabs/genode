/*
 * \brief  Core-specific instance of the VM session interface
 * \author Alexander Boettcher
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
#include <base/registry.h>
#include <base/rpc_server.h>
#include <base/heap.h>
#include <util/bit_allocator.h>
#include <vm_session/vm_session.h>

/* core includes */
#include <cap_mapping.h>
#include <dataspace_component.h>
#include <region_map_component.h>
#include <trace/source_registry.h>
#include <foc_native_vcpu/foc_native_vcpu.h>

namespace Core {

	class Vm_session_component;
	struct Vcpu;

	enum { MAX_VCPU_IDS = (Platform::VCPU_VIRT_EXT_END -
	                       Platform::VCPU_VIRT_EXT_START) / L4_PAGESIZE };
	using Vcpu_id_allocator = Bit_allocator<MAX_VCPU_IDS>;
}


struct Core::Vcpu : Rpc_object<Vm_session::Native_vcpu, Vcpu>
{
	private:

		Rpc_entrypoint                 &_ep;
		Constrained_ram_allocator      &_ram_alloc;
		Cap_quota_guard                &_cap_alloc;
		Vcpu_id_allocator              &_vcpu_ids;
		Cap_mapping                     _recall            { true };
		Foc::l4_cap_idx_t               _task_index_client { };
		addr_t                          _foc_vcpu_state    { };

	public:

		Vcpu(Rpc_entrypoint &, Constrained_ram_allocator &, Cap_quota_guard &,
		     Platform_thread &, Cap_mapping &, Vcpu_id_allocator &);

		~Vcpu();

		Cap_mapping &recall_cap() { return _recall; }

		/*******************************
		 ** Native_vcpu RPC interface **
		 *******************************/

		Foc::l4_cap_idx_t task_index()     const { return _task_index_client; }
		addr_t            foc_vcpu_state() const { return _foc_vcpu_state; }
};


class Core::Vm_session_component
:
	private Ram_quota_guard,
	private Cap_quota_guard,
	public  Rpc_object<Vm_session, Vm_session_component>,
	public  Region_map_detach
{
	private:

		using Con_ram_allocator = Constrained_ram_allocator;
		using Avl_region        = Allocator_avl_tpl<Rm_region>;

		Rpc_entrypoint    &_ep;
		Con_ram_allocator  _constrained_md_ram_alloc;
		Sliced_heap        _heap;
		Avl_region         _map       { &_heap };
		Cap_mapping        _task_vcpu { true };
		Vcpu_id_allocator  _vcpu_ids  { };

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
		void detach_at         (addr_t)         override;
		void unmap_region      (addr_t, size_t) override;
		void reserve_and_flush (addr_t)         override;


		/**************************
		 ** Vm session interface **
		 **************************/

		Capability<Native_vcpu> create_vcpu(Thread_capability) override;
		void attach_pic(addr_t) override { /* unused on Fiasco.OC */ }

		void attach(Dataspace_capability, addr_t, Attach_attr) override; /* vm_session_common.cc */
		void detach(addr_t, size_t) override;                            /* vm_session_common.cc */
};

#endif /* _CORE__VM_SESSION_COMPONENT_H_ */
