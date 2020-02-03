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

/* Base includes */
#include <base/cache.h>
#include <cpu/vm_state.h>
#include <util/list.h>
#include <util/flex_iterator.h>

/* Core includes */
#include <core_env.h>
#include <cpu_thread_component.h>
#include <dataspace_component.h>
#include <vm_session_component.h>
#include <platform.h>
#include <pager.h>
#include <util.h>

#include <nova/cap_map.h>

/* NOVA includes */
#include <nova/syscalls.h>

using Genode::addr_t;
using Genode::Vm_session_component;
using Vcpu_id = Genode::Vm_session::Vcpu_id;

enum { CAP_RANGE_LOG2 = 2, CAP_RANGE = 1 << CAP_RANGE_LOG2 };

Vm_session_component::Vcpu::Vcpu(Constrained_ram_allocator &ram_alloc,
                                 Cap_quota_guard &cap_alloc,
                                 Vcpu_id const id,
                                 Affinity::Location const location,
                                 Session_label const &label,
                                 Trace::Control_area &trace_control_area,
                                 Trace::Source_registry &trace_sources)
:
	_ram_alloc(ram_alloc),
	_cap_alloc(cap_alloc),
	_trace_sources(trace_sources),
	_sel_sm_ec_sc(invalid()),
	_id(id),
	_location(location),
	_label(label),
	_trace_control_slot(trace_control_area)
{
	/* account caps required to setup vCPU */
	_cap_alloc.withdraw(Cap_quota{CAP_RANGE});

	/* now try to allocate cap indexes */
	_sel_sm_ec_sc = cap_map().insert(CAP_RANGE_LOG2);
	if (_sel_sm_ec_sc == invalid()) {
		error("out of caps in core");
		_cap_alloc.replenish(Cap_quota{CAP_RANGE});
		return;
	}

	try {
		/* create ds for vCPU state */
		_ds_cap = _ram_alloc.alloc(align_addr(sizeof(Genode::Vm_state), 12),
		                           Cache_attribute::CACHED);
	} catch (...) {
		_cap_alloc.replenish(Cap_quota{CAP_RANGE});
		cap_map().remove(_sel_sm_ec_sc, CAP_RANGE_LOG2);
		throw;
	}

	_trace_sources.insert(&_trace_source);
}

Vm_session_component::Vcpu::~Vcpu()
{
	_trace_sources.remove(&_trace_source);

	if (_ds_cap.valid())
		_ram_alloc.free(_ds_cap);

	if (_sel_sm_ec_sc != invalid()) {
		_cap_alloc.replenish(Cap_quota{CAP_RANGE});
		cap_map().remove(_sel_sm_ec_sc, CAP_RANGE_LOG2);
	}
}

addr_t Vm_session_component::Vcpu::new_pt_id()
{
	enum { MAX_VM_EXITS = (1U << Nova::NUM_INITIAL_VCPU_PT_LOG2) };
	if (_vm_pt_cnt >= MAX_VM_EXITS)
		return invalid();

	return MAX_VM_EXITS * _id.id + _vm_pt_cnt ++;
}

Genode::Trace::Source::Info Vm_session_component::Vcpu::trace_source_info() const
{
	Genode::uint64_t sc_time = 0;

	uint8_t res = Nova::sc_ctrl(sc_sel(), sc_time);
	if (res != Nova::NOVA_OK)
		warning("sc_ctrl failed res=", res);

	return { _label, String<5>("vCPU"),
	         Trace::Execution_time(sc_time, sc_time),
	         _location };
}


static Nova::uint8_t map_async_caps(Nova::Obj_crd const src,
                                    Nova::Obj_crd const dst,
                                    addr_t const dst_pd)
{
	using Nova::Utcb;
	using Genode::Thread;

	Utcb &utcb = *reinterpret_cast<Utcb *>(Thread::myself()->utcb());
	addr_t const src_pd = Genode::platform_specific().core_pd_sel();

	utcb.set_msg_word(0);
	/* ignore return value as one item always fits into the utcb */
	bool const ok = utcb.append_item(src, 0);
	(void)ok;

	/* asynchronously map capabilities */
	return Nova::delegate(src_pd, dst_pd, dst);
}

static Nova::uint8_t kernel_quota_upgrade(addr_t const pd_target)
{
	using Genode::Pager_object;

	return Pager_object::handle_oom(Pager_object::SRC_CORE_PD, pd_target,
	                                "core", "ep",
	                                Pager_object::Policy::UPGRADE_CORE_TO_DST);
}

