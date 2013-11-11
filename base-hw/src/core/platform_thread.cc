/*
 * \brief   Thread facility
 * \author  Martin Stein
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
#include <core_env.h>
#include <rm_session_component.h>

using namespace Genode;

namespace Kernel { unsigned core_id(); }


bool Platform_thread::_attaches_utcb_by_itself()
{
	/*
	 * If this is a main thread outside of core it'll not manage its
	 * virtual context area by itself, as it is done for other threads
	 * through a sub RM-session.
	 */
	return _pd_id == Kernel::core_id() || !main_thread();
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
			rm->detach(_virt_utcb);
		}
	}
	/* free UTCB */
	if (_pd_id == Kernel::core_id()) {
		Range_allocator * const ram = platform()->ram_alloc();
		ram->free((void *)_phys_utcb, sizeof(Native_utcb));
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
		Pager_capability cap = reinterpret_cap_cast<Pager_object>(object->cap());
		rm->remove_client(cap);
	}
	/* destroy object at the kernel */
	Kernel::delete_thread(_id);
}

Platform_thread::Platform_thread(const char * name,
                                 Thread_base * const thread_base,
                                 size_t const stack_size, unsigned const pd_id)
:
	_thread_base(thread_base), _stack_size(stack_size),
	_pd_id(pd_id), _rm_client(0), _virt_utcb(0),
	_priority(Kernel::Priority::MAX),
	_main_thread(0)
{
	strncpy(_name, name, NAME_MAX_LEN);

	/* create UTCB for a core thread */
	Range_allocator * const ram = platform()->ram_alloc();
	if (!ram->alloc_aligned(sizeof(Native_utcb), (void **)&_phys_utcb,
	                        MIN_MAPPING_SIZE_LOG2).is_ok())
	{
		PERR("failed to allocate UTCB");
		throw Cpu_session::Out_of_metadata();
	}
	_virt_utcb = _phys_utcb;

	/* common constructor parts */
	_init();
}


Platform_thread::Platform_thread(const char * name, unsigned int priority,
                                 addr_t utcb)
:
	_thread_base(0), _stack_size(0), _pd_id(0), _rm_client(0),
	_virt_utcb((Native_utcb *)utcb),
	_priority(Cpu_session::scale_priority(Kernel::Priority::MAX, priority)),
	_main_thread(0)
{
	strncpy(_name, name, NAME_MAX_LEN);

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
	_phys_utcb = (Native_utcb *)ram->phys_addr(_utcb);

	/* common constructor parts */
	_init();
}


int Platform_thread::join_pd(unsigned const pd_id, bool const main_thread,
                             Weak_ptr<Address_space> address_space)
{
	/* check if we're already in another PD */
	if (_pd_id && _pd_id != pd_id) {
		PERR("already joined a PD");
		return -1;
	}
	/* denote configuration for start method */
	_pd_id         = pd_id;
	_main_thread   = main_thread;
	_address_space = address_space;
	return 0;
}


void Platform_thread::_init()
{
	/* create kernel object */
	_id = Kernel::new_thread(_kernel_thread, this);
	if (!_id) {
		PERR("failed to create kernel object");
		throw Cpu_session::Thread_creation_failed();
	}
}


int Platform_thread::start(void * ip, void * sp, unsigned int cpu_no)
{
	/* must be in a PD to get started */
	if (!_pd_id) {
		PERR("invalid PD");
		return -1;
	}
	/* attach UTCB if the thread can't do this by itself */
	if (!_attaches_utcb_by_itself())
	{
		/*
		 * Declare page aligned virtual UTCB outside the context area.
		 * Kernel afterwards offers this as bootstrap argument to the thread.
		 */
		_virt_utcb = (Native_utcb *)((platform()->vm_start()
		             + platform()->vm_size() - sizeof(Native_utcb))
		             & ~((1<<MIN_MAPPING_SIZE_LOG2)-1));

		/* attach UTCB */
		if (!_rm_client) {
			PERR("invalid RM client");
			return -1;
		};
		Rm_session_component * const rm = _rm_client->member_rm_session();
		try { rm->attach(_utcb, 0, 0, true, _virt_utcb, 0); }
		catch (...) {
			PERR("failed to attach UTCB");
			return -1;
		}
	}
	/* let thread participate in CPU scheduling */
	_tlb = Kernel::start_thread(this, ip, sp, cpu_no);
	if (!_tlb) {
		PERR("failed to start thread");
		return -1;
	}
	return 0;
}


void Platform_thread::pager(Pager_object * const pager)
{
	if (pager) {
		Kernel::set_pager(pager->cap().dst(), _id);
		_rm_client = dynamic_cast<Rm_client *>(pager);
		return;
	}
	Kernel::set_pager(0, _id);
	_rm_client = 0;
	return;
}


Genode::Pager_object * Platform_thread::pager()
{
	return _rm_client ? static_cast<Pager_object *>(_rm_client) : 0;
}
