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
#include <cpu_thread_component.h>
#include <guest_memory.h>
#include <kernel/vcpu.h>
#include <object.h>
#include <page_table_allocator.h>
#include <phys_allocated.h>
#include <trace/source_registry.h>
#include <vmid_allocator.h>


namespace Core { template <typename> class Vm_session_component; }


template <typename TABLE>
class Core::Vm_session_component
:
	public  Session_object<Vm_session>,
	private Region_map_detach,
	public  Revoke
{
	private:

		struct Vcpu : public Rpc_object<Vm_session::Native_vcpu, Vcpu>,
		              public Revoke
		{
			constexpr size_t ds_size() {
				return align_addr(sizeof(Vcpu_state), get_page_size_log2()); }

			Kernel::Vcpu::Identity &id;
			Rpc_entrypoint         &ep;
			Local_rm               &rm;

			Ram_allocator::Result ds;

			Local_rm::Attachment::Attempt ds_attached {
				ds.convert<Local_rm::Attachment::Attempt>(
					[&] (Ram::Allocation &a) {
						Region_map::Attr attr { };
						attr.writeable = true;
						return rm.attach(a.cap, attr);
					}, [] (auto) {
						return Region_map::Attach_error::INVALID_DATASPACE; })
			};

			Affinity::Location location;
			Board::Vcpu_state  hw_state;

			Signal_context_capability   sigh_cap { };
			Kernel_object<Kernel::Vcpu> kobj     { };

			using Constructed = Attempt<Ok, Alloc_error>;

			Constructed const constructed()
			{
				if (ds.failed())
					return ds.convert<Constructed>([] (auto&) { return Ok(); },
					                               [] (auto &e) { return e; });
				if (ds_attached.failed())
					return ds_attached.convert<Constructed>(
						[] (auto&) { return Ok(); },
						[] (auto&) { return Alloc_error::DENIED; });

				return hw_state.constructed.convert<Constructed>(
					[] (auto&) { return Ok(); },
					[] (auto &e) {
						using Error = Accounted_mapped_ram_allocator::Error;
						switch (e) {
						case Error::OUT_OF_RAM: return Alloc_error::OUT_OF_RAM;
						case Error::DENIED:     break;
						};
						return Alloc_error::DENIED;
				});
			}

			Vcpu(Kernel::Vcpu::Identity         &id,
			     Rpc_entrypoint                 &ep,
			     Accounted_ram_allocator        &ram,
			     Local_rm                       &local_rm,
			     Accounted_mapped_ram_allocator &core_ram,
			     Affinity::Location              location)
			:
				id(id), ep(ep), rm(local_rm),
				ds(ram.try_alloc(ds_size(), Cache::UNCACHED)),
				location(location),
				hw_state(core_ram, local_rm,
				         ds_attached.convert<Genode::Vcpu_state*>(
					[] (auto &a) { return (Vcpu_state*)a.ptr; },
					[] (auto)    { return nullptr; }))
			{
				ep.manage(this);
			}

			~Vcpu() { ep.dissolve(this); }

			void revoke_signal_context(Signal_context_capability cap) override
			{
				if (cap != sigh_cap)
					return;

				sigh_cap = Signal_context_capability();
				if (kobj.constructed()) kobj.destruct();
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

			void exception_handler(Signal_context_capability handler)
			{
				if (!handler.valid() || kobj.constructed())
					return;

				sigh_cap = handler;

				unsigned const cpu = location.xpos();

				kobj.create(cpu, (void *)&hw_state,
				            Capability_space::capid(handler), id);
			}
		};

		Rpc_entrypoint &_ep;
		Local_rm       &_local_rm;
		Vmid_allocator &_id_alloc;

		Accounted_ram_allocator _ram;
		Accounted_mapped_ram_allocator _mapped_ram;
		Heap _heap { _ram, _local_rm };

		Phys_allocated<TABLE> _table { _mapped_ram };
		Page_table_allocator  _table_alloc { _mapped_ram, _heap };

		Guest_memory _memory { _ep, *this, _ram, _local_rm };

		Registry<Registered<Vcpu>> _vcpus { };

		Registry<Revoke>::Element _elem;

		Kernel::Vcpu::Identity _id { _id_alloc.alloc(),
		                             (void *)_table.phys_addr() };

		using Error = Accounted_mapped_ram_allocator::Error;
		using Constructed = Attempt<Ok, Error>;

		Constructed const constructed { _table.constructed.failed()
			? _table.constructed
			: _id.id.convert<Constructed>([] (auto) { return Ok(); },
			                              [] (auto) { return Error::DENIED; })
		};

		using Attach_result = Guest_memory::Attach_result;

		Attach_result _attach(addr_t vm_addr, addr_t phys_addr,
                              size_t size, bool exec, bool write,
                              Cache cache)
		{
			using namespace Hw;

			Page_flags pflags { write ? RW : RO,
			                    exec ? EXEC : NO_EXEC,
			                    USER, NO_GLOBAL, RAM, cache };

			Attach_result result = Attach_result::OK;

			_table.obj([&] (auto &table) {
				table.insert(vm_addr, phys_addr, size,
				              pflags, _table_alloc).with_error(
					[&] (Page_table_error e) {
						if (e == Page_table_error::INVALID_RANGE) {
							result = Attach_result::INVALID_DS;
						} else {
							result = Attach_result::OUT_OF_RAM;
							error("Translation table needs to much RAM");
						}
					});
				});
			return result;
		}

		void _detach(addr_t vm_addr, size_t size)
		{
			_table.obj([&] (auto &table) {
				table.remove(vm_addr, size, _table_alloc); });
		}


		/*********************************
		 ** Region_map_detach interface **
		 *********************************/

		void detach_at(addr_t const addr) override
		{
			_memory.detach_at(addr, [&](addr_t vm_addr, size_t size) {
				_detach(vm_addr, size); });
		}

		void reserve_and_flush(addr_t const addr) override
		{
			_memory.reserve_and_flush(addr, [&](addr_t vm_addr, size_t size) {
				_detach(vm_addr, size); });
		}

		void unmap_region (addr_t, size_t) override { /* unused */ }

	public:

		/*
		 * Noncopyable
		 */
		Vm_session_component(Vm_session_component const &) = delete;
		Vm_session_component &operator = (Vm_session_component const &) = delete;

		Vm_session_component(Registry<Revoke>       &registry,
		                     Vmid_allocator         &id_alloc,
		                     Rpc_entrypoint         &ep,
		                     Resources               resources,
		                     Label const            &label,
		                     Diag                    diag,
		                     Ram_allocator          &ram,
		                     Mapped_ram_allocator   &mapped_ram,
		                     Local_rm               &rm,
		                     Trace::Source_registry &)
		:
			Session_object(ep, resources, label, diag),
			_ep(ep),
			_local_rm(rm),
			_id_alloc(id_alloc),
			_ram(ram, _ram_quota_guard(), _cap_quota_guard()),
			_mapped_ram(mapped_ram, _ram_quota_guard()),
			_elem(registry, *this)
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
		            Attach_attr attribute) override
		{
			Attach_result ret =
				_memory.attach(cap, guest_phys, attribute,
				               [&] (addr_t vm_addr, addr_t phys_addr,
				                    size_t size, bool exec, bool write,
				                    Cache cacheable) {
					return _attach(vm_addr, phys_addr, size, exec,
					               write, cacheable); });

			switch(ret) {
			case Attach_result::OK             : return;
			case Attach_result::INVALID_DS     : throw Invalid_dataspace();
			case Attach_result::OUT_OF_RAM     : throw Out_of_ram();
			case Attach_result::OUT_OF_CAPS    : throw Out_of_caps();
			case Attach_result::REGION_CONFLICT: throw Region_conflict();
			};
		}

		void attach_pic(addr_t) override;

		void detach(addr_t guest_phys, size_t size) override
		{
			_memory.detach(guest_phys, size, [&](addr_t vm_addr, size_t size) {
				_detach(vm_addr, size); });
		}

		Capability<Native_vcpu> create_vcpu(Thread_capability tcap) override
		{
			Affinity::Location vcpu_location;
			_ep.apply(tcap, [&] (Cpu_thread_component *ptr) {
				if (!ptr) return;
				vcpu_location = ptr->platform_thread().affinity();
			});

			Vcpu &vcpu = *new (_heap)
				Registered<Vcpu>(_vcpus, _id, _ep, _ram,
				                 _local_rm, _mapped_ram, vcpu_location);
			return vcpu.constructed().template convert<Capability<Native_vcpu>>(
				[&] (Ok) { return vcpu.cap(); },
				[&] (auto e) {
					destroy(_heap, &vcpu);
					switch (e) {
					case Alloc_error::OUT_OF_CAPS: throw Out_of_caps();
					case Alloc_error::OUT_OF_RAM:  throw Out_of_ram();
					case Alloc_error::DENIED:      break;
					};
					return Capability<Native_vcpu>(); });
		}
};

#endif /* _CORE__VM_SESSION_COMPONENT_H_ */