template <typename FUNC>
static Genode::uint8_t _with_kernel_quota_upgrade(addr_t const pd_target,
                                                  FUNC const &func)
{
	Genode::uint8_t res;
	do {
		res = func();
	} while (res == Nova::NOVA_PD_OOM &&
	         Nova::NOVA_OK == kernel_quota_upgrade(pd_target));
	return res;
}

Vcpu_id Vm_session_component::_create_vcpu(Thread_capability cap)
{
	Vcpu_id ret;
	if (!cap.valid()) return ret;

	/* lookup vmm pd and cpu location of handler thread in VMM */
	addr_t             kernel_cpu_id = 0;
	Affinity::Location vcpu_location;

	auto lambda = [&] (Cpu_thread_component *ptr) {
		if (!ptr)
			return Vcpu::invalid();

		Cpu_thread_component &thread = *ptr;

		vcpu_location = thread.platform_thread().affinity();
		kernel_cpu_id = platform_specific().kernel_cpu_id(thread.platform_thread().affinity());

		return thread.platform_thread().pager().pd_sel();
	};
	addr_t const vmm_pd_sel = _ep.apply(cap, lambda);

	/* if VMM pd lookup failed then deny to create vCPU */
	if (!vmm_pd_sel || vmm_pd_sel == Vcpu::invalid())
		return ret;

	/* allocate vCPU object */
	Vcpu &vcpu = *new (_heap) Vcpu(_constrained_md_ram_alloc,
	                               _cap_quota_guard(),
	                               Vcpu_id {_id_alloc},
	                               vcpu_location,
	                               _session_label,
	                               _trace_control_area,
	                               _trace_sources);

	/* we ran out of caps in core */
	if (!vcpu.ds_cap().valid())
		return ret;

	/* core PD selector */
	addr_t const core_pd = platform_specific().core_pd_sel();

	/* setup vCPU resources */
	uint8_t res = _with_kernel_quota_upgrade(_pd_sel, [&] {
		return Nova::create_sm(vcpu.sm_sel(), core_pd, 0);
	});

	if (res != Nova::NOVA_OK) {
		error("create_sm = ", res);
		destroy(_heap, &vcpu);
		return ret;
	}

	addr_t const event_base = (1U << Nova::NUM_INITIAL_VCPU_PT_LOG2) * _id_alloc;
	enum { THREAD_GLOBAL = true, NO_UTCB = 0, NO_STACK = 0 };
	res = _with_kernel_quota_upgrade(_pd_sel, [&] {
		return Nova::create_ec(vcpu.ec_sel(), _pd_sel, kernel_cpu_id,
		                       NO_UTCB, NO_STACK, event_base, THREAD_GLOBAL);
	});

	if (res != Nova::NOVA_OK) {
		error("create_ec = ", res);
		destroy(_heap, &vcpu);
		return ret;
	}

	addr_t const dst_sm_ec_sel = Nova::NUM_INITIAL_PT_RESERVED
	                             + _id_alloc * CAP_RANGE;

	res = _with_kernel_quota_upgrade(vmm_pd_sel, [&] {
		using namespace Nova;

		enum { CAP_LOG2_COUNT = 1 };
		int permission = Obj_crd::RIGHT_EC_RECALL | Obj_crd::RIGHT_SM_UP |
		                 Obj_crd::RIGHT_SM_DOWN;
		Obj_crd const src(vcpu.sm_sel(), CAP_LOG2_COUNT, permission);
		Obj_crd const dst(dst_sm_ec_sel, CAP_LOG2_COUNT);

		return map_async_caps(src, dst, vmm_pd_sel);
	});

	if (res != Nova::NOVA_OK)
	{
		error("map sm ", res, " ", _id_alloc);
		destroy(_heap, &vcpu);
		return ret;
	}

	_vcpus.insert(&vcpu);
	_id_alloc++;
	return vcpu.id();
}

void Vm_session_component::_run(Vcpu_id const vcpu_id)
{
	Vcpu * ptr = _lookup(vcpu_id);
	if (!ptr)
		return;

	Vcpu &vcpu = *ptr;

	if (!vcpu.init())
		return;

	uint8_t res = _with_kernel_quota_upgrade(_pd_sel, [&] {
		return Nova::create_sc(vcpu.sc_sel(), _pd_sel, vcpu.ec_sel(),
		                       Nova::Qpd(Nova::Qpd::DEFAULT_QUANTUM, _priority));
	});

	if (res == Nova::NOVA_OK)
		vcpu.alive();
	else
		error("create_sc=", res);
}

