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

/* base includes */
#include <util/flex_iterator.h>

/* core includes */
#include <core_env.h>
#include <vm_session_component.h>
#include <pd_session_component.h>
#include <cpu_thread_component.h>
#include <arch_kernel_object.h>


using namespace Genode;

void Vm_session_component::Vcpu::_free_up()
{
	if (_ds_cap.valid())
		_ram_alloc.free(_ds_cap);

	if (_notification.value()) {
		int ret = seL4_CNode_Delete(seL4_CapInitThreadCNode,
	                                _notification.value(), 32);
		if (ret == seL4_NoError)
			platform_specific().core_sel_alloc().free(_notification);
		else
			Genode::error(__func__, " cnode delete error ", ret);
	}
}

Vm_session_component::Vcpu::Vcpu(Constrained_ram_allocator &ram_alloc,
                                 Cap_quota_guard &cap_alloc,
                                 Vcpu_id const vcpu_id,
                                 seL4_Untyped const service)
:
	_ram_alloc(ram_alloc),
	_ds_cap (_ram_alloc.alloc(4096, Cache_attribute::CACHED)),
	_vcpu_id(vcpu_id)
{
	try {
		/* notification cap */
		Cap_quota_guard::Reservation caps(cap_alloc, Cap_quota{1});

		_notification = platform_specific().core_sel_alloc().alloc();
		create<Notification_kobj>(service,
		                          platform_specific().core_cnode().sel(),
		                          _notification);

		caps.acknowledge();
	} catch (...) {
		_free_up();
		throw;
	}
}

Vm_session_component::Vm_session_component(Rpc_entrypoint &ep,
                                           Resources resources,
                                           Label const &,
                                           Diag,
                                           Ram_allocator &ram,
                                           Region_map &local_rm)
try
:
	Ram_quota_guard(resources.ram_quota),
	Cap_quota_guard(resources.cap_quota),
	_ep(ep),
	_constrained_md_ram_alloc(ram, _ram_quota_guard(), _cap_quota_guard()),
	_sliced_heap(_constrained_md_ram_alloc, local_rm),
	_heap(_constrained_md_ram_alloc, local_rm),
	_pd_id(Platform_pd::pd_id_alloc().alloc()),
	_vm_page_table(platform_specific().core_sel_alloc().alloc()),
	_vm_space(_vm_page_table,
	          platform_specific().core_sel_alloc(),
	          platform().ram_alloc(),
	          platform_specific().top_cnode(),
	          platform_specific().core_cnode(),
	          platform_specific().phys_cnode(),
	          _pd_id, _page_table_registry, "VM")
{
	Platform        &platform   = platform_specific();
	Range_allocator &phys_alloc = platform.ram_alloc();

	/* _pd_id && _vm_page_table */
	Cap_quota_guard::Reservation caps(_cap_quota_guard(), Cap_quota{2});
	/* ept object requires a page taken directly from core's phys_alloc */
	/* notifications requires a page taken directly from core's phys_alloc */
	Ram_quota_guard::Reservation ram(_ram_quota_guard(), Ram_quota{2 * 4096});

	try {
		_ept._phys    = Untyped_memory::alloc_page(phys_alloc);
		_ept._service = Untyped_memory::untyped_sel(_ept._phys).value();

		create<Ept_kobj>(_ept._service, platform.core_cnode().sel(),
		                 _vm_page_table);
	} catch (...) {
		throw Service_denied();
	}

	long ret = seL4_X86_ASIDPool_Assign(platform.asid_pool().value(),
	                                    _vm_page_table.value());
	if (ret != seL4_NoError)
		throw Service_denied();

	try {
		_notifications._phys = Untyped_memory::alloc_page(phys_alloc);
		_notifications._service = Untyped_memory::untyped_sel(_notifications._phys).value();
	} catch (...) {
		throw Service_denied();
	}

	/* configure managed VM area */
	_map.add_range(0, 0UL - 0x1000);
	_map.add_range(0UL - 0x1000, 0x1000);

	caps.acknowledge();
	ram.acknowledge();
} catch (...) {

	if (_notifications._service)
		Untyped_memory::free_page(platform().ram_alloc(), _notifications._phys);

	if (_ept._service) {
		int ret = seL4_CNode_Delete(seL4_CapInitThreadCNode,
		                            _vm_page_table.value(), 32);
		if (ret == seL4_NoError)
			Untyped_memory::free_page(platform().ram_alloc(), _ept._phys);

		if (ret != seL4_NoError)
			error(__FUNCTION__, ": could not free ASID entry, "
			      "leaking physical memory ", ret);
	}

	if (_vm_page_table.value())
		platform_specific().core_sel_alloc().free(_vm_page_table);

	if (_pd_id)
		Platform_pd::pd_id_alloc().free(_pd_id);

	throw;
}

