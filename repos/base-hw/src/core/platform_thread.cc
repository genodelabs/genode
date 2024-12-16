/*
 * \brief   Thread facility
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2012-02-12
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <platform_thread.h>
#include <platform_pd.h>
#include <rm_session_component.h>
#include <map_local.h>

/* base-internal includes */
#include <base/internal/native_utcb.h>
#include <base/internal/capability_space.h>

/* kernel includes */
#include <kernel/pd.h>
#include <kernel/main.h>

using namespace Core;


addr_t Platform_thread::Utcb::_attach(Region_map &core_rm)
{
	Region_map::Attr attr { };
	attr.writeable = true;
	return core_rm.attach(_ds, attr).convert<addr_t>(
		[&] (Region_map::Range range) { return range.start; },
		[&] (Region_map::Attach_error) {
			error("failed to attach UTCB of new thread within core");
			return 0ul; });
}


static addr_t _alloc_core_local_utcb(addr_t core_addr)
{
	/*
	 * All non-core threads use the typical dataspace/rm_session
	 * mechanisms to allocate and attach its UTCB.
	 * But for the very first core threads, we need to use plain
	 * physical and virtual memory allocators to create/attach its
	 * UTCBs. Therefore, we've to allocate and map those here.
	 */
	return platform().ram_alloc().try_alloc(sizeof(Native_utcb)).convert<addr_t>(

		[&] (void *utcb_phys) {
			map_local((addr_t)utcb_phys, core_addr,
			          sizeof(Native_utcb) / get_page_size());
			return addr_t(utcb_phys);
		},
		[&] (Range_allocator::Alloc_error) {
			error("failed to allocate UTCB for core/kernel thread!");
			return 0ul;
		});
}


Platform_thread::Utcb::Utcb(addr_t core_addr)
:
	core_addr(core_addr),
	phys_addr(_alloc_core_local_utcb(core_addr))
{ }


void Platform_thread::_init() { }


Weak_ptr<Address_space>& Platform_thread::address_space() {
	return _address_space; }


void Platform_thread::quota(size_t const quota)
{
	_quota = (unsigned)quota;
	Kernel::thread_quota(*_kobj, quota);
}


Platform_thread::Platform_thread(Label const &label, Native_utcb &utcb)
:
	_label(label),
	_pd(_kernel_main_get_core_platform_pd()),
	_pager(nullptr),
	_utcb((addr_t)&utcb),
	_main_thread(false),
	_location(Affinity::Location()),
	_kobj(_kobj.CALLED_FROM_CORE, _label.string())
{ }


Platform_thread::Platform_thread(Platform_pd              &pd,
                                 Rpc_entrypoint           &ep,
                                 Ram_allocator            &ram,
                                 Region_map               &core_rm,
                                 size_t             const  quota,
                                 Label              const &label,
                                 unsigned           const  virt_prio,
                                 Affinity::Location const  location,
                                 addr_t                 /* utcb */)
:
	_label(label),
	_pd(pd),
	_pager(nullptr),
	_utcb(ep, ram, core_rm),
	_priority(_scale_priority(virt_prio)),
	_quota((unsigned)quota),
	_main_thread(!pd.has_any_thread),
	_location(location),
	_kobj(_kobj.CALLED_FROM_CORE, _priority, _quota, _label.string())
{
	_address_space = pd.weak_ptr();
	pd.has_any_thread = true;
}


Platform_thread::~Platform_thread()
{
	/* core/kernel threads have no dataspace, but plain memory as UTCB */
	if (!_utcb._ds.valid()) {
		error("UTCB of core/kernel thread gets destructed!");
		return;
	}

	/* detach UTCB of main threads */
	if (_main_thread) {
		Locked_ptr<Address_space> locked_ptr(_address_space);
		if (locked_ptr.valid())
			locked_ptr->flush(user_utcb_main_thread(), sizeof(Native_utcb),
			                  Address_space::Core_local_addr{0});
	}
}


void Platform_thread::affinity(Affinity::Location const &)
{
	/* yet no migration support, don't claim wrong location, e.g. for tracing */
}


Affinity::Location Platform_thread::affinity() const { return _location; }


void Platform_thread::start(void * const ip, void * const sp)
{
	/* attach UTCB in case of a main thread */
	if (_main_thread) {

		Locked_ptr<Address_space> locked_ptr(_address_space);
		if (!locked_ptr.valid()) {
			error("unable to start thread in invalid address space");
			return;
		};
		Hw::Address_space * as = static_cast<Hw::Address_space*>(&*locked_ptr);
		if (!as->insert_translation(user_utcb_main_thread(), _utcb.phys_addr,
		                            sizeof(Native_utcb), Hw::PAGE_FLAGS_UTCB)) {
			error("failed to attach UTCB");
			return;
		}
	}

	/* initialize thread registers */
	_kobj->regs->ip = reinterpret_cast<addr_t>(ip);
	_kobj->regs->sp = reinterpret_cast<addr_t>(sp);

	/* start executing new thread */
	unsigned const cpu = _location.xpos();

	Native_utcb &utcb = *Thread::myself()->utcb();

	/* reset capability counter */
	utcb.cap_cnt(0);
	utcb.cap_add(Capability_space::capid(_kobj.cap()));
	if (_main_thread) {
		utcb.cap_add(Capability_space::capid(_pd.parent()));
		utcb.cap_add(Capability_space::capid(_utcb._ds));
	}
	Kernel::start_thread(*_kobj, cpu, _pd.kernel_pd(), *(Native_utcb*)_utcb.core_addr);
}


void Platform_thread::pager(Pager_object &pager)
{
	using namespace Kernel;

	thread_pager(*_kobj, Capability_space::capid(pager.cap()));
	_pager = &pager;
}


Core::Pager_object &Platform_thread::pager()
{
	if (_pager)
		return *_pager;

	ASSERT_NEVER_CALLED;
}


Thread_state Platform_thread::state()
{
	Cpu_state cpu { };
	Kernel::get_cpu_state(*_kobj, cpu);

	auto state = [&] () -> Thread_state::State
	{
		using Exception_state = Kernel::Thread::Exception_state;
		switch (exception_state()) {
		case Exception_state::NO_EXCEPTION: return Thread_state::State::VALID;
		case Exception_state::MMU_FAULT:    return Thread_state::State::PAGE_FAULT;
		case Exception_state::EXCEPTION:    return Thread_state::State::EXCEPTION;
		}
		return Thread_state::State::UNAVAILABLE;
	};

	return {
		.state = state(),
		.cpu   = cpu
	};
}


void Platform_thread::state(Thread_state thread_state)
{
	Kernel::set_cpu_state(*_kobj, thread_state.cpu);
}


void Platform_thread::restart()
{
	Kernel::restart_thread(Capability_space::capid(_kobj.cap()));
}
