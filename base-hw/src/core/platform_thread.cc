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
#include <platform_thread.h>
#include <platform_pd.h>
#include <core_env.h>
#include <rm_session_component.h>

/* kernel includes */
#include <kernel/kernel.h>

using namespace Genode;

void Platform_thread::_init() { }


bool Platform_thread::_attaches_utcb_by_itself()
{
	/*
	 * If this is a main thread outside of core it'll not manage its
	 * virtual context area by itself, as it is done for other threads
	 * through a sub RM-session.
	 */
	return _pd == Kernel::core_pd()->platform_pd() || !_main_thread;
}


Weak_ptr<Address_space> Platform_thread::address_space()
{
	return _address_space;
}


Platform_thread::~Platform_thread()
{
	/* detach UTCB */
	if (!_attaches_utcb_by_itself()) {

		/* the RM client may be destructed before platform thread */
		if (_rm_client) {
			Rm_session_component * const rm = _rm_client->member_rm_session();
			rm->detach(_utcb_virt);
		}
	}
	/* free UTCB */
	if (_pd == Kernel::core_pd()->platform_pd()) {
		Range_allocator * const ram = platform()->ram_alloc();
		ram->free(_utcb_phys, sizeof(Native_utcb));
	} else {
		Ram_session_component * const ram =
			dynamic_cast<Ram_session_component *>(core_env()->ram_session());
		assert(ram);
		ram->free(_utcb);
	}
	/* release from pager */
	if (_rm_client) {
		Pager_object * const object = dynamic_cast<Pager_object *>(_rm_client);
		assert(object);
		Rm_session_component * const rm = _rm_client->member_rm_session();
		assert(rm);
		Pager_capability cap = reinterpret_cap_cast<Pager_object>(object->Object_pool<Pager_object>::Entry::cap());
		rm->remove_client(cap);
	}
	/* destroy object at the kernel */
	Kernel::bin_thread(_id);
}


Platform_thread::Platform_thread(size_t const stack_size,
                                 const char * const label)
:
	_stack_size(stack_size),
	_pd(Kernel::core_pd()->platform_pd()),
	_rm_client(0),
	_utcb_virt(0),
	_main_thread(0)
{
	strncpy(_label, label, LABEL_MAX_LEN);

	/* create UTCB for a core thread */
	Range_allocator * const ram = platform()->ram_alloc();
	if (!ram->alloc_aligned(sizeof(Native_utcb), (void **)&_utcb_phys,
	                        MIN_MAPPING_SIZE_LOG2).is_ok())
	{
		PERR("failed to allocate UTCB");
		throw Cpu_session::Out_of_metadata();
	}
	_utcb_virt = _utcb_phys;

	/* set-up default start-info */
	_utcb_virt->core_start_info()->init(Processor_driver::primary_id());

	/* create kernel object */
	_id = Kernel::new_thread(_kernel_thread, Kernel::Priority::MAX, _label);
	if (!_id) {
		PERR("failed to create kernel object");
		throw Cpu_session::Thread_creation_failed();
	}
}


Platform_thread::Platform_thread(const char * const label,
                                 unsigned const virt_prio,
                                 addr_t const utcb)
