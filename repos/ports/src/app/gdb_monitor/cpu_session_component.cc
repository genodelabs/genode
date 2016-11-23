/*
 * \brief  Implementation of the CPU session interface
 * \author Christian Prochaska
 * \date   2011-04-28
 */

/*
 * Copyright (C) 2011-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <base/log.h>
#include <base/sleep.h>
#include <cpu_session_component.h>
#include <util/list.h>

/* GDB monitor includes */
#include "cpu_thread_component.h"

/* genode-low.cc */
extern void genode_remove_thread(unsigned long lwpid);

using namespace Genode;
using namespace Gdb_monitor;


Cpu_session &Cpu_session_component::parent_cpu_session()
{
	return _parent_cpu_session;
}


Rpc_entrypoint &Cpu_session_component::thread_ep()
{
	return _ep;
}


Signal_receiver *Cpu_session_component::exception_signal_receiver()
{
	return _exception_signal_receiver;
}


Thread_capability Cpu_session_component::thread_cap(unsigned long lwpid)
{
	Cpu_thread_component *cpu_thread = _thread_list.first();
	while (cpu_thread) {
		if (cpu_thread->lwpid() == lwpid)
			return cpu_thread->thread_cap();
		cpu_thread = cpu_thread->next();
	}
	return Thread_capability();
}


Cpu_thread_component *Cpu_session_component::lookup_cpu_thread(unsigned long lwpid)
{
	Cpu_thread_component *cpu_thread = _thread_list.first();
	while (cpu_thread) {
		if (cpu_thread->lwpid() == lwpid)
			return cpu_thread;
		cpu_thread = cpu_thread->next();
	}
	return nullptr;
}


Cpu_thread_component *Cpu_session_component::lookup_cpu_thread(Thread_capability thread_cap)
{
	Cpu_thread_component *cpu_thread = _thread_list.first();
	while (cpu_thread) {
		if (cpu_thread->thread_cap().local_name() == thread_cap.local_name())
			return cpu_thread;
		cpu_thread = cpu_thread->next();
	}
	return 0;
}


unsigned long Cpu_session_component::lwpid(Thread_capability thread_cap)
{
	return lookup_cpu_thread(thread_cap)->lwpid();
}


int Cpu_session_component::signal_pipe_read_fd(Thread_capability thread_cap)
{
	return lookup_cpu_thread(thread_cap)->signal_pipe_read_fd();
}


int Cpu_session_component::send_signal(Thread_capability thread_cap,
                                       int signo)
{
	Cpu_thread_component *cpu_thread = lookup_cpu_thread(thread_cap);

	cpu_thread->pause();

	switch (signo) {
		case SIGSTOP:
			Signal_transmitter(cpu_thread->sigstop_signal_context_cap()).submit();
			return 1;
		case SIGINT:
			Signal_transmitter(cpu_thread->sigint_signal_context_cap()).submit();
			return 1;
		default:
			error("unexpected signal ", signo);
			return 0;
	}
}


/*
 * This function delivers a SIGSEGV to the first thread with an unresolved
 * page fault that it finds. Multiple page-faulted threads are currently
 * not supported.
 */

void Cpu_session_component::handle_unresolved_page_fault()
{
	/*
	 * It can happen that the thread state of the thread which caused the
	 * page fault is not accessible yet. In that case, we'll retry until
	 * it is accessible.
	 */

	while (1) {

		Thread_capability thread_cap = first();

		while (thread_cap.valid()) {

			try {

				Cpu_thread_component *cpu_thread = lookup_cpu_thread(thread_cap);

				Thread_state thread_state = cpu_thread->state();

				if (thread_state.unresolved_page_fault) {

					/*
					 * On base-foc it is necessary to pause the thread before
					 * IP and SP are available in the thread state.
					 */
					cpu_thread->pause();
					cpu_thread->deliver_signal(SIGSEGV);

					return;
				}

			} catch (Cpu_thread::State_access_failed) { }

			thread_cap = next(thread_cap);
		}

	}
}


void Cpu_session_component::stop_new_threads(bool stop)
{
	_stop_new_threads = stop;
}


bool Cpu_session_component::stop_new_threads()
{
	return _stop_new_threads;
}


Lock &Cpu_session_component::stop_new_threads_lock()
{
	return _stop_new_threads_lock;
}


