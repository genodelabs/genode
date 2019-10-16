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
#include <core_env.h>
#include <rm_session_component.h>
#include <map_local.h>

/* base-internal includes */
#include <base/internal/native_utcb.h>
#include <base/internal/capability_space.h>

/* kernel includes */
#include <kernel/pd.h>
#include <kernel/kernel.h>

using namespace Genode;

void Platform_thread::_init() { }


Weak_ptr<Address_space>& Platform_thread::address_space() {
	return _address_space; }


Platform_thread::~Platform_thread()
{
	/* detach UTCB of main threads */
	if (_main_thread) {
		Locked_ptr<Address_space> locked_ptr(_address_space);
		if (locked_ptr.valid())
			locked_ptr->flush((addr_t)_utcb_pd_addr, sizeof(Native_utcb),
			                  Address_space::Core_local_addr{0});
	}

	/* free UTCB */
	core_env().pd_session()->free(_utcb);
}


void Platform_thread::quota(size_t const quota)
{
	_quota = quota;
	Kernel::thread_quota(*_kobj, quota);
}


Platform_thread::Platform_thread(Label const &label, Native_utcb &utcb)
:
	_label(label),
	_pd(&Kernel::core_pd().platform_pd()),
	_pager(nullptr),
	_utcb_core_addr(&utcb),
	_utcb_pd_addr(&utcb),
	_main_thread(false),
	_kobj(true, _label.string())
{
	/* create UTCB for a core thread */
	void *utcb_phys;
	if (!platform().ram_alloc().alloc(sizeof(Native_utcb), &utcb_phys)) {
		error("failed to allocate UTCB");
		throw Out_of_ram();
	}
	map_local((addr_t)utcb_phys, (addr_t)_utcb_core_addr,
	          sizeof(Native_utcb) / get_page_size());
}


Platform_thread::Platform_thread(size_t             const  quota,
                                 Label              const &label,
                                 unsigned           const  virt_prio,
                                 Affinity::Location const  location,
                                 addr_t             const  utcb)
:
	_label(label),
	_pd(nullptr),
	_pager(nullptr),
	_utcb_pd_addr((Native_utcb *)utcb),
	_priority(_scale_priority(virt_prio)),
	_quota(quota),
	_main_thread(false),
	_kobj(true, _priority, _quota, _label.string())
{
	try {
		_utcb = core_env().pd_session()->alloc(sizeof(Native_utcb), CACHED);
	} catch (...) {
		error("failed to allocate UTCB");
		throw Out_of_ram();
	}
	_utcb_core_addr = (Native_utcb *)core_env().rm_session()->attach(_utcb);
	affinity(location);
}


void Platform_thread::join_pd(Platform_pd * pd, bool const main_thread,
                              Weak_ptr<Address_space> address_space)
{
	/* check if thread is already in another protection domain */
	if (_pd && _pd != pd) {
		error("thread already in another protection domain");
		return;
	}

	/* join protection domain */
	_pd = pd;
	_main_thread = main_thread;
	_address_space = address_space;
}


void Platform_thread::affinity(Affinity::Location const & location)
{
	_location = location;
}


Affinity::Location Platform_thread::affinity() const { return _location; }


int Platform_thread::start(void * const ip, void * const sp)
{
	/* attach UTCB in case of a main thread */
	if (_main_thread) {

		/* lookup dataspace component for physical address */
		auto lambda = [&] (Dataspace_component *dsc) {
			if (!dsc) return -1;

			/* lock the address space */
			Locked_ptr<Address_space> locked_ptr(_address_space);
			if (!locked_ptr.valid()) {
				error("invalid RM client");
				return -1;
			};
			_utcb_pd_addr = (Native_utcb *)user_utcb_main_thread();
			Hw::Address_space * as = static_cast<Hw::Address_space*>(&*locked_ptr);
			if (!as->insert_translation((addr_t)_utcb_pd_addr, dsc->phys_addr(),
			                            sizeof(Native_utcb), Hw::PAGE_FLAGS_UTCB)) {
				error("failed to attach UTCB");
				return -1;
			}
			return 0;
		};
		if (core_env().entrypoint().apply(_utcb, lambda)) return -1;
	}

	/* initialize thread registers */
	_kobj->regs->ip = reinterpret_cast<addr_t>(ip);
	_kobj->regs->sp = reinterpret_cast<addr_t>(sp);

	/* start executing new thread */
	if (!_pd) {
		error("no protection domain associated!");
		return -1;
	}

	unsigned const cpu =
		_location.valid() ? _location.xpos() : 0;

	Native_utcb &utcb = *Thread::myself()->utcb();

	/* reset capability counter */
	utcb.cap_cnt(0);
	utcb.cap_add(Capability_space::capid(_kobj.cap()));
	if (_main_thread) {
		utcb.cap_add(Capability_space::capid(_pd->parent()));
		utcb.cap_add(Capability_space::capid(_utcb));
	}
	Kernel::start_thread(*_kobj, cpu, _pd->kernel_pd(), *_utcb_core_addr);
	return 0;
}


void Platform_thread::pager(Pager_object &pager)
{
	using namespace Kernel;

	thread_pager(*_kobj, Capability_space::capid(pager.cap()));
	_pager = &pager;
}


Genode::Pager_object &Platform_thread::pager()
{
	if (_pager)
		return *_pager;

	ASSERT_NEVER_CALLED;
}


Thread_state Platform_thread::state()
{
	Thread_state bstate(*_kobj->regs);
	return Thread_state(bstate);
}


void Platform_thread::state(Thread_state thread_state)
{
	Cpu_state * cstate = static_cast<Cpu_state *>(&*_kobj->regs);
	*cstate = static_cast<Cpu_state>(thread_state);
}


void Platform_thread::restart()
{
	Kernel::restart_thread(Capability_space::capid(_kobj.cap()));
}
