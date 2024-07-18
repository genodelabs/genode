/*
 * \brief  POSIX signal handling
 * \author Norman Feske
 * \date   2019-10-11
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC__INTERNAL__SIGNAL_H_
#define _LIBC__INTERNAL__SIGNAL_H_

/* Genode includes */
#include <util/noncopyable.h>
#include <base/registry.h>

/* libc includes */
#include <signal.h>

/* libc-internal includes */
#include <internal/call_func.h>
#include <internal/types.h>

namespace Libc { struct Signal; }


struct Libc::Signal : Noncopyable
{
	public:

		struct sigaction signal_action[NSIG + 1] { };

	private:

		struct Pending
		{
			virtual ~Pending() { }

			unsigned const n;

			Pending(unsigned n) : n(n) { }
		};

		Constructible<Registered<Pending>> _charged_signals[NSIG + 1];

		Registry<Registered<Pending>> _pending_signals { };

		void _execute_signal_handler(unsigned n);

		unsigned _count = 0;

		bool _exit = false;

		unsigned _exit_code = 0;

		unsigned _nesting_level = 0;

		pid_t const _local_pid;

		void  * _signal_stack_default     { };
		void  * _signal_stack_alternative { };
		jmp_buf _signal_context           { };

		struct Signal_arguments
		{
			Signal  & signal;
			Pending & pending;

			Signal_arguments(Signal &signal, Pending &pending)
			: signal(signal), pending(pending) { }
		};

		static void _signal_entry(Signal_arguments &arg)
		{
			arg.signal._execute_signal_handler(arg.pending.n);
			arg.signal._charged_signals[arg.pending.n].destruct();

			_longjmp(arg.signal._signal_context, 1);
		}

		void _execute_on_signal_stack(Pending &pending)
		{
			bool const onstack = signal_action[pending.n].sa_flags & SA_ONSTACK;

			void * signal_stack = (_signal_stack_alternative && onstack) ?
			                      _signal_stack_alternative :
			                      _signal_stack_default;

			if (!signal_stack) {
				auto myself = Thread::myself();
				if (myself)
					_signal_stack_default = { myself->alloc_secondary_stack("signal", 16 * 1024) };

				signal_stack = _signal_stack_default;
			}

			if (!signal_stack) {
				error(__func__, " signal stack allocation failed");
				return;
			}

			Signal_arguments const arg(*this, pending);

			/* save continuation of current stack */
			if (!_setjmp(_signal_context)) {
				/* _setjmp() returned directly -> switch to signal stack */
				call_func(signal_stack, (void *)_signal_entry, (void *)&arg);

				/* never reached */
			}
			/* _setjmp() returned after _longjmp() */
		}

	public:

		Signal(pid_t local_pid) : _local_pid(local_pid) { }

		void charge(unsigned n)
		{
			if (n > NSIG)
				return;

			_charged_signals[n].construct(_pending_signals, n);
			_count++;
		}

		void use_alternative_stack(void *ptr) {
			_signal_stack_alternative = ptr; }

		void execute_signal_handlers()
		{
			/*
			 * Prevent nested execution of signal handlers, which may happen
			 * if I/O operations are executed by a signal handler.
			 */
			if (_nesting_level > 0) {
				warning("attempt to nested execution of signal handlers");
				return;
			}

			_nesting_level++;

			_pending_signals.for_each([&] (Pending &pending) {
				_execute_on_signal_stack(pending);
			});

			_nesting_level--;

			/*
			 * Exit application due to a signal such as SIGINT.
			 */
			if (_exit)
				exit(_exit_code);
		}

		/**
		 * Return number of triggered signals
		 *
		 * The value is intended to be used for tracking whether a signal
		 * occurred during a blocking operation ('select').
		 */
		unsigned count() const { return _count; }

		/**
		 * Return true if specified PID belongs to the process itself
		 */
		bool local_pid(pid_t pid) const { return pid == _local_pid; }
};

#endif /* _LIBC__INTERNAL__SIGNAL_H_ */
