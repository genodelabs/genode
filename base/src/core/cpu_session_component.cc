/**
 * \brief  Core implementation of the CPU session/thread interfaces
 * \author Christian Helmuth
 * \date   2006-07-17
 *
 * FIXME arg_string and quota missing
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <util/arg_string.h>

/* Core includes */
#include <cpu_session_component.h>
#include <rm_session_component.h>

using namespace Genode;


Thread_capability Cpu_session_component::create_thread(Name const &name, addr_t utcb)
{
	Lock::Guard thread_list_lock_guard(_thread_list_lock);
	Lock::Guard slab_lock_guard(_thread_alloc_lock);

	Cpu_thread_component *thread = 0;
	try {
		thread = new(&_thread_alloc) Cpu_thread_component(name.string(),
		                                                  _priority, utcb);
	} catch (Allocator::Out_of_memory) {
		throw Thread_creation_failed();
	}

	_thread_list.insert(thread);
	return _thread_ep->manage(thread);
}


void Cpu_session_component::_unsynchronized_kill_thread(Cpu_thread_component *thread)
{
	Lock::Guard lock_guard(_thread_alloc_lock);

	_thread_ep->dissolve(thread);
	_thread_list.remove(thread);

	/* If the thread is associated with a rm_session dissolve it */
	Rm_client *rc = dynamic_cast<Rm_client*>(thread->platform_thread()->pager());
	if (rc)
		rc->member_rm_session()->dissolve(rc);

	destroy(&_thread_alloc, thread);
}


void Cpu_session_component::kill_thread(Thread_capability thread_cap)
{
	Lock::Guard lock_guard(_thread_list_lock);

	Cpu_thread_component *thread = _lookup_thread(thread_cap);
	if (!thread) return;

	_unsynchronized_kill_thread(thread);
}


Thread_capability Cpu_session_component::first()
{
	Lock::Guard lock_guard(_thread_list_lock);

	return _thread_list.first() ? _thread_list.first()->cap()
	                            : Thread_capability();
}


Thread_capability Cpu_session_component::next(Thread_capability thread_cap)
{
	Lock::Guard lock_guard(_thread_list_lock);

	Cpu_thread_component *thread = _lookup_thread(thread_cap);

	if (!thread || !thread->next())
		return Thread_capability();

	return Thread_capability(thread->next()->cap());
}


int Cpu_session_component::set_pager(Thread_capability thread_cap,
                                     Pager_capability  pager_cap)
{
	Cpu_thread_component *thread = _lookup_thread(thread_cap);
	if (!thread) return -1;

	Pager_object *p = dynamic_cast<Pager_object *>(_pager_ep->obj_by_cap(pager_cap));
	if (!p) return -2;

	thread->platform_thread()->pager(p);
	return 0;
}


int Cpu_session_component::start(Thread_capability thread_cap,
                                 addr_t ip, addr_t sp)
{
	Cpu_thread_component *thread = _lookup_thread(thread_cap);
	if (!thread) return -1;

	thread->platform_thread()->start((void *)ip, (void *)sp);
	return 0;
}


void Cpu_session_component::pause(Thread_capability thread_cap)
{
	Cpu_thread_component *thread = _lookup_thread(thread_cap);
	if (!thread) return;

	thread->platform_thread()->pause();
}


void Cpu_session_component::resume(Thread_capability thread_cap)
{
	Cpu_thread_component *thread = _lookup_thread(thread_cap);
	if (!thread) return;

	thread->platform_thread()->resume();
}


void Cpu_session_component::cancel_blocking(Thread_capability thread_cap)
{
	Cpu_thread_component *thread = _lookup_thread(thread_cap);

	if (thread)
		thread->platform_thread()->cancel_blocking();
}


int Cpu_session_component::state(Thread_capability thread_cap,
                                 Thread_state *state_dst)
{
	Cpu_thread_component *thread = _lookup_thread(thread_cap);
	if (!thread) return -1;

	thread->platform_thread()->state(state_dst);
	return 0;
}


void Cpu_session_component::exception_handler(Thread_capability         thread_cap,
                                              Signal_context_capability sigh_cap)
{
	Cpu_thread_component *thread = _lookup_thread(thread_cap);
	if (!thread || !thread->platform_thread()->pager()) return;

	thread->platform_thread()->pager()->exception_handler(sigh_cap);
}


Cpu_session_component::Cpu_session_component(Rpc_entrypoint *thread_ep,
                                             Pager_entrypoint *pager_ep,
                                             Allocator *md_alloc,
                                             const char *args)
: _thread_ep(thread_ep), _pager_ep(pager_ep),
  _md_alloc(md_alloc, Arg_string::find_arg(args, "ram_quota").long_value(0)),
  _thread_alloc(&_md_alloc), _priority(0)
{
	Arg a = Arg_string::find_arg(args, "priority");
	if (a.valid()) {
		_priority = a.ulong_value(0);

		/* clamp priority value to valid range */
		_priority = min((unsigned)PRIORITY_LIMIT - 1, _priority);
	}
}


Cpu_session_component::~Cpu_session_component()
{
	Lock::Guard lock_guard(_thread_list_lock);

	/*
	 * We have to keep the '_thread_list_lock' during the whole destructor to
	 * prevent races with incoming calls of the 'create_thread' function,
	 * adding new threads while we are destroying them.
	 */

	for (Cpu_thread_component *thread; (thread = _thread_list.first()); )
		_unsynchronized_kill_thread(thread);
}
