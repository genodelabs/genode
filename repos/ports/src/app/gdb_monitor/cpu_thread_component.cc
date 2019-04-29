/*
 * \brief  Cpu_thread_component class for GDB monitor
 * \author Christian Prochaska
 * \date   2016-05-12
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


/* GDB monitor includes */
#include "cpu_thread_component.h"
#include "genode-low.h"

/* libc includes */
#include <signal.h>
#include <unistd.h>


static unsigned long new_lwpid = GENODE_MAIN_LWPID;


using namespace Gdb_monitor;


bool Cpu_thread_component::_set_breakpoint_at_first_instruction(addr_t ip)
{
	_breakpoint_ip = ip;

	if (genode_read_memory(_breakpoint_ip, _original_instructions,
	                       _breakpoint_len) != 0) {
		warning(__PRETTY_FUNCTION__, ": could not read memory at thread start address");
		return false;
	}

	if (genode_write_memory(_breakpoint_ip, _breakpoint_data,
	                        _breakpoint_len) != 0) {
		warning(__PRETTY_FUNCTION__, ": could not set breakpoint at thread start address");
		return false;
	}

	return true;
}


void Cpu_thread_component::_remove_breakpoint_at_first_instruction()
{
	if (genode_write_memory(_breakpoint_ip, _original_instructions,
	                        _breakpoint_len) != 0)
		warning(__PRETTY_FUNCTION__, ": could not remove breakpoint at thread start address");
}


void Cpu_thread_component::_handle_exception()
{
	deliver_signal(SIGTRAP);
}


void Cpu_thread_component::_handle_sigstop()
{
	deliver_signal(SIGSTOP);
}


void Cpu_thread_component::_handle_sigint()
{
	deliver_signal(SIGINT);
}


Cpu_thread_component::Cpu_thread_component(Cpu_session_component   &cpu_session_component,
		                                   Capability<Pd_session>   pd,
                                           Cpu_session::Name const &name,
                                           Affinity::Location       affinity,
                                           Cpu_session::Weight      weight,
                                           addr_t                   utcb,
                                           int const                new_thread_pipe_write_end,
                                           int const                breakpoint_len,
                                           unsigned char const     *breakpoint_data)
:
	_cpu_session_component(cpu_session_component),
	_parent_cpu_thread(
		_cpu_session_component.parent_cpu_session().create_thread(pd,
		                                                          name,
		                                                          affinity,
		                                                          weight,
		                                                          utcb)),
	_new_thread_pipe_write_end(new_thread_pipe_write_end),
	_breakpoint_len(breakpoint_len),
	_breakpoint_data(breakpoint_data),
	_exception_handler(_cpu_session_component.signal_ep(), *this,
	                   &Cpu_thread_component::_handle_exception),
	_sigstop_handler(_cpu_session_component.signal_ep(), *this,
	                 &Cpu_thread_component::_handle_sigstop),
	_sigint_handler(_cpu_session_component.signal_ep(), *this,
	                &Cpu_thread_component::_handle_sigint)
{
	_cpu_session_component.thread_ep().manage(this);

	if (pipe(_pipefd) != 0)
		error("could not create pipe");
}


Cpu_thread_component::~Cpu_thread_component()
{
	close(_pipefd[0]);
	close(_pipefd[1]);

	_cpu_session_component.thread_ep().dissolve(this);
}


int Cpu_thread_component::send_signal(int signo)
{
	pause();

	switch (signo) {
		case SIGSTOP:
			Signal_transmitter(sigstop_signal_context_cap()).submit();
			return 1;
		case SIGINT:
			Signal_transmitter(sigint_signal_context_cap()).submit();
			return 1;
		default:
			error("unexpected signal ", signo);
			return 0;
	}
}


