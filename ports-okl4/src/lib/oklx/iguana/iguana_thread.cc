/*
 * \brief  Iguana API functions needed by OKLinux
 * \author Stefan Kalkowski
 * \date   2009-05-07
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <oklx_threads.h>

using namespace Okl4;

extern "C" {

	thread_ref_t thread_create(L4_ThreadId_t *thrd)
	{
		thrd->raw = Genode::Oklx_thread_list::thread_list()->add().raw;
		return thrd->raw;
	}


	L4_ThreadId_t thread_l4tid(thread_ref_t server)
	{
		L4_ThreadId_t tid;
		tid.raw = server;
		return tid;
	}


	thread_ref_t thread_myself(void)
	{
		return __L4_TCR_ThreadWord(Genode::UTCB_TCR_THREAD_WORD_MYSELF);
	}


	void thread_delete(L4_ThreadId_t thrd)
	{
		using namespace Genode;
		Oklx_process *p = Oklx_process::processes()->first();
		while(p)
		{
			if(p->kill_thread(thrd))
				return;
			p = p->next();
		}
	}

} // extern "C"
