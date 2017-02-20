/**
 * \brief  Hard-context for use within rump kernel
 * \author Sebastian Sumpf
 * \date   2014-02-05
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
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
#include <rump/env.h>
#include <util/avl_tree.h>


/*************
 ** Threads **
 *************/

typedef void *(*func)(void *);

namespace Timer {
	class Connection;
};


class Hard_context : public Genode::Avl_node<Hard_context>
{
	private:

		int  _cookie;
		lwp *_lwp     = nullptr;


	public:

		Genode::Thread *_myself = nullptr;

	public:

		Hard_context(int cookie)
		: _cookie(cookie){ }

		void set_lwp(lwp *l) { _lwp = l; }
		lwp *get_lwp() { return _lwp; }

		static Timer::Connection &timer();

		bool higher(Hard_context *h) { return _myself > h->_myself; }

		Hard_context *find(Genode::Thread const *t)
		{
			if (_myself == t)
				return this;

			Hard_context *h = child(_myself > t);
			if (!h)
				return nullptr;

			return h->find(t);
		}

		void thread(Genode::Thread *t) { _myself = t; }
};


class Hard_context_registry : public Genode::Avl_tree<Hard_context>
{
	private:

		Genode::Lock lock;

	public:

		Hard_context *find(Genode::Thread const *t)
		{
			Genode::Lock::Guard g(lock);
			if (!first())
				return nullptr;

			return first()->find(t);
		}

		void insert(Hard_context *h)
		{
			Genode::Lock::Guard g(lock);
			Avl_tree::insert(h);
		}

		void remove(Hard_context *h)
		{
			Genode::Lock::Guard g(lock);
			Avl_tree::remove(h);
		}

		static Hard_context_registry &r()
		{
			static Hard_context_registry _r;
			return _r;
		}
};


class Hard_context_thread : public Hard_context,
                            public Genode::Thread
{
	private:

		func  _func;
		void *_arg;

	protected:

		void entry()
		{
			Hard_context::thread(Genode::Thread::myself());
			Hard_context_registry::r().insert(this);
			_func(_arg);
			Hard_context_registry::r().remove(this);
			Genode::log(__func__, " returned from func");
		}

	public:

		Hard_context_thread(char const *name, func f, void *arg, int cookie, bool run = true)
		: Hard_context(cookie), Thread(Rump::env().env(), name, sizeof(long) * 2048),
			_func(f), _arg(arg) { if (run) start(); }
};


#endif /* _INCLUDE__HARD_CONTEXT_H_ */
