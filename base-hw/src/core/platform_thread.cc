/*
 * \brief   Thread facility
 * \author  Martin Stein
 * \date    2012-02-12
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
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
	return _pd_id == Kernel::core_id() || !_main_thread;
}


Platform_thread::~Platform_thread()
{
	/* detach UTCB if main thread outside core */
	if (!_attaches_utcb_by_itself()) {
		assert(_rm_client);
		Rm_session_component * const rm = _rm_client->member_rm_session();
		rm->detach(_virt_utcb);
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
	/* destroy object at the kernel */
	Kernel::delete_thread(_id);

	/* free kernel object space */
	Range_allocator * ram = platform()->ram_alloc();
	ram->free(&_kernel_thread, Kernel::thread_size());
}


Platform_thread::Platform_thread(const char * name,
                                 Thread_base * const thread_base,
                                 unsigned long const stack_size,
                                 unsigned long const pd_id)
:
	_thread_base(thread_base), _stack_size(stack_size),
	_pd_id(pd_id), _rm_client(0), _virt_utcb(0)
{
	strncpy(_name, name, NAME_MAX_LEN);

	/* create UTCB for a core thread */
	Range_allocator * const ram = platform()->ram_alloc();
	assert(ram->alloc_aligned(sizeof(Native_utcb), (void **)&_phys_utcb,
	                          MIN_MAPPING_SIZE_LOG2));
	_virt_utcb = _phys_utcb;

	/* common constructor parts */
	_init();
}


Platform_thread::Platform_thread(const char * name, unsigned int priority,
                                 addr_t utcb)
:
	_thread_base(0), _stack_size(0), _pd_id(0), _rm_client(0),
	_virt_utcb((Native_utcb *)utcb)
{
	strncpy(_name, name, NAME_MAX_LEN);

	/*
	 * Allocate UTCB backing store for a thread outside of core. Page alignment
	 * is done by RAM session by default. It's save to use core env because
	 * this cannot be its server activation thread.
	 */
	try {
		Ram_session_component * const ram =
			dynamic_cast<Ram_session_component *>(core_env()->ram_session());
		assert(ram);
		_utcb = ram->alloc(sizeof(Native_utcb), 1);
		_phys_utcb = (Native_utcb *)ram->phys_addr(_utcb);
	}
	catch (...) { assert(0); }

	/* common constructor parts */
	_init();
}


int Platform_thread::join_pd(unsigned long const pd_id,
                             bool const main_thread)
{
	/* check if we're already in another PD */
	if (_pd_id && _pd_id != pd_id) return -1;

	/* denote configuration for start method */
	_pd_id = pd_id;
	_main_thread = main_thread;
	return 0;
}


void Platform_thread::_init()
{
	/* create kernel object */
	Range_allocator * ram = platform()->ram_alloc();
	assert(ram->alloc(Kernel::thread_size(), &_kernel_thread));
	_id = Kernel::new_thread(_kernel_thread, this);
	assert(_id);
}


int Platform_thread::start(void * ip, void * sp, unsigned int cpu_no)
{
	/* check thread attributes */
	assert(_pd_id);

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
		assert(_rm_client);
		Rm_session_component * const rm = _rm_client->member_rm_session();
		try { rm->attach(_utcb, 0, 0, true, _virt_utcb, 0); }
		catch (...) { assert(0); }
	}
	/* let thread participate in CPU scheduling */
	_software_tlb = Kernel::start_thread(this, ip, sp, cpu_no);
	return _software_tlb ? 0 : -1;
}


void Platform_thread::pager(Pager_object * const pager)
{
	/* announce pager thread to kernel */
	Kernel::set_pager(pager->cap().dst(), _id);

	/* get RM client from pager pointer */
	_rm_client = dynamic_cast<Rm_client *>(pager);
	assert(_rm_client);
}


Genode::Pager_object * Platform_thread::pager() const
{
	assert(_rm_client)
	return static_cast<Pager_object *>(_rm_client);
}

