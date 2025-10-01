/*
 * \brief  VM session component for 'base-hw'
 * \author Stefan Kalkowski
 * \author Benjamin Lamowski
 * \date   2024-09-20
 */

/*
 * Copyright (C) 2015-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__VM_SESSION_COMPONENT_H_
#define _CORE__VM_SESSION_COMPONENT_H_

/* base includes */
#include <base/allocator.h>
#include <base/session_object.h>
#include <base/registry.h>
#include <vm_session/vm_session.h>
#include <dataspace/capability.h>

/* core includes */
#include <cpu_thread_component.h>
#include <region_map_component.h>
#include <trace/source_registry.h>

/* base-hw includes */
#include <spec/x86_64/ept.h>
#include <spec/x86_64/hpt.h>
#include <vcpu.h>
#include <vmid_allocator.h>
#include <guest_memory.h>
#include <phys_allocated.h>


namespace Core {
	template <typename TABLE> class Vm_session_component;
	using Vmx_session_component = Vm_session_component<Hw::Ept>;
	using Svm_session_component = Vm_session_component<Hw::Hpt>;
}


template <typename TABLE>
class Core::Vm_session_component
:
	public Session_object<Vm_session>,
	public Revoke,
	private Region_map_detach
{
	private:

		using Vm_page_table_array = typename TABLE::Array;

		/*
		 * Noncopyable
		 */
		Vm_session_component(Vm_session_component const &);
		Vm_session_component &operator = (Vm_session_component const &);

		Registry<Registered<Vcpu>>          _vcpus { };

		Registry<Revoke>::Element           _elem;
		Rpc_entrypoint                     &_ep;
		Accounted_ram_allocator             _accounted_ram_alloc;
		Accounted_mapped_ram_allocator      _accounted_mapped_ram;
		Local_rm                           &_local_rm;
		Heap                                _heap;
		Phys_allocated<TABLE>               _table;
		Page_table_allocator                _table_alloc;
		Guest_memory                        _memory;
		Vmid_allocator                     &_vmid_alloc;

		uint8_t _remaining_print_count { 10 };

		void error(auto &&... args)
		{
			if (_remaining_print_count) {
				Genode::error(args...);
				_remaining_print_count--;
			}
		}


		using Constructed = Attempt<Ok, Accounted_mapped_ram_allocator::Error>;

		Constructed const constructed = _table.constructed;

		Kernel::Vcpu::Identity _id {
			_vmid_alloc.alloc().convert<unsigned>(
				[] (addr_t const id)       -> unsigned { return unsigned(id); },
				[] (Vmid_allocator::Error) -> unsigned { throw Service_denied(); }),
			(void *)_table.phys_addr()
		};


		/*********************************
		 ** Region_map_detach interface **
		 *********************************/

		void detach_at(addr_t addr) override
		{
			_memory.detach_at(addr, [&](addr_t vm_addr, size_t size) {
				_table.obj([&] (TABLE &table) {
					table.remove(vm_addr, size, _table_alloc);
				});
			});
		}

		void reserve_and_flush(addr_t addr) override
		{
			_memory.reserve_and_flush(addr, [&](addr_t vm_addr, size_t size) {
				_table.obj([&] (TABLE &table) {
					table.remove(vm_addr, size, _table_alloc);
				});
			});
		}

		void unmap_region(addr_t, size_t) override { /* unused */ }

	public:

		Vm_session_component(Registry<Revoke> &registry,
		                     Vmid_allocator &vmid_alloc,
		                     Rpc_entrypoint &ds_ep,
		                     Resources resources,
		                     Label const &label,
		                     Diag diag,
		                     Ram_allocator &ram_alloc,
		                     Mapped_ram_allocator &mapped_ram,
		                     Local_rm &local_rm,
		                     Trace::Source_registry &)
		:
			Session_object(ds_ep, resources, label, diag),
			_elem(registry, *this),
			_ep(ds_ep),
			_accounted_ram_alloc(ram_alloc, _ram_quota_guard(), _cap_quota_guard()),
			_accounted_mapped_ram(mapped_ram, _ram_quota_guard()),
			_local_rm(local_rm),
			_heap(_accounted_ram_alloc, local_rm),
			_table(_accounted_mapped_ram),
			_table_alloc(_accounted_mapped_ram, _heap),
			_memory(ds_ep, *this, _accounted_ram_alloc, local_rm),
			_vmid_alloc(vmid_alloc)
		{
			using Error = Accounted_mapped_ram_allocator::Error;

			constructed.with_error([] (Error e) {
				switch (e) {
				case Error::OUT_OF_RAM:  throw Out_of_ram();
				case Error::DENIED:      throw Service_denied();
				};
			});
		}

		~Vm_session_component()
		{
			_vcpus.for_each([&] (Registered<Vcpu> &vcpu) {
				destroy(_heap, &vcpu); });

			(void)_vmid_alloc.free(_id.id);
		}

		void revoke_signal_context(Signal_context_capability cap) override
		{
			_vcpus.for_each([&] (Vcpu &vcpu) {
				vcpu.revoke_signal_context(cap); });
		}


		/**************************
		 ** Vm session interface **
		 **************************/

		void attach(Dataspace_capability cap, addr_t guest_phys,
		            Attach_attr attr) override
		{
			using Attach_result = Guest_memory::Attach_result;

			auto const &map_fn = [&] (addr_t vm_addr, addr_t phys_addr,
			                          size_t size, bool exec, bool write,
			                          Cache cacheable) {
				Attach_result ret = Attach_result::OK;

				Page_flags const pflags { write ? RW : RO,
				                          exec ? EXEC : NO_EXEC,
				                          USER, NO_GLOBAL, RAM,
				                          cacheable };

				_table.obj([&] (TABLE &table) {
					table.insert(vm_addr, phys_addr, size, pflags,
					             _table_alloc).with_error(
						[&] (Hw::Page_table_error e) {
							if (e == Hw::Page_table_error::INVALID_RANGE)
								ret = Attach_result::INVALID_DS;
							else {
								ret = Attach_result::OUT_OF_RAM;
								error("Translation table needs too much RAM");
							}
					});
				});
				return ret;
			};

			switch(_memory.attach(cap, guest_phys, attr, map_fn)) {
			case Attach_result::OK             : return;
			case Attach_result::INVALID_DS     : throw Invalid_dataspace();
			case Attach_result::OUT_OF_RAM     : throw Out_of_ram();
			case Attach_result::OUT_OF_CAPS    : throw Out_of_caps();
			case Attach_result::REGION_CONFLICT: throw Region_conflict();
			}
		}

		void attach_pic(addr_t) override
		{ }

		void detach(addr_t guest_phys, size_t size) override
		{
			_memory.detach(guest_phys, size, [&](addr_t vm_addr, size_t size) {
				_table.obj([&] (TABLE &table) {
					table.remove(vm_addr, size, _table_alloc); });
			});
		}

		Capability<Native_vcpu> create_vcpu(Thread_capability tcap) override
		{
			using Error = Accounted_mapped_ram_allocator::Error;

			Affinity::Location vcpu_location;
			_ep.apply(tcap, [&] (Cpu_thread_component *ptr) {
				if (!ptr) return;
				vcpu_location = ptr->platform_thread().affinity();
			});

			Vcpu &vcpu = *new (_heap)
						Registered<Vcpu>(_vcpus,
						                 _id,
						                 _ep,
						                 _accounted_ram_alloc,
						                 _local_rm,
						                 _accounted_mapped_ram,
						                 vcpu_location);
			return vcpu.constructed.convert<Capability<Native_vcpu>>(
				[&] (Ok) { return vcpu.cap(); },
				[&] (Error e) {
					destroy(_heap, &vcpu);
					switch (e) {
					case Error::OUT_OF_RAM:  throw Out_of_ram();
					case Error::DENIED:      break;
					};
					return Capability<Native_vcpu>(); });
		}
};

#endif /* _CORE__VM_SESSION_COMPONENT_H_ */
