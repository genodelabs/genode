/*
 * \brief   Thread facility
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2012-02-12
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <assert.h>
#include <platform_thread.h>
#include <platform_pd.h>
#include <core_env.h>
#include <rm_session_component.h>
#include <map_local.h>

#include <kernel/pd.h>

/* kernel includes */
#include <kernel/kernel.h>

using namespace Genode;

void Platform_thread::_init() { }


Weak_ptr<Address_space> Platform_thread::address_space() {
	return _address_space; }


Platform_thread::~Platform_thread()
{
	/* detach UTCB of main threads */
	if (_main_thread) {
		Locked_ptr<Address_space> locked_ptr(_address_space);
		if (locked_ptr.is_valid())
			locked_ptr->flush((addr_t)_utcb_pd_addr, sizeof(Native_utcb));
	}

	/* free UTCB */
	core_env()->ram_session()->free(_utcb);
}


void Platform_thread::quota(size_t const quota) {
	Kernel::thread_quota(kernel_object(), quota); }


Platform_thread::Platform_thread(const char * const label,
                                 Native_utcb * utcb)
: Kernel_object<Kernel::Thread>(true, Kernel::Cpu_priority::MAX, 0, _label),
  _pd(Kernel::core_pd()->platform_pd()),
  _pager(nullptr),
  _utcb_core_addr(utcb),
  _utcb_pd_addr(utcb),
  _main_thread(false)
{
	strncpy(_label, label, LABEL_MAX_LEN);

	/* create UTCB for a core thread */
	void *utcb_phys;
	if (!platform()->ram_alloc()->alloc(sizeof(Native_utcb), &utcb_phys)) {
		PERR("failed to allocate UTCB");
		throw Cpu_session::Out_of_metadata();
	}
	map_local((addr_t)utcb_phys, (addr_t)_utcb_core_addr,
	          sizeof(Native_utcb) / get_page_size());
}


Platform_thread::Platform_thread(size_t const quota,
                                 const char * const label,
                                 unsigned const virt_prio,
                                 addr_t const utcb)
: Kernel_object<Kernel::Thread>(true, _priority(virt_prio), quota, _label),
  _pd(nullptr),
  _pager(nullptr),
  _utcb_pd_addr((Native_utcb *)utcb),
  _main_thread(false)
{
	strncpy(_label, label, LABEL_MAX_LEN);

	try {
		_utcb = core_env()->ram_session()->alloc(sizeof(Native_utcb),
		                                         CACHED);
	} catch (...) {
		PERR("failed to allocate UTCB");
		throw Cpu_session::Out_of_metadata();
	}
	_utcb_core_addr = (Native_utcb *)core_env()->rm_session()->attach(_utcb);
}


int Platform_thread::join_pd(Platform_pd * pd, bool const main_thread,
                             Weak_ptr<Address_space> address_space)
{
	/* check if thread is already in another protection domain */
	if (_pd && _pd != pd) {
		PERR("thread already in another protection domain");
		return -1;
	}

	/* join protection domain */
	_pd = pd;
	_main_thread = main_thread;
	_address_space = address_space;
	return 0;
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
		Rpc_entrypoint * ep = core_env()->entrypoint();
		Object_pool<Dataspace_component>::Guard dsc(ep->lookup_and_lock(_utcb));
		if (!dsc) return -1;

		/* lock the address space */
		Locked_ptr<Address_space> locked_ptr(_address_space);
		if (!locked_ptr.is_valid()) {
			PERR("invalid RM client");
			return -1;
		};
		Page_flags const flags = Page_flags::apply_mapping(true, CACHED, false);
		_utcb_pd_addr           = utcb_main_thread();
		Hw::Address_space * as = static_cast<Hw::Address_space*>(&*locked_ptr);
		if (!as->insert_translation((addr_t)_utcb_pd_addr, dsc->phys_addr(),
		                            sizeof(Native_utcb), flags)) {
			PERR("failed to attach UTCB");
			return -1;
		}
	}

	/* initialize thread registers */
	typedef Kernel::Thread_reg_id Reg_id;
	enum { WRITES = 2 };
	addr_t * write_regs = (addr_t*) Thread_base::myself()->utcb()->base();
	write_regs[0] = Reg_id::IP;
	write_regs[1] = Reg_id::SP;
	addr_t values[] = { (addr_t)ip, (addr_t)sp };
	if (Kernel::access_thread_regs(kernel_object(), 0, WRITES, values)) {
		PERR("failed to initialize thread registers");
		return -1;
	}

	/* start executing new thread */
	if (!_pd) {
		PWRN("No protection domain associated!");
		return -1;
	}

	unsigned const cpu =
		_location.valid() ? _location.xpos() : Cpu::primary_id();

	Native_utcb * utcb = Thread_base::myself()->utcb();

	/* reset capability counter */
	utcb->cap_cnt(0);
	utcb->cap_add(_cap.dst());
	if (_main_thread) {
		utcb->cap_add(_pd->parent().dst());
		utcb->cap_add(_utcb.dst());
	}
	Kernel::start_thread(kernel_object(), cpu, _pd->kernel_pd(),
	                     _utcb_core_addr);
	return 0;
}


void Platform_thread::pager(Pager_object * const pager)
{
	using namespace Kernel;

	if (route_thread_event(kernel_object(), Thread_event_id::FAULT,
	                       pager ? pager->cap().dst() : cap_id_invalid()))
		PERR("failed to set pager object for thread %s", label());

	_pager = pager;
}


Genode::Pager_object * Platform_thread::pager() { return _pager; }


addr_t const * cpu_state_regs();

size_t cpu_state_regs_length();


Thread_state Platform_thread::state()
{
	static addr_t const * const src = cpu_state_regs();
	static size_t const length = cpu_state_regs_length();
	static size_t const size = length * sizeof(src[0]);
	void  * dst = (void*)Thread_base::myself()->utcb()->base();
	Genode::memcpy(dst, src, size);
	Thread_state thread_state;
	Cpu_state * const cpu_state = static_cast<Cpu_state *>(&thread_state);
	if (Kernel::access_thread_regs(kernel_object(), length, 0,
	                               (addr_t *)cpu_state)) {
		throw Cpu_session::State_access_failed();
	}
	return thread_state;
};


void Platform_thread::state(Thread_state thread_state)
{
	static addr_t const * const src = cpu_state_regs();
	static size_t const length = cpu_state_regs_length();
	static size_t const size = length * sizeof(src[0]);
	void  * dst = (void*)Thread_base::myself()->utcb()->base();
	Genode::memcpy(dst, src, size);
	Cpu_state * const cpu_state = static_cast<Cpu_state *>(&thread_state);
	if (Kernel::access_thread_regs(kernel_object(), 0, length,
	                               (addr_t *)cpu_state)) {
		throw Cpu_session::State_access_failed();
	}
};
