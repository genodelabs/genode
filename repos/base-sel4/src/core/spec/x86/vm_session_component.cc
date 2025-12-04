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
	if (!_notification.value())
		return;

	auto ret = seL4_CNode_Revoke(seL4_CapInitThreadCNode, _notification.value(), 32);
	if (ret == seL4_NoError) {
		ret = seL4_CNode_Delete(seL4_CapInitThreadCNode, _notification.value(), 32);
		if (ret == seL4_NoError) {
			platform_specific().core_sel_alloc().free(_notification);
			return;
		}
	}

	error(__func__, " failed - leaking id");
}


Vm_session_component::Vcpu::Vcpu(Rpc_entrypoint          &ep,
                                 Accounted_ram_allocator &ram_alloc,
                                 Cap_quota_guard         &cap_alloc,
                                 seL4_Untyped const       service)
:
	_ep(ep),
	_ram_alloc(ram_alloc),
	_ds(_ram_alloc.try_alloc(align_addr(sizeof(Vcpu_state), AT_PAGE), Cache::CACHED))
{
	constructed = Ok();

	_ds.with_error([&] (auto error) { constructed = error; });
	if (constructed.failed())
		return;

	/* account for notification cap */
	cap_alloc.reserve(Cap_quota{1}).with_result(
		[&] (Cap_quota_guard::Reservation &r) { r.deallocate = false; },
		[&] (Cap_quota_guard::Error) { constructed = Alloc_error::OUT_OF_CAPS; });
	if (constructed.failed())
		return;

	platform_specific().core_sel_alloc().alloc().with_result([&](auto sel) {
		auto cap_sel = Cap_sel(unsigned(sel));

		if (create<Notification_kobj>(service,
		                              platform_specific().core_cnode().sel(),
		                              cap_sel))
			_notification = Cap_sel(unsigned(sel));
		else
			platform_specific().core_sel_alloc().free(cap_sel);
	}, [](auto) { /* _notification stays invalid */ });

	_ep.manage(this);
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
                                           Ram_allocator &ram,
                                           Local_rm &local_rm,
                                           unsigned,
                                           Trace::Source_registry &)
:
	Ram_quota_guard(resources.ram_quota),
	Cap_quota_guard(resources.cap_quota),
	_ep(ep),
	_ram(ram, _ram_quota_guard(), _cap_quota_guard()),
	_memory(ep, *this, _ram, local_rm),
	_heap(_ram, local_rm)
{
	constructed = Ok();

	/* cleanup lambda on failure */
	auto failed = [&] {
		if (constructed.ok())
			return false;

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

		return true;
	};

	/* reserve for _pd_id && _vm_page_table */
	_cap_quota_guard().reserve(Cap_quota{2}).with_result(
		[&] (Cap_quota_guard::Reservation &r) { r.deallocate = false; },
		[&] (Cap_quota_guard::Error) { constructed = Session_error::OUT_OF_CAPS; });
	if (failed())
		return;

	_ram_quota_guard().reserve(Ram_quota{2 * 4096}).with_result(
		[&] (Ram_quota_guard::Reservation &r) { r.deallocate = false; },
		[&] (Ram_quota_guard::Error) { constructed = Session_error::OUT_OF_RAM; });
	if (failed())
		return;

	Platform        &platform   = platform_specific();
	Range_allocator &phys_alloc = platform.ram_alloc();

	platform_specific().core_sel_alloc().alloc().with_result([&](auto sel) {
		_vm_page_table = Cap_sel(unsigned(sel));
	}, [&] (auto) { constructed = Session_error::DENIED; });
	if (failed())
		return;

	Platform_pd::pd_id_alloc().alloc().with_result(
		[&](addr_t idx) { _pd_id = unsigned(idx); },
		[&](auto) { constructed = Session_error::DENIED; });
	if (failed())
		return;

	_vm_space.construct(_vm_page_table,
	                    platform_specific().core_sel_alloc(),
	                    phys_alloc,
	                    platform_specific().top_cnode(),
	                    platform_specific().core_cnode(),
	                    platform_specific().phys_cnode(),
	                    _pd_id, _page_table_registry, "VM");

	auto ept_phys = Untyped_memory::alloc_page(phys_alloc);

	ept_phys.with_result([&](auto &result) {
		result.deallocate = false;

		auto ept_phys    = addr_t(result.ptr);
		auto ept_service = Untyped_memory::untyped_sel(ept_phys).value();

		if (create<Ept_kobj>(ept_service, platform.core_cnode().sel(),
		                     _vm_page_table)) {
			_ept._phys    = ept_phys;
			_ept._service = ept_service;
		} else constructed = Session_error::DENIED;
	}, [&](auto) { constructed = Session_error::DENIED; });
	if (failed())
		return;

	long ret = seL4_X86_ASIDPool_Assign(platform.asid_pool().value(),
	                                    _vm_page_table.value());
	if (ret != seL4_NoError) constructed = Session_error::DENIED;
	if (failed())
		return;

	auto notify_phys = Untyped_memory::alloc_page(phys_alloc);

	notify_phys.with_result([&](auto &result) {
		result.deallocate = false;

		_notifications._phys = addr_t(result.ptr);
		_notifications._service = Untyped_memory::untyped_sel(_notifications._phys).value();
	}, [&](auto) { constructed = Session_error::DENIED; });
	if (failed())
		return;
} 

