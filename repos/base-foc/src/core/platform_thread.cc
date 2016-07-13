/*
 * \brief  Fiasco thread facility
 * \author Stefan Kalkowski
 * \date   2011-01-04
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/ipc.h>
#include <base/log.h>
#include <util/string.h>

/* core includes */
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
	/* map the pager cap */
	if (_platform_pd)
		_pager.map(_platform_pd->native_task().data()->kcap());

	/* reserve utcb area and associate thread with this task */
	l4_thread_control_start();
	l4_thread_control_pager(_pager.remote);
	l4_thread_control_exc_handler(_pager.remote);
	l4_thread_control_bind((l4_utcb_t *)_utcb, _platform_pd->native_task().data()->kcap());
	l4_msgtag_t tag = l4_thread_control_commit(_thread.local.data()->kcap());
	if (l4_msgtag_has_error(tag)) {
		PWRN("l4_thread_control_commit for %lx failed!",
		     (unsigned long) _thread.local.data()->kcap());
		return -1;
	}

	_state = RUNNING;

	/* set ip and sp and run the thread */
	tag = l4_thread_ex_regs(_thread.local.data()->kcap(), (l4_addr_t) ip,
	                        (l4_addr_t) sp, 0);
	if (l4_msgtag_has_error(tag)) {
		PWRN("l4_thread_ex_regs failed!");
		return -1;
	}

	return 0;
}


void Platform_thread::pause()
{
	if (!_pager_obj)
		return;

	_pager_obj->state.lock.lock();

	if (_pager_obj->state.paused == true) {
		_pager_obj->state.lock.unlock();
		return;
	}

	unsigned exc      = _pager_obj->state.exceptions;
	_pager_obj->state.ip  = ~0UL;
	_pager_obj->state.sp  = ~0UL;
	l4_umword_t flags = L4_THREAD_EX_REGS_TRIGGER_EXCEPTION;

	/* Mark thread to be stopped */
	_pager_obj->state.paused = true;

	/*
	 * Force the thread to be paused to trigger an exception.
	 * The pager thread, which also acts as exception handler, will
	 * leave the thread in exception state until, it gets woken again
	 */
	l4_thread_ex_regs_ret(_thread.local.data()->kcap(), &_pager_obj->state.ip,
	                      &_pager_obj->state.sp, &flags);

	/*
	 * The thread state ("ready") is encoded in the lowest bit of the flags.
	 */
	bool in_syscall = (flags & 1) == 0;
	_pager_obj->state.lock.unlock();

	/**
	 * Check whether the thread was in ongoing ipc, if so it won't raise
	 * an exception before ipc is completed.
	 */
	if (!in_syscall) {
		/*
		 * Wait until the pager thread got an exception from
		 * the requested thread, and stored its thread state
		 */
		while (exc == _pager_obj->state.exceptions && !_pager_obj->state.in_exception)
			l4_thread_switch(_thread.local.data()->kcap());
	}
}


void Platform_thread::single_step(bool enabled)
{
	Fiasco::l4_cap_idx_t const tid = thread().local.data()->kcap();

	enum { THREAD_SINGLE_STEP = 0x40000 };
	int const flags = enabled ? THREAD_SINGLE_STEP : 0;

	Fiasco::l4_thread_ex_regs(tid, ~0UL, ~0UL, flags);
}


void Platform_thread::resume()
{
	if (!_pager_obj)
		return;

	_pager_obj->state.lock.lock();

	/* Mark thread to be runable again */
	_pager_obj->state.paused = false;
	_pager_obj->state.lock.unlock();

	/* Send a message to the exception handler, to unblock the client */
	Msgbuf<16> snd, rcv;
	snd.insert(_pager_obj);
	ipc_call(_pager_obj->cap(), snd, rcv, 0);
}


void Platform_thread::bind(Platform_pd *pd)
{
	_platform_pd = pd;
	_gate.map(pd->native_task().data()->kcap());
	_irq.map(pd->native_task().data()->kcap());
}


void Platform_thread::unbind()
{
	if (_state == RUNNING) {
		/* first set the thread as its own pager */
		l4_thread_control_start();
		l4_thread_control_pager(_gate.remote);
		l4_thread_control_exc_handler(_gate.remote);
		if (l4_msgtag_has_error(l4_thread_control_commit(_thread.local.data()->kcap())))
			PWRN("l4_thread_control_commit for %lx failed!",
				 (unsigned long) _thread.local.data()->kcap());

		/* now force it into a pagefault */
		l4_thread_ex_regs(_thread.local.data()->kcap(), 0, 0, L4_THREAD_EX_REGS_CANCEL);
	}

	_platform_pd = (Platform_pd*) 0;
}


void Platform_thread::pager(Pager_object *pager_obj)
{
	_pager_obj   = pager_obj;
	if (_pager_obj)
		_pager.local = pager_obj->cap();
	else
		_pager.local = Native_capability();
}