Vm_session_component::~Vm_session_component()
{
	for (;Vcpu * vcpu = _vcpus.first();) {
		_vcpus.remove(vcpu);
		destroy(_heap, vcpu);
	}

	/* detach all regions */
	while (true) {
		addr_t out_addr = 0;

		if (!_map.any_block_addr(&out_addr))
			break;

		detach(out_addr);
	}

	if (_vm_page_table.value())
		platform_specific().core_sel_alloc().free(_vm_page_table);

	if (_pd_id)
		Platform_pd::pd_id_alloc().free(_pd_id);
}

void Vm_session_component::_create_vcpu(Thread_capability cap)
{
	if (!cap.valid())
		return;

	auto lambda = [&] (Cpu_thread_component *thread) {
		if (!thread)
			return;

		/* allocate vCPU object */
		Vcpu * vcpu = nullptr;

		/* code to revert partial allocations in case of Out_of_ram/_quota */
		auto free_up = [&] () { if (vcpu) destroy(_heap, vcpu); };

		try {
			vcpu = new (_heap) Vcpu(_constrained_md_ram_alloc,
			                               _cap_quota_guard(),
			                               Vcpu_id{_id_alloc},
			                               _notifications._service);

			Platform_thread &pthread = thread->platform_thread();
			pthread.setup_vcpu(_vm_page_table, vcpu->notification_cap());

			int ret = seL4_TCB_BindNotification(pthread.tcb_sel().value(),
			                                    vcpu->notification_cap().value());
			if (ret != seL4_NoError)
				throw 0;
		} catch (Out_of_ram) {
			free_up();
			throw;
		} catch (Out_of_caps) {
			free_up();
			throw;
		} catch (...) {
			Genode::error("unexpected exception occurred");
			free_up();
			return;
		}

		_vcpus.insert(vcpu);
		_id_alloc++;
	};

	_ep.apply(cap, lambda);
}

Dataspace_capability Vm_session_component::_cpu_state(Vcpu_id const vcpu_id)
{
	Vcpu * vcpu = _lookup(vcpu_id);
	if (!vcpu)
		return Dataspace_capability();

	return vcpu->ds_cap();
}

void Vm_session_component::_pause(Vcpu_id const vcpu_id)
{
	Vcpu * vcpu = _lookup(vcpu_id);
	if (!vcpu)
		return;

	vcpu->signal();
}

void Vm_session_component::_attach_vm_memory(Dataspace_component &dsc,
                                             addr_t const guest_phys,
                                             bool const executable,
                                             bool const writeable)
{
	_vm_space.alloc_guest_page_tables(guest_phys, dsc.size());

	enum { FLUSHABLE = true };
	_vm_space.map_guest(dsc.phys_addr(), guest_phys, dsc.size() >> 12,
	                    dsc.cacheability(),
	                    dsc.writable() && writeable,
	                    executable, FLUSHABLE);
}

void Vm_session_component::_detach_vm_memory(addr_t guest_phys, size_t size)
{
	Flexpage_iterator flex(guest_phys, size, guest_phys, size, 0);
	Flexpage page = flex.page();

	while (page.valid()) {
		_vm_space.unmap(page.addr, (1 << page.log2_order) / 4096);

		page = flex.page();
	}
}
