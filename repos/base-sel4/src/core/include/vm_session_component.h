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
#include <base/rpc_server.h>
#include <base/registry.h>
#include <vm_session/vm_session.h>

/* core includes */
#include <guest_memory.h>
#include <trace/source_registry.h>
#include <sel4_native_vcpu/sel4_native_vcpu.h>

namespace Core { class Vm_session_component; }


class Core::Vm_session_component
:
	private Ram_quota_guard,
	private Cap_quota_guard,
	public  Rpc_object<Vm_session, Vm_session_component>,
	private Region_map_detach
{
	private:

		class Vcpu : public Rpc_object<Vm_session::Native_vcpu, Vcpu>
		{
			private:

				Rpc_entrypoint             &_ep;
				Accounted_ram_allocator    &_ram_alloc;
				Ram_allocator::Result const _ds;
				Cap_sel                     _notification { 0 };

				void _free_up();

			public:

				using Constructed = Attempt<Ok, Alloc_error>;
				Constructed constructed { Alloc_error::DENIED };

				Vcpu(Rpc_entrypoint &, Accounted_ram_allocator &,
				     Cap_quota_guard &, seL4_Untyped);

				~Vcpu();

				Cap_sel notification_cap() const { return _notification; }

				/*******************************
				 ** Native_vcpu RPC interface **
				 *******************************/

				Capability<Dataspace> state() const
				{
					return _ds.convert<Capability<Dataspace>>(
						[&] (Ram::Allocation const &ds) { return ds.cap; },
						[&] (Ram::Error) { return Capability<Dataspace>(); });
				}
		};

		using Vcpu_allocator =
			Memory::Constrained_obj_allocator<Registered<Vcpu>>;

		Rpc_entrypoint          &_ep;
		Accounted_ram_allocator  _ram;
		Guest_memory             _memory;
		Heap                     _heap;
		Vcpu_allocator           _vcpu_alloc { _heap };
		unsigned                 _pd_id    { 0 };
		Cap_sel                  _vm_page_table { 0 };
		Page_table_registry      _page_table_registry { _heap };
		Constructible<Vm_space>  _vm_space { };

		struct {
			addr_t       _phys;
			seL4_Untyped _service;
		}                          _ept { 0, 0 };
		struct {
			addr_t       _phys;
			seL4_Untyped _service;
		}                          _notifications { 0, 0 };

		Registry<Registered<Vcpu>> _vcpus { };

		void _detach(addr_t, size_t);


		/*********************************
		 ** Region_map_detach interface **
		 *********************************/

		/* used on destruction of attached dataspaces */
		void detach_at         (addr_t) override;
		void reserve_and_flush (addr_t) override;
		void unmap_region      (addr_t, size_t) override { /* unsused */ };

	protected:

		Ram_quota_guard &_ram_quota_guard() { return *this; }
		Cap_quota_guard &_cap_quota_guard() { return *this; }

	public:

		using Constructed = Attempt<Ok, Session_error>;
		Constructed constructed { Session_error::DENIED };

		using Ram_quota_guard::upgrade;
		using Cap_quota_guard::upgrade;

		Vm_session_component(Rpc_entrypoint &, Resources, Label const &,
		                     Ram_allocator &ram, Local_rm &, unsigned,
		                     Trace::Source_registry &);
		~Vm_session_component();


		/**************************
		 ** Vm session interface **
		 **************************/

		Create_vcpu_result create_vcpu(Thread_capability) override;
		Attach_result attach_pic(addr_t) override {
			return Attach_error::INVALID_DATASPACE; /* unused on seL4 */ }

		Attach_result attach(Dataspace_capability, addr_t, Attach_attr) override;
		void detach(addr_t, size_t) override;
};

#endif /* _CORE__VM_SESSION_COMPONENT_H_ */
