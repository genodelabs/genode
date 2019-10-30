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

	public:

		Signal(pid_t local_pid) : _local_pid(local_pid) { }

		void charge(unsigned n)
		{
			if (n > NSIG)
				return;

			_charged_signals[n].construct(_pending_signals, n);
			_count++;
		}

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
				_execute_signal_handler(pending.n);
				_charged_signals[pending.n].destruct();
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
