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


/***************************
 ** Public object members **
 ***************************/

int Platform_pd::bind_thread(Platform_thread *thread)
{
	for (unsigned i = 0; i < THREAD_MAX; i++) {
		if (_threads[i])
			continue;

		_threads[i]                = thread;
		if (thread->core_thread())
			thread->_utcb = (l4_utcb_t*) (core_utcb_base() + i * L4_UTCB_OFFSET);
		else
			thread->_utcb =
				reinterpret_cast<l4_utcb_t*>(utcb_area_start() + i * L4_UTCB_OFFSET);
		Native_thread cap_offset   = THREADS_BASE_CAP + i * THREAD_CAP_SLOT;
		thread->_gate.remote   = cap_offset + THREAD_GATE_CAP;
		thread->_pager.remote  = cap_offset + THREAD_PAGER_CAP;
		thread->_irq.remote    = cap_offset + THREAD_IRQ_CAP;

		/* if it's no core-thread we have to map parent and pager gate cap */
		if (!thread->core_thread()) {
			_task.map(_task.local.dst());
			_parent.map(_task.local.dst());
		}

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
	if (!parent.valid()) return -1;
	_parent.local  = parent;
	_parent.remote = PARENT_CAP;
	return 0;
}


Platform_pd::Platform_pd(Core_cap_index* i)
: _task(Native_capability(i), TASK_CAP)
{
	for (unsigned i = 0; i < THREAD_MAX; i++)
		_threads[i] = (Platform_thread*) 0;
}


Platform_pd::Platform_pd()
: _task(true, TASK_CAP)
{
	for (unsigned i = 0; i < THREAD_MAX; i++)
		_threads[i] = (Platform_thread*) 0;

	l4_fpage_t utcb_area = l4_fpage(utcb_area_start(),
	                                log2<unsigned>(UTCB_AREA_SIZE), 0);
	l4_msgtag_t tag = l4_factory_create_task(L4_BASE_FACTORY_CAP,
	                                         _task.local.dst(), utcb_area);
	if (l4_msgtag_has_error(tag))
		PERR("pd creation failed");
}


Platform_pd::~Platform_pd()
{
	for (unsigned i = 0; i < THREAD_MAX; i++) {
		if (_threads[i])
			_threads[i]->unbind();
	}
}
