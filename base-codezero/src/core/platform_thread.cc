/*
 * \brief   Thread facility
 * \author  Norman Feske
 * \date    2009-10-02
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <util/string.h>

/* core includes */
#include <platform_thread.h>

/* Codezero includes */
#include <codezero/syscalls.h>

enum { verbose_thread_start = true };

using namespace Genode;
using namespace Codezero;


void Platform_thread::set_cpu(unsigned int cpu_no)
{
	PDBG("not yet implemented");
}


int Platform_thread::start(void *ip, void *sp, unsigned int cpu_no)
{
	Native_thread_id pager = _pager ? _pager->cap().dst() : -1;

	/* setup thread context */
	struct exregs_data exregs;
	exregs.flags = 0;
	exregs_set_stack(&exregs, (unsigned long)sp);
	exregs_set_pc   (&exregs, (unsigned long)ip);
	exregs_set_pager(&exregs, pager.tid);
	exregs_set_utcb (&exregs, _utcb);

	int ret = l4_exchange_registers(&exregs, _tid);
	if (ret < 0) {
		printf("l4_exchange_registers returned ret=%d\n", ret);
		return -2;
	}

	/* start execution */
	struct task_ids ids = { _tid, _space_id, _tid };
	ret = l4_thread_control(THREAD_RUN, &ids);
	if (ret < 0) {
		printf("Error: l4_thread_control(THREAD_RUN) returned %d\n", ret);
		return -3;
	}

	if (verbose_thread_start)
		printf("core started thread \"%s\" with ID %d inside space ID %d\n",
		       _name, _tid, _space_id);
	return 0;
}


void Platform_thread::pause()
{
	PDBG("not implemented");
}


void Platform_thread::resume()
{
	PDBG("not implemented");
}


int Platform_thread::state(Thread_state *state_dst)
{
	PDBG("not implemented");
	return -1;
}


void Platform_thread::cancel_blocking()
{
	PDBG("not implemented");
}


Platform_thread::Platform_thread(const char *name, unsigned, addr_t,
                                 int thread_id)
: _tid(-1)
{
	strncpy(_name, name, sizeof(_name));
}


Platform_thread::~Platform_thread()
{
	PDBG("not implemented");
}
