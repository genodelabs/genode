/*
 * \brief  Implementation of Thread API interface for core
 * \author Norman Feske
 * \date   2006-05-03
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Codezero includes */
#include <codezero/syscalls.h>

/* Genode includes */
#include <base/thread.h>
#include <base/printf.h>
#include <base/sleep.h>

/* core includes */
#include <platform.h>
#include <platform_thread.h>

enum { verbose_thread_start = true };

using namespace Genode;

void Thread_base::_deinit_platform_thread() { }


/**
 * Create and start new thread
 *
 * \param space_no   space ID in which the new thread will be executed
 * \param sp         initial stack pointer
 * \param ip         initial instruction pointer
 * \return           new thread ID, or
 *                   negative error code
 */
inline int create_thread(unsigned space_no,
                         void *sp, void *ip,
                         int pager_tid = 1)
{
	using namespace Codezero;

	struct task_ids ids = { 1U, space_no, TASK_ID_INVALID };

	/* allocate new thread at the kernel */
	unsigned long flags = THREAD_CREATE | TC_SHARE_SPACE | TC_SHARE_GROUP;
	int ret = l4_thread_control(flags, &ids);
	if (ret < 0) {
		PERR("l4_thread_control returned %d, spid=%d\n",
		     ret, ids.spid);
		return -1;
	}

	unsigned long utcb_base_addr = (unsigned long)l4_get_utcb();

	/* calculate utcb address of new thread */
	unsigned long new_utcb = utcb_base_addr + ids.tid*sizeof(struct utcb);

	/* setup thread context */
	struct exregs_data exregs;
	memset(&exregs, 0, sizeof(exregs));
	exregs_set_stack(&exregs, (unsigned long)sp);
	exregs_set_pc   (&exregs, (unsigned long)ip);
	exregs_set_pager(&exregs, pager_tid);
	exregs_set_utcb (&exregs, new_utcb);

	ret = l4_exchange_registers(&exregs, ids.tid);
	if (ret < 0) {
		printf("l4_exchange_registers returned ret=%d\n", ret);
		return -2;
	}

	/* start execution */
	ret = l4_thread_control(THREAD_RUN, &ids);
	if (ret < 0) {
		printf("Error: l4_thread_control(THREAD_RUN) returned %d\n", ret);
		return -3;
	}

	/* return new thread ID allocated by the kernel */
	return ids.tid;
}


void Thread_base::_thread_start()
{
	Thread_base::myself()->_thread_bootstrap();
	Thread_base::myself()->entry();
	sleep_forever();
}


void Thread_base::start()
{
	/* create and start platform thread */
	_tid.pt = new(platform()->core_mem_alloc()) Platform_thread(_context->name);

	_tid.l4id = create_thread(1, stack_top(), (void *)&_thread_start);

	if (_tid.l4id < 0)
		PERR("create_thread returned %d", _tid.l4id);

	if (verbose_thread_start)
		printf("core started local thread \"%s\" with ID %d\n",
		       _context->name, _tid.l4id);

}


void Thread_base::cancel_blocking()
{
	PWRN("not implemented");
}


