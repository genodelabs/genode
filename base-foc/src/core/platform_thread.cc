/*
 * \brief  Fiasco thread facility
 * \author Stefan Kalkowski
 * \date   2011-01-04
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/ipc.h>
#include <base/printf.h>
#include <util/string.h>

/* core includes */
#include <cap_session_component.h>
#include <platform_thread.h>
#include <platform.h>
#include <core_env.h>

/* Fiasco includes */
namespace Fiasco {
#include <l4/sys/debugger.h>
#include <l4/sys/factory.h>
#include <l4/sys/irq.h>
#include <l4/sys/scheduler.h>
#include <l4/sys/thread.h>
#include <l4/sys/types.h>
}

using namespace Genode;
using namespace Fiasco;


int Platform_thread::start(void *ip, void *sp)
{
	if (_pager && _platform_pd) {
		/* map pager cap */
		l4_msgtag_t tag = l4_task_map(_platform_pd->native_task(), L4_BASE_TASK_CAP,
		                  l4_obj_fpage(_pager->cap().tid(), 0, L4_FPAGE_RWX),
		                  _remote_pager_cap | L4_ITEM_MAP);
		if (l4_msgtag_has_error(tag))
			PWRN("mapping pager cap failed");
	}

	/* reserve utcb area and associate thread with this task */
	l4_thread_control_start();
	l4_thread_control_pager(_remote_pager_cap);
	l4_thread_control_exc_handler(_remote_pager_cap);
	l4_thread_control_bind(_utcb, _platform_pd->native_task());

	l4_msgtag_t tag = l4_thread_control_commit(_thread_cap.tid());
	if (l4_msgtag_has_error(tag)) {
		PWRN("l4_thread_control_commit for %lx failed!",
		     (unsigned long) _thread_cap.tid());
		return -1;
	}

	/* set ip and sp and run the thread */
	tag = l4_thread_ex_regs(_thread_cap.tid(), (l4_addr_t) ip, (l4_addr_t) sp, 0);
	if (l4_msgtag_has_error(tag)) {
		PWRN("l4_thread_ex_regs failed!");
		return -1;
	}

	return 0;
}


void Platform_thread::pause()
{
	if (!_pager)
		return;

	_pager->state.lock.lock();

	if (_pager->state.paused == true) {
		_pager->state.lock.unlock();
		return;
	}

	unsigned exc      = _pager->state.exceptions;
	_pager->state.ip  = ~0UL;
	_pager->state.sp  = ~0UL;
	l4_umword_t flags = L4_THREAD_EX_REGS_TRIGGER_EXCEPTION;

	/* Mark thread to be stopped */
	_pager->state.paused = true;

	/*
	 * Force the thread to be paused to trigger an exception.
	 * The pager thread, which also acts as exception handler, will
	 * leave the thread in exception state until, it gets woken again
	 */
	l4_thread_ex_regs_ret(_thread_cap.tid(), &_pager->state.ip,
	                      &_pager->state.sp, &flags);
	bool in_syscall  = flags == 0;
	_pager->state.lock.unlock();

	/**
	 * Check whether the thread was in ongoing ipc, if so it won't raise
	 * an exception before ipc is completed.
	 */
	if (!in_syscall) {
		/*
		 * Wait until the pager thread got an exception from
		 * the requested thread, and stored its thread state
		 */
		while (exc == _pager->state.exceptions && !_pager->state.in_exception)
			l4_thread_switch(_thread_cap.tid());
	}
}


void Platform_thread::resume()
{
	if (!_pager)
		return;

	_pager->state.lock.lock();

	/* Mark thread to be runable again */
	_pager->state.paused = false;
	_pager->state.lock.unlock();

	/* Send a message to the exception handler, to unblock the client */
	Msgbuf<16> snd, rcv;
	Ipc_client ipc_client(_pager->cap(), &snd, &rcv);
	ipc_client << _pager << IPC_CALL;
}


void Platform_thread::bind(Platform_pd *pd)
{
	l4_msgtag_t   tag;
	Native_task   task      = pd->native_task();

	_platform_pd = pd;

	if (!_core_thread) {
		/* map parent and task cap if it doesn't happen already */
		_platform_pd->map_task_cap();
		_platform_pd->map_parent_cap();
	}

	if (_gate_cap.valid()) {
		/* map thread's gate cap */
		tag = l4_task_map(task, L4_BASE_TASK_CAP,
		                  l4_obj_fpage(_gate_cap.tid(), 0, L4_FPAGE_RWX),
		                  _remote_gate_cap.tid() | L4_ITEM_MAP);
		if (l4_msgtag_has_error(tag))
			PWRN("mapping thread's gate cap failed");
	}

	/* map thread's irq cap */
	tag = l4_task_map(task, L4_BASE_TASK_CAP,
	                  l4_obj_fpage(_irq_cap, 0, L4_FPAGE_RWX),
	                  _remote_irq_cap | L4_ITEM_MAP);
	if (l4_msgtag_has_error(tag))
		PWRN("mapping thread's irq cap failed");
}


