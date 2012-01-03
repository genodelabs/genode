/*
 * \brief   Fiasco protection domain facility
 * \author  Christian Helmuth
 * \author  Stefan Kalkowski
 * \date    2006-04-11
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/native_types.h>
#include <util/misc_math.h>

/* core includes */
#include <util.h>
#include <platform.h>
#include <platform_pd.h>

/* Fiasco includes */
namespace Fiasco {
#include <l4/sys/utcb.h>
#include <l4/sys/factory.h>
}

using namespace Fiasco;
using namespace Genode;


static addr_t core_utcb_base() {
	static addr_t base = (addr_t) l4_utcb();
	return base;
}


/****************************
 ** Private object members **
 ****************************/

void Platform_pd::_create_pd(bool syscall)
{
	if (!_l4_task_cap.valid())
		_l4_task_cap = Capability_allocator::allocator()->alloc();

	if (syscall) {
		if (!_l4_task_cap)
			panic("no cap slot for pd creation available!");

		l4_fpage_t utcb_area = l4_fpage(UTCB_AREA_START,
		                                log2<unsigned>(UTCB_AREA_SIZE), 0);
		l4_msgtag_t tag = l4_factory_create_task(L4_BASE_FACTORY_CAP,
		                                         _l4_task_cap, utcb_area);

		if (l4_msgtag_has_error(tag))
			panic("pd creation failed");
	}
}


void Platform_pd::_destroy_pd()
{
	l4_task_unmap(L4_BASE_TASK_CAP,
	              l4_obj_fpage(_l4_task_cap, 0, L4_FPAGE_RWX),
	              L4_FP_ALL_SPACES | L4_FP_DELETE_OBJ);
}


/***************************
 ** Public object members **
 ***************************/

int Platform_pd::bind_thread(Platform_thread *thread)
{
	using namespace Fiasco;

	for (unsigned i = 0; i < THREAD_MAX; i++) {
		if (_threads[i])
			continue;

		_threads[i]                = thread;
		if (thread->core_thread())
			thread->_utcb = (l4_utcb_t*) (core_utcb_base() + i * L4_UTCB_OFFSET);
		else
			thread->_utcb =
				reinterpret_cast<l4_utcb_t*>(UTCB_AREA_START + i * L4_UTCB_OFFSET);
		Native_thread cap_offset   = Fiasco_capability::THREADS_BASE_CAP +
		                             i * Fiasco_capability::THREAD_CAP_SLOT;
		thread->_remote_gate_cap   = Native_capability(cap_offset + Fiasco_capability::THREAD_GATE_CAP,
		                                               thread->_gate_cap.local_name());
		thread->_remote_pager_cap  = cap_offset + Fiasco_capability::THREAD_PAGER_CAP;
		thread->_remote_irq_cap    = cap_offset + Fiasco_capability::THREAD_IRQ_CAP;

		/* inform thread about binding */
		thread->bind(this);
		return 0;
	}

	PERR("thread alloc failed");
	return -1;
}


void Platform_pd::unbind_thread(Platform_thread *thread)
{
	/* inform thread about unbinding */
	thread->unbind();

	for (unsigned i = 0; i < THREAD_MAX; i++)
		if (_threads[i] == thread) {
			_threads[i] = (Platform_thread*) 0;
			return;
		}
}


int Platform_pd::assign_parent(Native_capability parent)
{
	if (_parent.valid()) return -1;
	_parent = parent;
	return 0;
}


void Platform_pd::map_parent_cap()
{
	if (!_parent_cap_mapped) {
		l4_msgtag_t tag = l4_task_map(_l4_task_cap, L4_BASE_TASK_CAP,
		                  l4_obj_fpage(_parent.dst(), 0, L4_FPAGE_RWX),
		                  Fiasco_capability::PARENT_CAP | L4_ITEM_MAP);
		if (l4_msgtag_has_error(tag))
			PWRN("mapping parent cap failed");

	_parent_cap_mapped = true;
	}
}


void Platform_pd::map_task_cap()
{
	if (!_task_cap_mapped) {
		l4_msgtag_t tag = l4_task_map(_l4_task_cap, L4_BASE_TASK_CAP,
		                  l4_obj_fpage(_l4_task_cap, 0, L4_FPAGE_RWX),
		                  Fiasco_capability::TASK_CAP | L4_ITEM_MAP);
		if (l4_msgtag_has_error(tag))
			PWRN("mapping task cap failed");
		_task_cap_mapped = true;
	}
}


Platform_pd::Platform_pd(bool create, Native_task task_cap)
: _l4_task_cap(task_cap),
  _parent_cap_mapped(false),
  _task_cap_mapped(false)
{
	for (unsigned i = 0; i < THREAD_MAX; i++)
		_threads[i] = (Platform_thread*) 0;

	_create_pd(create);
}


Platform_pd::~Platform_pd()
{
	for (unsigned i = 0; i < THREAD_MAX; i++) {
		if (_threads[i])
			_threads[i]->unbind();
	}
	_destroy_pd();
}
