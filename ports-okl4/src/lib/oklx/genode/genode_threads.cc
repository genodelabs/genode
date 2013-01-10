/*
 * \brief  Genode C API thread functions needed by OKLinux
 * \author Stefan Kalkowski
 * \date   2009-05-19
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>

/* OKLinux support library */
#include <oklx_threads.h>

namespace Okl4 {
extern "C" {
#include <iguana/thread.h>
#include <genode/sleep.h>
}
}

using namespace Genode;
using namespace Okl4;

static Thread_capability oklx_pager_cap; /* cap to the Linux main thread */


/**
 * Get the thread capability of the active kernel thread
 */
static Thread_capability my_cap()
{
	Okl4::L4_Word_t tid = thread_myself();                                      

	Oklx_kernel_thread * thread = Genode::Oklx_thread_list::thread_list()->first();
	while (thread) {
		if (thread->tid().raw == tid)
			return thread->cap();
		thread = thread->next();
	}

	/* invalid cap */
	return Thread_capability(); 
}


void Oklx_kernel_thread::entry()
{
	L4_ThreadId_t tid;

	/* Save our thread id to the first entry in the UTCB */
	__L4_TCR_Set_ThreadWord(0, L4_UserDefinedHandle());

	/* Synchronize with the thread, that created us and sleep afterwards */
	L4_Wait(&tid);
	sleep_forever();
}


L4_ThreadId_t Oklx_thread_list::add()
{
	try
	{
		/* Create the thread an start it immediately */
		Thread_capability cap = _cpu.create_thread("Lx_kernel_thread");
		Oklx_kernel_thread *thd = new (env()->heap()) Oklx_kernel_thread(cap);
		_threads.insert(thd);
		env()->pd_session()->bind_thread(cap);
		Pager_capability pager = env()->rm_session()->add_client(cap);
		_cpu.set_pager(cap, pager);
		_cpu.start(cap, (addr_t)&Oklx_kernel_thread::entry,
		           (addr_t)thd->stack_addr());

		/* Get the OKL4 thread id of the new thread */
		Thread_state state = _cpu.state(cap);
		thd->set_tid(state.tid);

		/* Acknowledge startup and return */
		L4_Send(state.tid);
		return state.tid;
	}
	catch(...)
	{
		PWRN("Creation of new thread failed!");
		return L4_nilthread;
	}
}


Oklx_thread_list* Oklx_thread_list::thread_list()
{
	static Oklx_thread_list _list;
	return &_list;
}


Oklx_process::~Oklx_process()
{
	/* When the process dies, kill all of its threads */
	while(_threads.first())
	{
		Oklx_user_thread *th = _threads.first();
		_threads.remove(th);
		destroy(env()->heap(), th);
	}
}


L4_ThreadId_t
Oklx_process::add_thread()
{
	Oklx_user_thread *th = 0;
	try {
		Thread_state dst_state;
		th = new (env()->heap()) Oklx_user_thread();
		_pd.bind_thread(th->cap());

		/*
		 * Initialize eip and esp with max. value to signal
		 * core, that it doesn't really need to start this thread,
		 * but will create the OKL4 thread inactive
		 */
		_cpu.start(th->cap(), 0xffffffff, 0xffffffff);
		dst_state = _cpu.state(th->cap());
		th->_tid = dst_state.tid;
		_threads.insert(th);
		return th->_tid;
	}
	catch(...)
	{
		PWRN("Couldn't create a new Thread for space %lx", _pd.space_id().raw);
		if(th)
			destroy(env()->heap(), th);
		return L4_nilthread;
	}
}


bool
Oklx_process::kill_thread(Okl4::L4_ThreadId_t tid)
{
	Oklx_user_thread *th = _threads.first();
	while (th)
	{
		if(th->tid().raw == tid.raw)
		{
			_threads.remove(th);
			destroy(env()->heap(), th);
			return true;
		}
		th = th->next();
	}
	return false;
}


List<Oklx_process>*
Oklx_process::processes()
{
	static List<Oklx_process> _list;
	return &_list;
}


void Oklx_process::set_pager()
{
	oklx_pager_cap = my_cap();
}


Thread_capability Oklx_process::pager_cap()
{
	return oklx_pager_cap;
}
