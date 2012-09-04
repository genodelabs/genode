/*
 * \brief  Implementation of the CPU session interface
 * \author Christian Prochaska
 * \date   2011-04-28
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <base/printf.h>
#include <cpu_session_component.h>
#include <util/list.h>

/* GDB monitor includes */
#include "config.h"

extern void genode_add_thread(unsigned long lwpid);
extern void genode_remove_thread(unsigned long lwpid);

using namespace Genode;
using namespace Gdb_monitor;

/* FIXME: use an allocator */
static unsigned long new_lwpid = GENODE_LWP_BASE;


Thread_info *Cpu_session_component::_thread_info(Thread_capability thread_cap)
{
	Thread_info *thread_info = _thread_list.first();
	while (thread_info) {
		if (thread_info->thread_cap().local_name() == thread_cap.local_name()) {
			return thread_info;
			break;
		}
		thread_info = thread_info->next();
	}
	return 0;
}


unsigned long Cpu_session_component::lwpid(Thread_capability thread_cap)
{
	return _thread_info(thread_cap)->lwpid();
}


Thread_capability Cpu_session_component::thread_cap(unsigned long lwpid)
{
	Thread_info *thread_info = _thread_list.first();
	while (thread_info) {
		if (thread_info->lwpid() == lwpid) {
			return thread_info->thread_cap();
		}
		thread_info = thread_info->next();
	}
	return Thread_capability();
}


Thread_capability Cpu_session_component::create_thread(Cpu_session::Name const &name, addr_t utcb)
{
	Thread_capability thread_cap =
		_parent_cpu_session.create_thread(name.string(), utcb);

	if (thread_cap.valid()) {
		Thread_info *thread_info = new (env()->heap()) Thread_info(thread_cap, new_lwpid++);
		_thread_list.append(thread_info);
	}

	return thread_cap;
}


Ram_dataspace_capability Cpu_session_component::utcb(Thread_capability thread)
{
	return _parent_cpu_session.utcb(thread);
}


void Cpu_session_component::kill_thread(Thread_capability thread_cap)
{
	Thread_info *thread_info = _thread_info(thread_cap);

	if (thread_info) {
		_exception_signal_receiver->dissolve(thread_info);
		genode_remove_thread(thread_info->lwpid());
		_thread_list.remove(thread_info);
		destroy(env()->heap(), thread_info);
	}

	_parent_cpu_session.kill_thread(thread_cap);
}


Thread_capability Cpu_session_component::first()
{
	Thread_info *thread_info = _thread_list.first();
	if (thread_info)
		return thread_info->thread_cap();
	else
		return Thread_capability();
}


Thread_capability Cpu_session_component::next(Thread_capability thread_cap)
{
	Thread_info *next_thread_info = _thread_info(thread_cap)->next();
	if (next_thread_info)
		return next_thread_info->thread_cap();
	else
		return Thread_capability();
}


int Cpu_session_component::set_pager(Thread_capability thread_cap,
                                     Pager_capability  pager_cap)
{
	return _parent_cpu_session.set_pager(thread_cap, pager_cap);
}


int Cpu_session_component::start(Thread_capability thread_cap,
                                 addr_t ip, addr_t sp)
{
	Thread_info *thread_info = _thread_info(thread_cap);

	if (thread_info)
		exception_handler(thread_cap, _exception_signal_receiver->manage(thread_info));

	int result = _parent_cpu_session.start(thread_cap, ip, sp);

	if (thread_info) {
		/* pause the first thread */
		if (thread_info->lwpid() == GENODE_LWP_BASE)
			pause(thread_cap);

		genode_add_thread(thread_info->lwpid());
	}

	return result;
}


void Cpu_session_component::pause(Thread_capability thread_cap)
{
	_parent_cpu_session.pause(thread_cap);
}


void Cpu_session_component::resume(Thread_capability thread_cap)
{
	_parent_cpu_session.resume(thread_cap);
}


void Cpu_session_component::cancel_blocking(Thread_capability thread_cap)
{
	_parent_cpu_session.cancel_blocking(thread_cap);
}


int Cpu_session_component::state(Thread_capability thread_cap,
                                 Thread_state *state_dst)
{
	return _parent_cpu_session.state(thread_cap, state_dst);
}


void Cpu_session_component::exception_handler(Thread_capability         thread_cap,
                                              Signal_context_capability sigh_cap)
{
	_parent_cpu_session.exception_handler(thread_cap, sigh_cap);
}


void Cpu_session_component::single_step(Thread_capability thread_cap, bool enable)
{
	_parent_cpu_session.single_step(thread_cap, enable);
}


unsigned Cpu_session_component::num_cpus() const
{
	return _parent_cpu_session.num_cpus();
}


void Cpu_session_component::affinity(Thread_capability thread_cap, unsigned cpu)
{
	_parent_cpu_session.affinity(thread_cap, cpu);
}


Cpu_session_component::Cpu_session_component(Signal_receiver *exception_signal_receiver, const char *args)
: _parent_cpu_session(env()->parent()->session<Cpu_session>(args)),
  _exception_signal_receiver(exception_signal_receiver)
{
}


Cpu_session_component::~Cpu_session_component()
{
}
