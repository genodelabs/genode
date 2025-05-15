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

/* base includes */
#include <util/flex_iterator.h>
#include <cpu/vcpu_state.h>

/* core includes */
#include <vm_session_component.h>
#include <pd_session_component.h>
#include <cpu_thread_component.h>
#include <arch_kernel_object.h>

using namespace Core;


/********************************
 ** Vm_session_component::Vcpu **
 ********************************/

void Vm_session_component::Vcpu::_free_up()
{
	if (_notification.value()) {
		int ret = seL4_CNode_Delete(seL4_CapInitThreadCNode,
	                                _notification.value(), 32);
		if (ret == seL4_NoError)
			platform_specific().core_sel_alloc().free(_notification);
		else
			error(__func__, " cnode delete error ", ret);
	}
}


Vm_session_component::Vcpu::Vcpu(Rpc_entrypoint          &ep,
                                 Accounted_ram_allocator &ram_alloc,
                                 Cap_quota_guard         &cap_alloc,
                                 seL4_Untyped const       service)
:
	_ep(ep),
	_ram_alloc(ram_alloc),
	_ds(_ram_alloc.try_alloc(align_addr(sizeof(Vcpu_state), 12), Cache::CACHED))
{
	_ds.with_error([] (Ram::Error e) { throw_exception(e); });

	/* account for notification cap */
	Cap_quota_guard::Result caps = cap_alloc.reserve(Cap_quota{1});
	if (caps.failed())
		throw Out_of_caps();

	_notification = platform_specific().core_sel_alloc().alloc();
	create<Notification_kobj>(service,
	                          platform_specific().core_cnode().sel(),
	                          _notification);

	_ep.manage(this);

	caps.with_result([&] (Cap_quota_guard::Reservation &r) { r.deallocate = false; },
	                 [&] (Cap_quota_guard::Error) { /* handled at 'reserve' */ });
}


Vm_session_component::Vcpu::~Vcpu()
{
	_ep.dissolve(this);
	_free_up();
}


/**************************
 ** Vm_session_component **
 **************************/

Vm_session_component::Vm_session_component(Rpc_entrypoint &ep,
                                           Resources resources,
                                           Label const &,
                                           Diag,
                                           Ram_allocator &ram,
                                           Local_rm &local_rm,
                                           unsigned,
                                           Trace::Source_registry &)
try
:
	Ram_quota_guard(resources.ram_quota),
	Cap_quota_guard(resources.cap_quota),
	_ep(ep),
	_ram(ram, _ram_quota_guard(), _cap_quota_guard()),
	_heap(_ram, local_rm),
	_vm_page_table(platform_specific().core_sel_alloc().alloc())
{
	Platform        &platform   = platform_specific();
	Range_allocator &phys_alloc = platform.ram_alloc();

	Platform_pd::pd_id_alloc().alloc().with_result(
		[&](addr_t idx) { _pd_id = unsigned(idx); },
		[ ](auto      ) { throw Service_denied(); });

	_vm_space.construct(_vm_page_table,
	                    platform_specific().core_sel_alloc(),
	                    phys_alloc,
	                    platform_specific().top_cnode(),
	                    platform_specific().core_cnode(),
	                    platform_specific().phys_cnode(),
	                    _pd_id, _page_table_registry, "VM");

	/* _pd_id && _vm_page_table */
	Cap_quota_guard::Result cap_reservation = _cap_quota_guard().reserve(Cap_quota{2});
	Ram_quota_guard::Result ram_reservation = _ram_quota_guard().reserve(Ram_quota{2 * 4096});

	if (cap_reservation.failed()) throw Out_of_caps();
	if (ram_reservation.failed()) throw Out_of_ram();

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
	(void)_map.add_range(0, 0UL - 0x1000);
	(void)_map.add_range(0UL - 0x1000, 0x1000);

	/* errors handled at 'reserve' */
	cap_reservation.with_result([&] (Cap_quota_guard::Reservation &r) { r.deallocate = false; },
	                            [&] (Cap_quota_guard::Error) { });
	ram_reservation.with_result([&] (Ram_quota_guard::Reservation &r) { r.deallocate = false; },
	                            [&] (Ram_quota_guard::Error) { });
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
	_vcpus.for_each([&] (Vcpu &vcpu) {
		destroy(_heap, &vcpu); });

	/* detach all regions */
	while (true) {
		addr_t out_addr = 0;

		if (!_map.any_block_addr(&out_addr))
			break;

		detach_at(out_addr);
	}

	if (_vm_page_table.value())
		platform_specific().core_sel_alloc().free(_vm_page_table);

	if (_pd_id)
		Platform_pd::pd_id_alloc().free(_pd_id);
}


Capability<Vm_session::Native_vcpu> Vm_session_component::create_vcpu(Thread_capability const cap)
{
	if (!cap.valid())
		return { };

	Vcpu * vcpu = nullptr;

	auto lambda = [&] (Cpu_thread_component *thread) {
		if (!thread)
			return;

		/* code to revert partial allocations in case of Out_of_ram/_quota */
		auto free_up = [&] { if (vcpu) destroy(_heap, vcpu); };

		try {
			vcpu = new (_heap) Registered<Vcpu>(_vcpus,
			                                    _ep,
			                                    _ram,
			                                    _cap_quota_guard(),
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
			error("unexpected exception occurred");
			free_up();
			return;
		}
	};

	_ep.apply(cap, lambda);

	return vcpu ? vcpu->cap() : Capability<Vm_session::Native_vcpu> {};
}


void Vm_session_component::_attach_vm_memory(Dataspace_component &dsc,
                                             addr_t const guest_phys,
                                             Attach_attr const attribute)
{
	Vm_space::Map_attr const attr_noflush {
		.cached         = (dsc.cacheability() == CACHED),
		.write_combined = (dsc.cacheability() == WRITE_COMBINED),
		.writeable      = dsc.writeable() && attribute.writeable,
		.executable     = attribute.executable,
		.flush_support  = false };

	Flexpage_iterator flex(dsc.phys_addr() + attribute.offset, attribute.size,
	                       guest_phys, attribute.size, guest_phys);

	Flexpage page = flex.page();
	while (page.valid()) {
		try {
			if (!_vm_space->alloc_guest_page_tables(page.hotspot, 1 << page.log2_order))
				throw Service_denied();

			_vm_space->map_guest(page.addr, page.hotspot,
			                    (1 << page.log2_order) / 4096, attr_noflush);
		} catch (Page_table_registry::Mapping_cache_full full) {
			if (full.reason == Page_table_registry::Mapping_cache_full::MEMORY)
				throw Out_of_ram();
			if (full.reason == Page_table_registry::Mapping_cache_full::CAPS)
				throw Out_of_caps();
			return;
		}

		page = flex.page();
	}
}


void Vm_session_component::_detach_vm_memory(addr_t guest_phys, size_t size)
{
	Flexpage_iterator flex(guest_phys, size, guest_phys, size, 0);
	Flexpage page = flex.page();

	while (page.valid()) {
		_vm_space->unmap(page.addr, (1 << page.log2_order) / 4096);

		page = flex.page();
	}
}
