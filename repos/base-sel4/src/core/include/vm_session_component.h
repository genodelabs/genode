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
#include <vm_session/vm_session.h>

/* core includes */
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

				Rpc_entrypoint                 &_ep;
				Constrained_ram_allocator      &_ram_alloc;
				Ram_dataspace_capability const  _ds_cap;
				Cap_sel                         _notification { 0 };

				void _free_up();

			public:

				Vcpu(Rpc_entrypoint &, Constrained_ram_allocator &,
				     Cap_quota_guard &, seL4_Untyped);

				~Vcpu();

				Cap_sel notification_cap() const { return _notification; }

				/*******************************
				 ** Native_vcpu RPC interface **
				 *******************************/

				Capability<Dataspace> state() const { return _ds_cap; }
		};

		using Avl_region = Allocator_avl_tpl<Rm_region>;

		Rpc_entrypoint            &_ep;
		Constrained_ram_allocator  _constrained_md_ram_alloc;
		Heap                       _heap;
		Avl_region                 _map { &_heap };
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
		void attach_pic(addr_t) override { /* unused on seL4 */ }

		void attach(Dataspace_capability, addr_t, Attach_attr) override;
		void detach(addr_t, size_t) override;
};

#endif /* _CORE__VM_SESSION_COMPONENT_H_ */