void Vm_session_component::_exception_handler(Signal_context_capability const cap,
                                              Vcpu_id const vcpu_id)
{
	if (!cap.valid())
		return;

	Vcpu * ptr = _lookup(vcpu_id);
	if (!ptr)
		return;

	Vcpu &vcpu = *ptr;

	addr_t const pt = vcpu.new_pt_id();
	if (pt == Vcpu::invalid())
		return;

	uint8_t res = _with_kernel_quota_upgrade(_pd_sel, [&] {
		Nova::Obj_crd const src(cap.local_name(), 0);
		Nova::Obj_crd const dst(pt, 0);

		return map_async_caps(src, dst, _pd_sel);
	});

	if (res != Nova::NOVA_OK)
		error("map pt ", res, " failed");
}

Genode::Dataspace_capability Vm_session_component::_cpu_state(Vcpu_id const vcpu_id)
{
	Vcpu * ptr = _lookup(vcpu_id);
	if (!ptr)
		return Dataspace_capability();

	Vcpu &vcpu = *ptr;
	return vcpu.ds_cap();
}

Vm_session_component::Vm_session_component(Rpc_entrypoint &ep,
                                           Resources resources,
                                           Label const &label,
                                           Diag,
                                           Ram_allocator &ram,
                                           Region_map &local_rm,
                                           unsigned const priority,
                                           Trace::Source_registry &trace_sources)
:
	Ram_quota_guard(resources.ram_quota),
	Cap_quota_guard(resources.cap_quota),
	_ep(ep),
	_trace_control_area(ram, local_rm), _trace_sources(trace_sources),
	_constrained_md_ram_alloc(ram, _ram_quota_guard(), _cap_quota_guard()),
	_heap(_constrained_md_ram_alloc, local_rm),
	_priority(scale_priority(priority, "VM session")),
	_session_label(label)
{
	_cap_quota_guard().withdraw(Cap_quota{1});

	_pd_sel = cap_map().insert();
	if (!_pd_sel || _pd_sel == Vcpu::invalid())
		throw Service_denied();

	addr_t const core_pd = platform_specific().core_pd_sel();
	enum { KEEP_FREE_PAGES_NOT_AVAILABLE_FOR_UPGRADE = 2, UPPER_LIMIT_PAGES = 32 };
	uint8_t res = Nova::create_pd(_pd_sel, core_pd, Nova::Obj_crd(),
	                              KEEP_FREE_PAGES_NOT_AVAILABLE_FOR_UPGRADE,
	                              UPPER_LIMIT_PAGES);
	if (res != Nova::NOVA_OK) {
		error("create_pd = ", res);
		cap_map().remove(_pd_sel, 0, true);
		throw Service_denied();
	}

	/* configure managed VM area */
	_map.add_range(0, 0UL - 0x1000);
	_map.add_range(0UL - 0x1000, 0x1000);
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

	if (_pd_sel && _pd_sel != Vcpu::invalid())
		cap_map().remove(_pd_sel, 0, true);
}

void Vm_session_component::_attach_vm_memory(Dataspace_component &dsc,
                                             addr_t const guest_phys,
                                             Attach_attr const attribute)
{
	using Nova::Utcb;
	Utcb & utcb = *reinterpret_cast<Utcb *>(Thread::myself()->utcb());
	addr_t const src_pd = platform_specific().core_pd_sel();

	Flexpage_iterator flex(dsc.phys_addr() + attribute.offset, attribute.size,
	                       guest_phys, attribute.size, guest_phys);

	Flexpage page = flex.page();
	while (page.valid()) {
		Nova::Rights const map_rights (true,
		                               dsc.writable() && attribute.writeable,
		                               attribute.executable);
		Nova::Mem_crd const mem(page.addr >> 12, page.log2_order - 12,
		                        map_rights);

		utcb.set_msg_word(0);
		/* ignore return value as one item always fits into the utcb */
		bool const ok = utcb.append_item(mem, 0, true, true);
		(void)ok;

		/* receive window in destination pd */
		Nova::Mem_crd crd_mem(page.hotspot >> 12, page.log2_order - 12,
		                      map_rights);

		/* asynchronously map memory */
		uint8_t res = _with_kernel_quota_upgrade(_pd_sel, [&] {
			return Nova::delegate(src_pd, _pd_sel, crd_mem); });

		if (res != Nova::NOVA_OK)
			error("could not map VM memory ", res);

		page = flex.page();
	}
}

void Vm_session_component::_detach_vm_memory(addr_t guest_phys, size_t size)
{
	Nova::Rights const revoke_rwx(true, true, true);

	Flexpage_iterator flex(guest_phys, size, guest_phys, size, 0);
	Flexpage page = flex.page();

	while (page.valid()) {
		Nova::Mem_crd mem(page.addr >> 12, page.log2_order - 12, revoke_rwx);
		Nova::revoke(mem, true, true, _pd_sel);

		page = flex.page();
	}
}