void Platform_thread::state(Thread_state s)
{
	if (_pager_obj)
		_pager_obj->state = s;
}


Thread_state Platform_thread::state()
{
	Thread_state s;
	if (_pager_obj) s = _pager_obj->state;

	s.kcap = _gate.remote;
	s.id   = _gate.local.local_name();
	s.utcb = _utcb;

	return s;
}


void Platform_thread::cancel_blocking()
{
	l4_irq_trigger(_irq.local.data()->kcap());
}


void Platform_thread::affinity(Affinity::Location location)
{
	_location = location;

	int const cpu = location.xpos();

	l4_sched_param_t params = l4_sched_param(_prio);
	params.affinity         = l4_sched_cpu_set(cpu, 0, 1);
	l4_msgtag_t tag = l4_scheduler_run_thread(L4_BASE_SCHEDULER_CAP,
	                                          _thread.local.data()->kcap(), &params);
	if (l4_error(tag))
		PWRN("setting affinity of %lx to %d failed!", _thread.local.data()->kcap(), cpu);
}


Affinity::Location Platform_thread::affinity() const
{
	return _location;
}


static Rpc_cap_factory &thread_cap_factory()
{
	static Rpc_cap_factory inst(*platform()->core_mem_alloc());
	return inst;
}


void Platform_thread::_create_thread()
{
	l4_msgtag_t tag = l4_factory_create_thread(L4_BASE_FACTORY_CAP,
	                                           _thread.local.data()->kcap());
	if (l4_msgtag_has_error(tag))
		error("cannot create more thread kernel-objects!");

	/* create initial gate for thread */
	_gate.local = thread_cap_factory().alloc(_thread.local);
}


void Platform_thread::_finalize_construction(const char *name)
{
	/* create irq for new thread */
	l4_msgtag_t tag = l4_factory_create_irq(L4_BASE_FACTORY_CAP,
	                                        _irq.local.data()->kcap());
	if (l4_msgtag_has_error(tag))
		PWRN("creating thread's irq failed");

	/* attach thread to irq */
	tag = l4_irq_attach(_irq.local.data()->kcap(), 0, _thread.local.data()->kcap());
	if (l4_msgtag_has_error(tag))
		PWRN("attaching thread's irq failed");

	/* set human readable name in kernel debugger */
	strncpy(_name, name, sizeof(_name));
	Fiasco::l4_debugger_set_object_name(_thread.local.data()->kcap(), name);

	/* set priority of thread */
	l4_sched_param_t params = l4_sched_param(_prio);
	l4_scheduler_run_thread(L4_BASE_SCHEDULER_CAP, _thread.local.data()->kcap(),
	                        &params);
}


Weak_ptr<Address_space> Platform_thread::address_space()
{
	return _platform_pd->Address_space::weak_ptr();
}


Platform_thread::Platform_thread(size_t, const char *name, unsigned prio,
                                 Affinity::Location location, addr_t)
: _state(DEAD),
  _core_thread(false),
  _thread(true),
  _irq(true),
  _utcb(0),
  _platform_pd(0),
  _pager_obj(0),
  _prio(Cpu_session::scale_priority(DEFAULT_PRIORITY, prio))
{
	/* XXX remove const cast */
	((Core_cap_index *)_thread.local.data())->pt(this);
	_create_thread();
	_finalize_construction(name);
	affinity(location);
}


Platform_thread::Platform_thread(Core_cap_index* thread,
                                 Core_cap_index* irq, const char *name)
: _state(RUNNING),
  _core_thread(true),
  _thread(Native_capability(*thread), L4_BASE_THREAD_CAP),
  _irq(Native_capability(*irq)),
  _utcb(0),
  _platform_pd(0),
  _pager_obj(0),
  _prio(Cpu_session::scale_priority(DEFAULT_PRIORITY, 0))
{
	/* XXX remove const cast */
	((Core_cap_index *)_thread.local.data())->pt(this);
	_finalize_construction(name);
}


Platform_thread::Platform_thread(const char *name)
: _state(DEAD),
  _core_thread(true),
  _thread(true),
  _irq(true),
  _utcb(0),
  _platform_pd(0),
  _pager_obj(0),
  _prio(Cpu_session::scale_priority(DEFAULT_PRIORITY, 0))
{
	/* XXX remove const cast */
	((Core_cap_index *)_thread.local.data())->pt(this);
	_create_thread();
	_finalize_construction(name);
}


Platform_thread::~Platform_thread()
{
	thread_cap_factory().free(_gate.local);

	/*
	 * We inform our protection domain about thread destruction, which will end up in
	 * Thread::unbind()
	 */
	if (_platform_pd)
		_platform_pd->unbind_thread(this);
}