:
	_stack_size(0),
	_pd(nullptr),
	_rm_client(0),
	_utcb_virt((Native_utcb *)utcb),
	_main_thread(0)
{
	strncpy(_label, label, LABEL_MAX_LEN);

	/*
	 * Allocate UTCB backing store for a thread outside of core. Page alignment
	 * is done by RAM session by default. It's save to use core env because
	 * this cannot be its server activation thread.
	 */
	Ram_session_component * const ram =
		dynamic_cast<Ram_session_component *>(core_env()->ram_session());
	assert(ram);
	try { _utcb = ram->alloc(sizeof(Native_utcb), 1); }
	catch (...) {
		PERR("failed to allocate UTCB");
		throw Cpu_session::Out_of_metadata();
	}
	_utcb_phys = (Native_utcb *)ram->phys_addr(_utcb);

	/* create kernel object */
	enum { MAX_PRIO = Kernel::Priority::MAX };
	auto const phys_prio = Cpu_session::scale_priority(MAX_PRIO, virt_prio);
	_id = Kernel::new_thread(_kernel_thread, phys_prio, _label);
	if (!_id) {
		PERR("failed to create kernel object");
		throw Cpu_session::Thread_creation_failed();
	}
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
		_utcb_virt = main_thread_utcb();
		if (!_rm_client) {
			PERR("invalid RM client");
			return -1;
		};
		Rm_session_component * const rm = _rm_client->member_rm_session();
		try { rm->attach(_utcb, 0, 0, true, _utcb_virt, 0); }
		catch (...) {
			PERR("failed to attach UTCB");
			return -1;
		}
	}
	/* initialize thread registers */
	typedef Kernel::Thread_reg_id Reg_id;
	enum { WRITES = 2 };
	addr_t * write_regs = (addr_t *)Thread_base::myself()->utcb()->base();
	write_regs[0] = Reg_id::IP;
	write_regs[1] = Reg_id::SP;
	addr_t values[] = { (addr_t)ip, (addr_t)sp };
	if (Kernel::access_thread_regs(id(), 0, WRITES, values)) {
		PERR("failed to initialize thread registers");
		return -1;
	}
	/* determine kernel name of targeted processor */
	unsigned processor_id;
	if (_location.valid()) { processor_id = _location.xpos(); }
	else { processor_id = Processor_driver::primary_id(); }

	/* start executing new thread */
	_utcb_phys->start_info()->init(_id, _utcb);
	_tlb = Kernel::start_thread(_id, processor_id, _pd->id(), _utcb_phys);
	if (!_tlb) {
		PERR("failed to start thread");
		return -1;
	}
	return 0;
}


void Platform_thread::pager(Pager_object * const pager)
{
	typedef Kernel::Thread_event_id Event_id;
	if (pager) {
		unsigned const sc_id = pager->signal_context_id();
		if (sc_id) {
			if (!Kernel::route_thread_event(id(), Event_id::FAULT, sc_id)) {
				_rm_client = dynamic_cast<Rm_client *>(pager);
				return;
			}
		}
		PERR("failed to attach signal context to fault");
		return;
	} else {
		if (!Kernel::route_thread_event(id(), Event_id::FAULT, 0)) {
			_rm_client = 0;
			return;
		}
		PERR("failed to detach signal context from fault");
		return;
	}
	return;
}


Genode::Pager_object * Platform_thread::pager()
{
	return _rm_client ? static_cast<Pager_object *>(_rm_client) : 0;
}


addr_t const * cpu_state_regs();

size_t cpu_state_regs_length();


Thread_state Platform_thread::state()
{
	static addr_t const * const src = cpu_state_regs();
	static size_t const length = cpu_state_regs_length();
	static size_t const size = length * sizeof(src[0]);
	void  * dst = Thread_base::myself()->utcb()->base();
	Genode::memcpy(dst, src, size);
	Thread_state thread_state;
	Cpu_state * const cpu_state = static_cast<Cpu_state *>(&thread_state);
	if (Kernel::access_thread_regs(id(), length, 0, (addr_t *)cpu_state)) {
		throw Cpu_session::State_access_failed();
	}
	return thread_state;
};


void Platform_thread::state(Thread_state thread_state)
{
	static addr_t const * const src = cpu_state_regs();
	static size_t const length = cpu_state_regs_length();
	static size_t const size = length * sizeof(src[0]);
	void  * dst = Thread_base::myself()->utcb()->base();
	Genode::memcpy(dst, src, size);
	Cpu_state * const cpu_state = static_cast<Cpu_state *>(&thread_state);
	if (Kernel::access_thread_regs(id(), 0, length, (addr_t *)cpu_state)) {
		throw Cpu_session::State_access_failed();
	}
};
