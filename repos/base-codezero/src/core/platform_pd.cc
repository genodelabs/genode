/*
 * \brief   Protection-domain facility
 * \author  Norman Feske
 * \date    2009-10-02
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>

/* core includes */
#include <platform_pd.h>
#include <platform.h>
#include <util.h>

using namespace Genode;
using namespace Codezero;


/***************************
 ** Public object members **
 ***************************/

int Platform_pd::bind_thread(Platform_thread *thread)
{
	/* allocate new thread at the kernel */
	struct task_ids ids = { 1, _space_id, TASK_ID_INVALID };
	int ret = l4_thread_control(THREAD_CREATE | TC_SHARE_SPACE, &ids);
	if (ret < 0) {
		PERR("l4_thread_control returned %d, tid=%d\n", ret, ids.tid);
		return -1;
	}

	/* allocate UTCB for new thread */
	int utcb_idx;
	for (utcb_idx = 0; utcb_idx < MAX_THREADS_PER_PD; utcb_idx++)
		if (!utcb_in_use[utcb_idx]) break;

	if (utcb_idx == MAX_THREADS_PER_PD) {
		PERR("UTCB allocation failed");
		return -2;
	}

	/* mark UTCB as being in use */
	utcb_in_use[utcb_idx] = true;

	/* map UTCB area for the first thread of a new PD */
	if (utcb_idx == 0) {
		void *utcb_phys = 0;
		if (!platform()->ram_alloc()->alloc(UTCB_AREA_SIZE, &utcb_phys)) {
			PERR("could not allocate physical pages for UTCB");
			return -3;
		}

		ret = l4_map(utcb_phys, (void *)UTCB_VIRT_BASE,
		             UTCB_AREA_SIZE/get_page_size(), MAP_USR_RW, ids.tid);
		if (ret < 0) {
			PERR("UTCB mapping into new PD failed, ret=%d", ret);
			return -4;
		}
	}

	addr_t utcb_addr = UTCB_VIRT_BASE + utcb_idx*sizeof(struct utcb);
	thread->_assign_physical_thread(ids.tid, _space_id, utcb_addr,
	                                this->Address_space::weak_ptr());
	return 0;
}


void Platform_pd::unbind_thread(Platform_thread *thread)
{
	/* find UTCB index of thread */
	unsigned utcb_idx;
	for (utcb_idx = 0; utcb_idx < MAX_THREADS_PER_PD; utcb_idx++)
		if (thread->utcb() == UTCB_VIRT_BASE + utcb_idx*sizeof(struct utcb))
			break;

	if (utcb_idx == MAX_THREADS_PER_PD) {
		PWRN("could not find UTCB index of thread");
		return;
	}

	utcb_in_use[utcb_idx] = false;

	PWRN("not fully implemented");
}


Platform_pd::Platform_pd(bool core)
{
	PWRN("not yet implemented");
}


Platform_pd::Platform_pd(Allocator * md_alloc, char const *,
                         signed pd_id, bool create)
: _space_id(TASK_ID_INVALID)
{
	_space_id = TASK_ID_INVALID;

	/* mark all UTCBs of the new PD as free */
	for (int i = 0; i < MAX_THREADS_PER_PD; i++)
		utcb_in_use[i] = false;

	struct task_ids ids = { TASK_ID_INVALID, TASK_ID_INVALID, TASK_ID_INVALID };

	int ret = l4_thread_control(THREAD_CREATE | TC_NEW_SPACE, &ids);
	if (ret < 0) {
		PERR("l4_thread_control(THREAD_CREATE | TC_NEW_SPACE) returned %d", ret);
		return;
	}

	/* set space ID to valid value to indicate success */
	_space_id = ids.spid;
}


Platform_pd::~Platform_pd()
{
	/* invalidate weak pointers to this object */
	Address_space::lock_for_destruction();

	PWRN("not yet implemented");
}