int Cpu_thread_component::deliver_signal(int signo)
{
	if ((signo == SIGTRAP) && _initial_sigtrap_pending) {

		_initial_sigtrap_pending = false;

		if (_verbose)
			log("received initial SIGTRAP for lwpid ", _lwpid);

		if (_lwpid == GENODE_MAIN_LWPID) {
			_remove_breakpoint_at_first_instruction();
			_initial_breakpoint_handled = true;
		}

		/*
		 * The lock guard prevents an interruption by
		 * 'genode_stop_all_threads()', which could cause
		 * the new thread to be resumed when it should be
		 * stopped.
		 */

		Lock::Guard stop_new_threads_lock_guard(
			_cpu_session_component.stop_new_threads_lock());

		if (!_cpu_session_component.stop_new_threads())
			resume();

		/*
		 * gdbserver expects SIGSTOP as first signal of a new thread,
		 * but we cannot write SIGSTOP here, because waitpid() would
		 * detect that the thread is in an exception state and wait
		 * for the SIGTRAP. So SIGINFO ist used for this purpose.
		 */
		signo = SIGINFO;
	}

	switch (signo) {
		case SIGSTOP:
			if (_verbose)
				log("delivering SIGSTOP to thread ", _lwpid);
			break;
		case SIGTRAP:
			if (_verbose)
				log("delivering SIGTRAP to thread ", _lwpid);
			break;
		case SIGSEGV:
			if (_verbose)
				log("delivering SIGSEGV to thread ", _lwpid);
			break;
		case SIGINT:
			if (_verbose)
				log("delivering SIGINT to thread ", _lwpid);
			break;
		case SIGINFO:
			if (_verbose)
				if (_lwpid != GENODE_MAIN_LWPID)
					log("delivering initial SIGSTOP to thread ", _lwpid);
			break;
		default:
			error("unexpected signal ", signo);
	}

	if (!((signo == SIGINFO) && (_lwpid == GENODE_MAIN_LWPID)))
		write(_pipefd[1], &signo, sizeof(signo));

	/*
	 * gdbserver might be blocking in 'waitpid()' without having
	 * the new thread's pipe fd in its 'select' fd set yet. Writing
	 * into the 'new thread pipe' here will unblock 'select' in this
	 * case.
	 */
	if (signo == SIGINFO)
		write(_new_thread_pipe_write_end, &_lwpid, sizeof(_lwpid));

	return 0;
}

Dataspace_capability Cpu_thread_component::utcb()
{
	return _parent_cpu_thread.utcb();
}


void Cpu_thread_component::start(addr_t ip, addr_t sp)
{
	_lwpid = new_lwpid++;
	_initial_ip = ip;

	/* register the exception handler */
	exception_sigh(exception_signal_context_cap());

	/* set breakpoint at first instruction */
	if (lwpid() == GENODE_MAIN_LWPID)
		_set_breakpoint_at_first_instruction(ip);
	else
		genode_set_initial_breakpoint_at(ip);

	_parent_cpu_thread.start(ip, sp);
}


void Cpu_thread_component::pause()
{
	_parent_cpu_thread.pause();
}


void Cpu_thread_component::resume()
{
	_parent_cpu_thread.resume();
}


void Cpu_thread_component::single_step(bool enable)
{
	_parent_cpu_thread.single_step(enable);
}


void Cpu_thread_component::cancel_blocking()
{
	_parent_cpu_thread.cancel_blocking();
}


void Cpu_thread_component::state(Thread_state const &state)
{
	_parent_cpu_thread.state(state);
}


Thread_state Cpu_thread_component::state()
{
	return _parent_cpu_thread.state();
}


void Cpu_thread_component::exception_sigh(Signal_context_capability handler)
{
	_parent_cpu_thread.exception_sigh(handler);
}


void Cpu_thread_component::affinity(Affinity::Location location)
{
	_parent_cpu_thread.affinity(location);
}


unsigned Cpu_thread_component::trace_control_index()
{
	return _parent_cpu_thread.trace_control_index();
}


Dataspace_capability Cpu_thread_component::trace_buffer()
{
	return _parent_cpu_thread.trace_buffer();
}


Dataspace_capability Cpu_thread_component::trace_policy()
{
	return _parent_cpu_thread.trace_policy();
}
