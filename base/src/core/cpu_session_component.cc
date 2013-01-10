/*
 * \brief  Core implementation of the CPU session/thread interfaces
 * \author Christian Helmuth
 * \date   2006-07-17
 *
 * FIXME arg_string and quota missing
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <util/arg_string.h>

/* core includes */
#include <cpu_session_component.h>
#include <rm_session_component.h>
#include <platform_generic.h>

using namespace Genode;


void Cpu_thread_component::update_exception_sigh()
{
	if (platform_thread()->pager())
		platform_thread()->pager()->exception_handler(_sigh);
};


Thread_capability Cpu_session_component::create_thread(Name const &name,
                                                       addr_t utcb)
{
	Cpu_thread_component *thread = 0;
	try {
		Lock::Guard slab_lock_guard(_thread_alloc_lock);
		thread = new(&_thread_alloc) Cpu_thread_component(name.string(),
		                                                  _priority, utcb,
		                                                  _default_exception_handler);
	} catch (Allocator::Out_of_memory) {
		throw Out_of_metadata();
	}

	Lock::Guard thread_list_lock_guard(_thread_list_lock);
	_thread_list.insert(thread);

	return _thread_ep->manage(thread);
}


void Cpu_session_component::_unsynchronized_kill_thread(Cpu_thread_component *thread)
{
	_thread_ep->dissolve(thread);
	_thread_list.remove(thread);

	Lock::Guard lock_guard(_thread_alloc_lock);
	destroy(&_thread_alloc, thread);
}


void Cpu_session_component::kill_thread(Thread_capability thread_cap)
{
	Cpu_thread_component * thread =
		dynamic_cast<Cpu_thread_component *>(_thread_ep->lookup_and_lock(thread_cap));
	if (!thread) return;

	Lock::Guard lock_guard(_thread_list_lock);
	_unsynchronized_kill_thread(thread);
}


int Cpu_session_component::set_pager(Thread_capability thread_cap,
                                     Pager_capability  pager_cap)
{
	Object_pool<Cpu_thread_component>::Guard thread(_thread_ep->lookup_and_lock(thread_cap));
	if (!thread) return -1;

	Object_pool<Pager_object>::Guard p(_pager_ep->lookup_and_lock(pager_cap));
	if (!p) return -2;

	thread->platform_thread()->pager(p);

	p->thread_cap(thread->cap());
   
	return 0;
}


int Cpu_session_component::start(Thread_capability thread_cap,
                                 addr_t ip, addr_t sp)
{
	Object_pool<Cpu_thread_component>::Guard thread(_thread_ep->lookup_and_lock(thread_cap));
	if (!thread) return -1;

	/*
	 * If an exception handler was installed prior to the call of 'set_pager',
	 * we need to update the pager object with the current exception handler.
	 */
	thread->update_exception_sigh();

	return thread->platform_thread()->start((void *)ip, (void *)sp);
}


void Cpu_session_component::pause(Thread_capability thread_cap)
{
	Object_pool<Cpu_thread_component>::Guard thread(_thread_ep->lookup_and_lock(thread_cap));
	if (!thread) return;

	thread->platform_thread()->pause();
}


void Cpu_session_component::resume(Thread_capability thread_cap)
{
	Object_pool<Cpu_thread_component>::Guard thread(_thread_ep->lookup_and_lock(thread_cap));
	if (!thread) return;

	thread->platform_thread()->resume();
}


void Cpu_session_component::cancel_blocking(Thread_capability thread_cap)
{
	Object_pool<Cpu_thread_component>::Guard thread(_thread_ep->lookup_and_lock(thread_cap));
	if (!thread) return;

	thread->platform_thread()->cancel_blocking();
}


Thread_state Cpu_session_component::state(Thread_capability thread_cap)
{
	Object_pool<Cpu_thread_component>::Guard thread(_thread_ep->lookup_and_lock(thread_cap));
	if (!thread) throw State_access_failed();

	return thread->platform_thread()->state();
}


void Cpu_session_component::state(Thread_capability thread_cap,
                                  Thread_state const &state)
{
	Object_pool<Cpu_thread_component>::Guard thread(_thread_ep->lookup_and_lock(thread_cap));
	if (!thread) throw State_access_failed();

	thread->platform_thread()->state(state);
}


void
Cpu_session_component::exception_handler(Thread_capability         thread_cap,
                                         Signal_context_capability sigh_cap)
{
	/*
	 * By specifying an invalid thread capability, the caller sets the default
	 * exception handler for the CPU session.
	 */
	if (!thread_cap.valid()) {
		_default_exception_handler = sigh_cap;
		return;
	}

	/*
	 * If an invalid signal handler is specified for a valid thread, we revert
	 * the signal handler to the CPU session's default signal handler.
	 */
	if (!sigh_cap.valid()) {
		sigh_cap = _default_exception_handler;
	}

	Object_pool<Cpu_thread_component>::Guard thread(_thread_ep->lookup_and_lock(thread_cap));
	if (!thread) return;

	thread->sigh(sigh_cap);
}


unsigned Cpu_session_component::num_cpus() const
{
	return platform()->num_cpus();
}


void Cpu_session_component::affinity(Thread_capability thread_cap, unsigned cpu)
{
	Object_pool<Cpu_thread_component>::Guard thread(_thread_ep->lookup_and_lock(thread_cap));
	if (!thread) return;

	thread->platform_thread()->affinity(cpu);
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
