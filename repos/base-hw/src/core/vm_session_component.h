/*
 * \brief  Core-specific instance of the VM session interface
 * \author Stefan Kalkowski
 * \date   2012-10-08
 */

/*
 * Copyright (C) 2012-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__VM_SESSION_COMPONENT_H_
#define _CORE__VM_SESSION_COMPONENT_H_

/* base includes */
#include <base/registry.h>
#include <base/session_object.h>
#include <vm_session/vm_session.h>

/* base-hw includes */
#include <hw_native_vcpu/hw_native_vcpu.h>

/* core includes */
#include <guest_memory.h>
#include <object.h>
#include <kernel/vcpu.h>
#include <trace/source_registry.h>

#include <vmid_allocator.h>


namespace Core { class Vm_session_component; }


class Core::Vm_session_component
:
	public  Session_object<Vm_session>,
	private Region_map_detach,
	public  Revoke
{
	private:

		/*
		 * Noncopyable
		 */
		Vm_session_component(Vm_session_component const &);
		Vm_session_component &operator = (Vm_session_component const &);

		struct Vcpu : public Rpc_object<Vm_session::Native_vcpu, Vcpu>,
		              public Revoke
		{
			static size_t _ds_size();

			Kernel::Vcpu::Identity     &id;
			Rpc_entrypoint             &ep;
			Ram_allocator::Result       ds;
			Signal_context_capability   sigh_cap { };
			addr_t                      ds_addr  { };
			Kernel_object<Kernel::Vcpu> kobj     { };
			Affinity::Location          location { };

			Vcpu(Ram_allocator &ram, Kernel::Vcpu::Identity &id, Rpc_entrypoint &ep)
			:
				id(id), ep(ep), ds(ram.try_alloc(_ds_size(), Cache::UNCACHED))
			{
				ep.manage(this);
			}

			~Vcpu()
			{
				ep.dissolve(this);
			}

			/*******************************
			 ** Native_vcpu RPC interface **
			 *******************************/

			Capability<Dataspace> state() const
			{
				return ds.convert<Capability<Dataspace>>(
					[&] (Ram::Allocation const &ds) { return ds.cap; },
					[&] (Ram::Error) { return Capability<Dataspace>(); });
			}

			Native_capability native_vcpu() { return kobj.cap(); }

			void exception_handler(Signal_context_capability);

			void revoke_signal_context(Signal_context_capability cap) override;
		};

		Constructible<Vcpu> _vcpus[Board::VCPU_MAX];

		Registry<Revoke>::Element _elem;

		Rpc_entrypoint             &_ep;
		Accounted_ram_allocator     _ram;
		Local_rm                   &_local_rm;
		Board::Vm_page_table       &_table;
		Board::Vm_page_table_array &_table_array;
		Vmid_allocator             &_vmid_alloc;
		Guest_memory                _memory;
		Kernel::Vcpu::Identity      _id;
		unsigned                    _vcpu_id_alloc { 0 };

		void *_alloc_table();

		using Attach_result = Guest_memory::Attach_result;

		Attach_result _attach_vm_memory(addr_t, addr_t, size_t, bool,
		                                bool, Cache);
		void _detach_vm_memory(addr_t, size_t);


		/*********************************
		 ** Region_map_detach interface **
		 *********************************/

		void detach_at         (addr_t) override;
		void reserve_and_flush (addr_t) override;
		void unmap_region      (addr_t, size_t) override { /* unused */ }

	public:

		Vm_session_component(Registry<Revoke> &registry,
		                     Vmid_allocator &, Rpc_entrypoint &,
		                     Resources, Label const &, Diag,
		                     Ram_allocator &ram, Local_rm &, unsigned,
		                     Trace::Source_registry &);
		~Vm_session_component();

		void revoke_signal_context(Signal_context_capability cap) override
		{
			for (unsigned i = 0; i < Board::VCPU_MAX; i++)
				if (_vcpus[i].constructed()) _vcpus[i]->revoke_signal_context(cap);
		}


		/**************************
		 ** Vm session interface **
		 **************************/

		void attach(Dataspace_capability, addr_t, Attach_attr) override;
		void attach_pic(addr_t) override;
		void detach(addr_t, size_t) override;

		Capability<Native_vcpu> create_vcpu(Thread_capability) override;
};

#endif /* _CORE__VM_SESSION_COMPONENT_H_ */
