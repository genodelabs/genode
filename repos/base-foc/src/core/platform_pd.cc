/*
 * \brief   Fiasco.OC protection domain facility
 * \author  Christian Helmuth
 * \author  Stefan Kalkowski
 * \date    2006-04-11
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/native_capability.h>
#include <util/misc_math.h>

/* core includes */
#include <util.h>
#include <platform.h>
#include <platform_pd.h>
#include <map_local.h>

/* Fiasco.OC includes */
#include <foc/syscall.h>

using namespace Foc;
using namespace Genode;


static addr_t core_utcb_base()
{
	static addr_t base = (addr_t) l4_utcb();
	return base;
}


/***************************
 ** Public object members **
 ***************************/

bool Platform_pd::bind_thread(Platform_thread &thread)
{
	/*
	 * Fiasco.OC limits the UTCB area for roottask to 16K. Therefore, the
	 * number of threads is limited to 16K / L4_UTCB_OFFSET.
	 * (see kernel/fiasco/src/kern/kernel_thread-std.cpp:94)
	 */
	unsigned const thread_max = thread.core_thread()
	                          ? 16*1024/L4_UTCB_OFFSET : THREAD_MAX;

	for (unsigned i = 0; i < thread_max; i++) {

		if (_threads[i])
			continue;

		_threads[i] = &thread;

		if (thread.core_thread())
			thread._utcb = (addr_t) (core_utcb_base() + i * L4_UTCB_OFFSET);
		else
			thread._utcb =
				reinterpret_cast<addr_t>(utcb_area_start() + i * L4_UTCB_OFFSET);

		Foc::l4_cap_idx_t cap_offset = THREAD_AREA_BASE + i * THREAD_AREA_SLOT;

		thread._gate.remote  = cap_offset + THREAD_GATE_CAP;
		thread._pager.remote = cap_offset + THREAD_PAGER_CAP;
		thread._irq.remote   = cap_offset + THREAD_IRQ_CAP;

		/* if it's no core-thread we have to map parent and pager gate cap */
		if (!thread.core_thread()) {
			_task.map(_task.local.data()->kcap());

			/*
			 * Set true low level debugging through the kernel debugger interface
			 */
			constexpr bool map_debug_cap = false;
			if (map_debug_cap)
				_debug.map(_task.local.data()->kcap());
		}

		/* inform thread about binding */
		thread.bind(*this);
		return true;
	}

	error("thread alloc failed");
	return false;
}


void Platform_pd::unbind_thread(Platform_thread &thread)
{
	/* inform thread about unbinding */
	thread.unbind();

	for (unsigned i = 0; i < THREAD_MAX; i++)
		if (_threads[i] == &thread) {
			_threads[i] = (Platform_thread*)nullptr;
			return;
		}
}


void Platform_pd::assign_parent(Native_capability parent)
{
	if (_parent.remote == Foc::L4_INVALID_CAP && parent.valid()) {
		_parent.local  = parent;
		_parent.remote = PARENT_CAP;
		_parent.map(_task.local.data()->kcap());
	}
}


void Platform_pd::flush(addr_t, size_t size, Core_local_addr core_local)
{
	unmap_local(core_local.value, size >> get_page_size_log2());
}


static Native_capability debug_cap()
{
	Cap_index::id_t const id = platform_specific().cap_id_alloc().alloc();
	static Cap_index * idx = cap_map().insert(id, DEBUG_CAP);
	static Native_capability debug_cap(reinterpret_cast<Core_cap_index*>(idx));
	return debug_cap;
}


Platform_pd::Platform_pd(Core_cap_index &ci)
:
	_task(Native_capability(&ci), TASK_CAP)
{ }


Platform_pd::Platform_pd(Allocator &, char const *)
:
	_task(true, TASK_CAP), _debug(debug_cap(), DEBUG_CAP)
{
	l4_fpage_t const utcb_area = l4_fpage(utcb_area_start(),
	                                      log2<unsigned>(UTCB_AREA_SIZE), 0);

	l4_msgtag_t const tag = l4_factory_create_task(L4_BASE_FACTORY_CAP,
	                                               _task.local.data()->kcap(),
	                                               utcb_area);
	if (l4_msgtag_has_error(tag))
		error("pd creation failed");
}


Platform_pd::~Platform_pd()
{
	for (unsigned i = 0; i < THREAD_MAX; i++)
		if (_threads[i])
			_threads[i]->unbind();
}
