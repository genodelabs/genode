/*
 * \brief   Fiasco protection domain facility
 * \author  Christian Helmuth
 * \date    2006-04-11
 *
 * On Fiasco, the pd class has several duties:
 *
 * - It is an allocator for L4 tasks and cares for versioning and recycling. We
 *   do this with "static class members".
 * - L4 threads are tied to L4 tasks and there are only 128 per L4 task. So
 *   each pd object is an allocator for its threads.
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/native_capability.h>

/* core includes */
#include <util.h>
#include <platform_pd.h>

/* L4/Fiasco includes */
#include <fiasco/syscall.h>

using namespace Fiasco;
using namespace Core;


static bool _init = false;


void Platform_pd::init()
{
	if (_init)
		return;

	unsigned i;
	Pd_alloc reserved(true, true, 0);
	Pd_alloc free(false, true, 0);

	/* mark reserved protection domains */
	for (i = 0; i < PD_FIRST; ++i) _pds()[i] = reserved;
	/* init remainder */
	for ( ; i < PD_MAX; ++i) _pds()[i] = free;

	_init = true;
}


void Platform_pd::_create_pd(bool syscall)
{
	enum { TASK_ID_MASK     = (1 << 11) - 1,
	       VERSION_LOW_MASK = (1 << 10) - 1 };

	l4_threadid_t l4t  = l4_myself();
	l4t.id.task        = _pd_id & TASK_ID_MASK;
	l4t.id.lthread     = 0;
	l4t.id.version_low = _version & VERSION_LOW_MASK;

	l4_taskid_t nt;
	if (syscall)
		nt = l4_task_new(l4t, 0, 0, 0, l4t);
	else
		nt = l4t;

	if (l4_is_nil_id(nt))
		panic("pd creation failed");

	_l4_task_id = nt;
}


void Platform_pd::_destroy_pd()
{
	l4_threadid_t l4t = _l4_task_id;

	/* L4 task deletion is: make inactive with myself as chief in 2nd parameter */
	l4_taskid_t nt = l4_task_new(l4t, convert_native_thread_id_to_badge(l4_myself()),
	                             0, 0, L4_NIL_ID);

	if (l4_is_nil_id(nt))
		panic("pd destruction failed");

	_l4_task_id = L4_INVALID_ID;
}


int Platform_pd::_alloc_pd(signed pd_id)
{
	if (pd_id == PD_INVALID) {
		unsigned i;

		for (i = PD_FIRST; i < PD_MAX; i++)
			if (_pds()[i].free) break;

		/* no free protection domains available */
		if (i == PD_MAX) return -1;

		pd_id = i;
	} else {
		if (!_pds()[pd_id].reserved || !_pds()[pd_id].free)
			return -1;
	}

	_pds()[pd_id].free = 0;

	_pd_id   = pd_id;
	_version = _pds()[pd_id].version;

	return pd_id;
}


void Platform_pd::_free_pd()
{
	unsigned t = _pd_id;

	/* XXX check and log double-free? */
	if (_pds()[t].free) return;

	/* maximum reuse count reached leave non-free */
	if (_pds()[t].version == PD_VERSION_MAX) return;

	_pds()[t].free = 1;
	++_pds()[t].version;
}


Platform_pd::Alloc_thread_id_result Platform_pd::alloc_thread_id(Platform_thread &thread)
{
	for (unsigned i = 0; i < THREAD_MAX; i++) {
		if (_threads[i] == nullptr) {
			_threads[i] = &thread;
			return Thread_id { i };
		}
	}
	return Alloc_thread_id_error::EXHAUSTED;
}


void Platform_pd::free_thread_id(Thread_id const id)
{
	if (id.value >= THREAD_MAX)
		return;

	if (!_threads[id.value])
		warning("double-free of thread ", Hex(_pd_id), ".", Hex(id.value), " detected");

	_threads[id.value] = nullptr;
}


void Platform_pd::flush(addr_t, size_t size, Core_local_addr core_local_base)
{
	/*
	 * Fiasco's 'unmap' syscall unmaps the specified flexpage from all address
	 * spaces to which we mapped the pages. We cannot target this operation to
	 * a specific L4 task. Hence, we unmap the dataspace from all tasks.
	 */

	using namespace Fiasco;

	addr_t addr = core_local_base.value;
	for (; addr < core_local_base.value + size; addr += L4_PAGESIZE)
		l4_fpage_unmap(l4_fpage(addr, L4_LOG2_PAGESIZE, 0, 0),
		               L4_FP_FLUSH_PAGE);
}


Platform_pd::Platform_pd(Allocator &, char const *)
{
	/* check correct init */
	if (!_init)
		panic("init pd facility via Platform_pd::init() before using it!");

	int ret = _alloc_pd(PD_INVALID);
	if (ret < 0) {
		panic("pd alloc failed");
	}

	_create_pd(true);
}


Platform_pd::Platform_pd(char const *, signed pd_id)
{
	/* check correct init */
	if (!_init)
		panic("init pd facility via Platform_pd::init() before using it!");

	int ret = _alloc_pd(pd_id);
	if (ret < 0) {
		panic("pd alloc failed");
	}

	_create_pd(false);
}


Platform_pd::~Platform_pd()
{
	bool any_thread_exists = false;
	for (Platform_thread *t : _threads) any_thread_exists |= (t != nullptr);
	if (any_thread_exists)
		error("attempt to destruct platform PD before threads");

	_destroy_pd();
	_free_pd();
}
