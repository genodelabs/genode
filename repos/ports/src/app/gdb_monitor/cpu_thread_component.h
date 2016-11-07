/*
 * \brief  Cpu_thread_component class for GDB monitor
 * \author Christian Prochaska
 * \date   2016-05-12
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CPU_THREAD_COMPONENT_H_
#define _CPU_THREAD_COMPONENT_H_

/* Genode includes */
#include <base/thread.h>
#include <cpu_session/cpu_session.h>
#include <cpu_thread/client.h>

/* libc includes */
#include <signal.h>
#include <unistd.h>

#include "config.h"
#include "append_list.h"
#include "cpu_session_component.h"

extern "C" int delete_gdb_breakpoint_at(long long where);

namespace Gdb_monitor { class Cpu_thread_component; }

class Gdb_monitor::Cpu_thread_component : public Rpc_object<Cpu_thread>,
                                          public Append_list<Cpu_thread_component>::Element
{
	private:

		static constexpr bool  _verbose = false;

		Cpu_session_component &_cpu_session_component;
		Cpu_thread_client      _parent_cpu_thread;
		unsigned long          _lwpid;
		addr_t                 _initial_ip;

		/*
		 * SIGTRAP, SIGSTOP and SIGINT must get delivered to the gdbserver code
		 * in the same order that they were generated. Since these signals are
		 * generated by different threads, the exception signal receiver is
		 * used as synchronization point.
		 */
		Signal_dispatcher<Cpu_thread_component> _exception_dispatcher;
		Signal_dispatcher<Cpu_thread_component> _sigstop_dispatcher;
		Signal_dispatcher<Cpu_thread_component> _sigint_dispatcher;

		int                    _pipefd[2];
		bool                   _initial_sigtrap_pending = true;
		bool                   _initial_breakpoint_handled = false;

		/* data for breakpoint at first instruction */
		enum { MAX_BREAKPOINT_LEN = 8 }; /* value from mem-break.c */
		unsigned char _original_instructions[MAX_BREAKPOINT_LEN];
		addr_t _breakpoint_ip;

		bool _set_breakpoint_at_first_instruction(addr_t ip);
		void _remove_breakpoint_at_first_instruction();

		void _dispatch_exception(unsigned)
		{
			deliver_signal(SIGTRAP);
		}

		void _dispatch_sigstop(unsigned)
		{
			deliver_signal(SIGSTOP);
		}

		void _dispatch_sigint(unsigned)
		{
			deliver_signal(SIGINT);
		}

	public:

		Cpu_thread_component(Cpu_session_component &cpu_session_component,
		                     Capability<Pd_session> pd,
		                     Cpu_session::Name const &name,
		                     Affinity::Location affinity,
		                     Cpu_session::Weight weight,
		                     addr_t utcb)
		: _cpu_session_component(cpu_session_component),
		  _parent_cpu_thread(
		      _cpu_session_component.parent_cpu_session().create_thread(pd,
		                                                                name,
		                                                                affinity,
		                                                                weight,
		                                                                utcb)),
		  _exception_dispatcher(
		      *_cpu_session_component.exception_signal_receiver(),
		      *this,
		      &Cpu_thread_component::_dispatch_exception),
		  _sigstop_dispatcher(
		      *_cpu_session_component.exception_signal_receiver(),
		      *this,
		      &Cpu_thread_component::_dispatch_sigstop),
		  _sigint_dispatcher(
		      *_cpu_session_component.exception_signal_receiver(),
		      *this,
		      &Cpu_thread_component::_dispatch_sigint)
		{
			_cpu_session_component.thread_ep().manage(this);

			if (pipe(_pipefd) != 0)
				error("could not create pipe");
		}

		~Cpu_thread_component()
		{
			close(_pipefd[0]);
			close(_pipefd[1]);

			_cpu_session_component.thread_ep().dissolve(this);
		}

		Signal_context_capability exception_signal_context_cap()
		{
			return _exception_dispatcher;
		}

		Signal_context_capability sigstop_signal_context_cap()
		{
			return _sigstop_dispatcher;
		}

		Signal_context_capability sigint_signal_context_cap()
		{
			return _sigint_dispatcher;
		}

		Thread_capability thread_cap() { return cap(); }
		unsigned long lwpid() { return _lwpid; }

		Thread_capability parent_thread_cap() { return _parent_cpu_thread; }

		int signal_pipe_read_fd() { return _pipefd[0]; }

		int handle_initial_breakpoint()
		{
			if (!_initial_breakpoint_handled) {
				_initial_breakpoint_handled = true;
				return 1;
			}

			return 0;
		}

		int send_signal(int signo)
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

		int deliver_signal(int signo)
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
						log("delivering initial SIGSTOP to thread ", _lwpid);
					break;
				default:
					error("unexpected signal ", signo);
			}

			write(_pipefd[1], &signo, sizeof(signo));

			return 0;
		}


		/**************************
		 ** CPU thread interface **
		 *************************/

		Dataspace_capability utcb() override;
		void start(addr_t, addr_t) override;
		void pause() override;
		void resume() override;
		void single_step(bool) override;
		void cancel_blocking() override;
		Thread_state state() override;
		void state(Thread_state const &) override;
		void exception_sigh(Signal_context_capability) override;
		void affinity(Affinity::Location) override;
		unsigned trace_control_index() override;
		Dataspace_capability trace_buffer() override;
		Dataspace_capability trace_policy() override;
};

#endif /* _CPU_THREAD_COMPONENT_H_ */