void Platform_thread::unbind()
{
	l4_thread_ex_regs(_thread_cap.tid(), 0, 0, 0);
	l4_task_unmap(L4_BASE_TASK_CAP,
	              l4_obj_fpage(_gate_cap.tid(), 0, L4_FPAGE_RWX),
	              L4_FP_ALL_SPACES | L4_FP_DELETE_OBJ);
	l4_task_unmap(L4_BASE_TASK_CAP,
	              l4_obj_fpage(_thread_cap.tid(), 0, L4_FPAGE_RWX),
	              L4_FP_ALL_SPACES | L4_FP_DELETE_OBJ);
	_platform_pd = (Platform_pd*) 0;
}


void Platform_thread::pager(Pager_object *pager)
{
	_pager = pager;
}


int Platform_thread::state(Thread_state *state_dst)
{
	if (_pager)
		*state_dst = _pager->state;

	state_dst->cap = _remote_gate_cap;
	return 0;
}


void Platform_thread::cancel_blocking()
{
	l4_irq_trigger(_irq_cap);
}


void Platform_thread::_create_thread()
{
	l4_msgtag_t tag = l4_factory_create_thread(L4_BASE_FACTORY_CAP,
	                                           _thread_cap.tid());
	if (l4_msgtag_has_error(tag))
		PERR("cannot create more thread kernel-objects!");
}


void Platform_thread::_finalize_construction(const char *name, unsigned prio)
{
	/* create irq for new thread */
	_irq_cap = cap_alloc()->alloc();
	l4_msgtag_t tag = l4_factory_create_irq(L4_BASE_FACTORY_CAP, _irq_cap);
	if (l4_msgtag_has_error(tag))
		PWRN("creating thread's irq failed");

	/* attach thread to irq */
	tag = l4_irq_attach(_irq_cap, 0, _thread_cap.tid());
	if (l4_msgtag_has_error(tag))
		PWRN("attaching thread's irq failed");

	/* set human readable name in kernel debugger */
	strncpy(_name, name, sizeof(_name));
	Fiasco::l4_debugger_set_object_name(_thread_cap.tid(), name);

	/* set priority of thread */
	prio = Cpu_session::scale_priority(DEFAULT_PRIORITY, prio);
	l4_sched_param_t params = l4_sched_param(prio);
	l4_scheduler_run_thread(L4_BASE_SCHEDULER_CAP, _thread_cap.tid(), &params);
}


Platform_thread::Platform_thread(const char *name,
                                 unsigned    prio)
: _core_thread(false),
  _badge(Badge_allocator::allocator()->alloc()),
  _thread_cap(cap_alloc()->alloc_id(_badge),
              _badge),
  _node(_thread_cap.local_name(), 0, this, _thread_cap.tid()),
  _utcb(0),
  _platform_pd(0),
  _pager(0)
{
	/* register the thread capability */
	Capability_tree::tree()->insert(&_node);

	_create_thread();

	/* create gate for new thread */
	_gate_cap = core_env()->cap_session()->alloc(_thread_cap);

	_finalize_construction(name, prio);
}


Platform_thread::Platform_thread(Native_thread cap, const char *name)
: _core_thread(true),
  _thread_cap(cap, -1),
  _node(_thread_cap.local_name(), 0, this, _thread_cap.tid()),
  _utcb(0),
  _platform_pd(0),
  _pager(0)
{
	/* register the thread capability */
	Capability_tree::tree()->insert(&_node);

	_finalize_construction(name, 0);
}


Platform_thread::Platform_thread(const char *name)
: _core_thread(true),
  _badge(Badge_allocator::allocator()->alloc()),
  _thread_cap(cap_alloc()->alloc_id(_badge),
              _badge),
  _node(_thread_cap.local_name(), 0, this, _thread_cap.tid()),
  _utcb(0),
  _platform_pd(0),
  _pager(0)
{
	/* register the thread capability */
	Capability_tree::tree()->insert(&_node);

	_create_thread();

	/* create gate for new thread */
	_gate_cap = Cap_session_component::alloc(0, _thread_cap);

	_finalize_construction(name, 0);
}


Platform_thread::~Platform_thread()
{
	/*
	 * We inform our protection domain about thread destruction, which will end up in
	 * Thread::unbind()
	 */
	if (_platform_pd)
		_platform_pd->unbind_thread(this);

	/* remove the thread capability */
	Capability_tree::tree()->remove(&_node);
	cap_alloc()->free(_thread_cap.tid());
	Badge_allocator::allocator()->free(_badge);
}