int Cpu_session_component::handle_initial_breakpoint(unsigned long lwpid)
{
	Cpu_thread_component *cpu_thread = _thread_list.first();
	while (cpu_thread) {
		if (cpu_thread->lwpid() == lwpid)
			return cpu_thread->handle_initial_breakpoint();
		cpu_thread = cpu_thread->next();
	}
	return 0;
}


void Cpu_session_component::pause_all_threads()
{
	Lock::Guard stop_new_threads_lock_guard(stop_new_threads_lock());

	stop_new_threads(true);

	for (Cpu_thread_component *cpu_thread = _thread_list.first();
	     cpu_thread;
		 cpu_thread = cpu_thread->next()) {

		cpu_thread->pause();
	}
}


void Cpu_session_component::resume_all_threads()
{
Lock::Guard stop_new_threads_guard(stop_new_threads_lock());

	stop_new_threads(false);

	for (Cpu_thread_component *cpu_thread = _thread_list.first();
	     cpu_thread;
		 cpu_thread = cpu_thread->next()) {

		cpu_thread->single_step(false);
		cpu_thread->resume();
	}
}


Thread_capability Cpu_session_component::first()
{
	Cpu_thread_component *cpu_thread = _thread_list.first();
	if (cpu_thread)
		return cpu_thread->thread_cap();
	else
		return Thread_capability();
}


Thread_capability Cpu_session_component::next(Thread_capability thread_cap)
{
	Cpu_thread_component *next_cpu_thread = lookup_cpu_thread(thread_cap)->next();
	if (next_cpu_thread)
		return next_cpu_thread->thread_cap();
	else
		return Thread_capability();
}


Thread_capability Cpu_session_component::create_thread(Capability<Pd_session> pd,
                                                       Cpu_session::Name const &name,
                                                       Affinity::Location affinity,
                                                       Weight weight,
                                                       addr_t utcb)
{
	Cpu_thread_component *cpu_thread =
		new (_md_alloc) Cpu_thread_component(*this, _core_pd, name,
		                                     affinity, weight, utcb);

	_thread_list.append(cpu_thread);

	return cpu_thread->cap();
}


void Cpu_session_component::kill_thread(Thread_capability thread_cap)
{
	Cpu_thread_component *cpu_thread = lookup_cpu_thread(thread_cap);

	if (cpu_thread) {
		genode_remove_thread(cpu_thread->lwpid());
		_thread_list.remove(cpu_thread);
		destroy(_md_alloc, cpu_thread);
	} else
		error(__PRETTY_FUNCTION__, ": "
		      "could not find thread info for the given thread capability");

	_parent_cpu_session.kill_thread(thread_cap);
}


void Cpu_session_component::exception_sigh(Signal_context_capability handler)
{
	_parent_cpu_session.exception_sigh(handler);
}


Affinity::Space Cpu_session_component::affinity_space() const
{
	return _parent_cpu_session.affinity_space();
}


Dataspace_capability Cpu_session_component::trace_control()
{
	return _parent_cpu_session.trace_control();
}


Capability<Cpu_session::Native_cpu> Cpu_session_component::native_cpu()
{
	return _native_cpu_cap;
}


Cpu_session_component::Cpu_session_component(Genode::Env &env,
                                             Rpc_entrypoint &ep,
                                             Allocator *md_alloc,
                                             Pd_session_capability core_pd,
                                             Signal_receiver *exception_signal_receiver,
                                             const char *args,
                                             Affinity const &affinity)
: _env(env),
  _ep(ep),
  _md_alloc(md_alloc),
  _core_pd(core_pd),
  _parent_cpu_session(env.session<Cpu_session>(_id_space_element.id(), args, affinity)),
  _exception_signal_receiver(exception_signal_receiver),
  _native_cpu_cap(_setup_native_cpu())
{
	_ep.manage(this);
}


Cpu_session_component::~Cpu_session_component()
{
	for (Cpu_thread_component *cpu_thread = _thread_list.first();
	     cpu_thread; cpu_thread = _thread_list.first()) {
	     _thread_list.remove(cpu_thread);
	     destroy(_md_alloc, cpu_thread);
	}

	_cleanup_native_cpu();

	_ep.dissolve(this);
}


int Cpu_session_component::ref_account(Cpu_session_capability) { return -1; }


int Cpu_session_component::transfer_quota(Cpu_session_capability, Genode::size_t) { return -1; }


Cpu_session::Quota Cpu_session_component::quota() { return Quota(); }