Vm_session_component::~Vm_session_component()
{
	_vcpus.for_each([&] (Vcpu &vcpu) {
		destroy(_heap, &vcpu); });

	if (_notifications._service)
		Untyped_memory::free_page(platform().ram_alloc(), _notifications._phys);

	int ret = seL4_NoError;

	if (_ept._service) {
		ret = seL4_CNode_Revoke(seL4_CapInitThreadCNode,
		                        _vm_page_table.value(), 32);
		if (ret == seL4_NoError) {
			ret = seL4_CNode_Delete(seL4_CapInitThreadCNode,
			                        _vm_page_table.value(), 32);
			if (ret == seL4_NoError)
				Untyped_memory::free_page(platform().ram_alloc(), _ept._phys);
		}

		if (ret != seL4_NoError)
			error(__func__, ": could not free ASID entry, "
			      "leaking physical memory ", ret);
	}

	if (_vm_page_table.value() && ret == seL4_NoError)
		platform_specific().core_sel_alloc().free(_vm_page_table);

	if (_pd_id)
		Platform_pd::pd_id_alloc().free(_pd_id);
}


Vm_session_component::Create_vcpu_result
Vm_session_component::create_vcpu(Thread_capability const cap)
{
	Create_vcpu_result result = Create_vcpu_error::DENIED;

	if (!cap.valid())
		return result;

	_ep.apply(cap, [&] (Cpu_thread_component *thread) {
		if (!thread)
			return;

		result = _vcpu_alloc.create(_vcpus, _ep, _ram, _cap_quota_guard(),
			                        _notifications._service)
			.template convert<Create_vcpu_result>(
				[&] (auto &a) {
					return a.obj.constructed.template convert<Create_vcpu_result>(
						[&] (auto) -> Create_vcpu_result {
							Platform_thread &pthread =
								thread->platform_thread();
							pthread.setup_vcpu(_vm_page_table,
							                   a.obj.notification_cap());

							if (seL4_TCB_BindNotification(pthread.tcb_sel().value(),
							                              a.obj.notification_cap().value())
							    != seL4_NoError)
								return Create_vcpu_error::DENIED;

							a.deallocate = false;
							return a.obj.cap();
						},
						[&] (auto error) { return error; });
				},
				[] (auto err) { return err; });
	});

	return result;
}


Vm_session_component::Attach_result
Vm_session_component::attach(Dataspace_capability const cap, addr_t const guest_phys,
                             Attach_attr attribute)
{
	auto map_fn = [&] (addr_t vm_addr, addr_t phys_addr, size_t size,
	                   bool exec, bool write, Cache cacheable)
	{
		Attach_result result = Region_map::Range(vm_addr, size);

		Vm_space::Map_attr const attr_flush {
			.cached         = (cacheable == CACHED),
			.write_combined = (cacheable == WRITE_COMBINED),
			.writeable      = write,
			.executable     = exec,
			.flush_support  = true };

		Flexpage_iterator flex(phys_addr, size, vm_addr, size, vm_addr);

		Flexpage page = flex.page();
		while (page.valid()) {

			size_t psize = 1 << page.log2_order;

			result = _vm_space->alloc_guest_page_tables(page.hotspot,
			                                            psize).convert<Attach_result>(
				[&] (auto ok) {
					return ok ? result : Attach_error::INVALID_DATASPACE; },
				[] (auto e) {
					switch(e) {
					case Alloc_error::OUT_OF_RAM:  return Attach_error::OUT_OF_RAM;
					case Alloc_error::OUT_OF_CAPS: return Attach_error::OUT_OF_CAPS;
					case Alloc_error::DENIED:      break;
					}
					return Attach_error::INVALID_DATASPACE;
				});
			if (result.failed())
				return result;

			result = _vm_space->map_guest(page.addr, page.hotspot, psize / 4096,
			                              attr_flush).convert<Attach_result>(
				[&] (auto ok) {
					return ok ? result : Attach_error::INVALID_DATASPACE; },
				[] (auto e) {
					switch(e) {
					case Alloc_error::OUT_OF_RAM:  return Attach_error::OUT_OF_RAM;
					case Alloc_error::OUT_OF_CAPS: return Attach_error::OUT_OF_CAPS;
					case Alloc_error::DENIED:      break;
					}
					return Attach_error::INVALID_DATASPACE;
				});
			if (result.failed())
				return result;

			page = flex.page();
		}

		return result;
	};

	return _memory.attach(cap, guest_phys, attribute,
	                      [&] (addr_t vm_addr, addr_t phys_addr, size_t size,
	                           bool exec, bool write, Cache cacheable) {
		return map_fn(vm_addr, phys_addr, size, exec, write, cacheable); });
}


void Vm_session_component::_detach(addr_t guest_phys, size_t size)
{
	Flexpage_iterator flex(guest_phys, size, guest_phys, size, 0);
	Flexpage page = flex.page();

	while (page.valid()) {
		_vm_space->unmap(page.addr, (1 << page.log2_order) / 4096);

		page = flex.page();
	}
}


void Vm_session_component::detach(addr_t guest_phys, size_t size)
{
	_memory.detach(guest_phys, size, [&](addr_t vm_addr, size_t size) {
		_detach(vm_addr, size); });
}


void Vm_session_component::detach_at(addr_t const addr)
{
	_memory.detach_at(addr, [&](addr_t vm_addr, size_t size) {
		_detach(vm_addr, size); });
}


void Vm_session_component::reserve_and_flush(addr_t const addr)
{
	_memory.reserve_and_flush(addr, [&](addr_t vm_addr, size_t size) {
		_detach(vm_addr, size); });
}
