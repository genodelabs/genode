/**
 * \brief  Hard-context for use within rump kernel
 * \author Sebastian Sumpf
 * \date   2014-02-05
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__HARD_CONTEXT_H_
#define _INCLUDE__HARD_CONTEXT_H_

extern "C" {
#include <sys/cdefs.h>
#include <sys/types.h>
#include <rump/rump.h>
}

#include <base/thread.h>
#include <base/log.h>


/*************
 ** Threads **
 *************/

typedef void *(*func)(void *);

namespace Timer {
	class Connection;
};


class Hard_context
{
	private:

		int  _cookie;
		lwp *_lwp     = 0;

	public:

		Hard_context(int cookie)
		: _cookie(cookie){ }

		void set_lwp(lwp *l) { _lwp = l; }
		lwp *get_lwp() { return _lwp; }

		static Timer::Connection *timer();
};


class Hard_context_thread : public Hard_context,
                            public Genode::Thread_deprecated<sizeof(Genode::addr_t) * 2048>
{
	private:

		func  _func;
		void *_arg;

	protected:

		void entry()
		{
			_func(_arg);
			Genode::log(__func__, " returned from func");
		}

	public:

		Hard_context_thread(char const *name, func f, void *arg, int cookie, bool run = true)
		: Hard_context(cookie), Thread_deprecated(name),
			_func(f), _arg(arg) { if (run) start(); }
};


#endif /* _INCLUDE__HARD_CONTEXT_H_ */
