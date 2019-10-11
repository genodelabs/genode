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

	public:

		void charge(unsigned n)
		{
			if (n > NSIG)
				return;

			_charged_signals[n].construct(_pending_signals, n);
		}

		void execute_signal_handlers()
		{
			_pending_signals.for_each([&] (Pending &pending) {
				_execute_signal_handler(pending.n);
				_charged_signals[pending.n].destruct();
			});
		}
};

#endif /* _LIBC__INTERNAL__SIGNAL_H_ */
