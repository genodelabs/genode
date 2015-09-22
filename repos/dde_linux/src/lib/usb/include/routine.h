/*
 * \brief  Pseudo-thread implementation using setjmp/longjmp
 * \author Sebastian Sumpf <Sebastian.Sumpf@genode-labs.com>
 * \date   2012-04-25
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _ROUTINE_H_
#define _ROUTINE_H_

#include <base/env.h>
#include <util/list.h>
#include <setjmp.h>

#include <platform/platform.h>

static const bool verbose = false;

namespace Timer {
	void update_jiffies();
}
/**
 * Allows pseudo-parallel execution of functions
 */
class Routine : public Genode::List<Routine>::Element
{
	private:

		enum { STACK_SIZE = 4 * 1024 * sizeof(long) };
		bool            _started;        /* true if already started */
		jmp_buf         _env;            /* state */
		int           (*_func)(void *);  /* function to call*/
		void           *_arg;            /* argument for function */
		char const     *_name;           /* name of this object */
		char           *_stack;          /* stack pointer */
		static Routine *_current;        /* currently scheduled object */
		static Routine *_dead;           /* object to remove */
		static Routine *_main;           /* main routine */
		static bool     _all;            /* true when all objects must be scheduled */


		/**
		 * List containing all registered objects
		 */
		static Genode::List<Routine> *_list()
		{
			static Genode::List<Routine> _l;
			return &_l;
		}

		/**
		 * Start/restore
		 */
		void _run()
		{
			/* will never return */
			if (!_started) {
				_started = true;
				Genode::Thread_base *th = Genode::Thread_base::myself();
				_stack = (char *) th->alloc_secondary_stack(_name, STACK_SIZE);

				if (verbose)
					PDBG("Start func %s (%p) sp: %p", _name, _func, _stack);

				/* XXX  move to platform code */

				/* switch stack and call '_func(_arg)' */
				platform_execute((void *)(_stack), (void *)_func, _arg);
			}

			/* restore old state */
			if (verbose)
				PDBG("Schedule %s (%p)", _name, _func);

			_longjmp(_env, 1);
		}

		/**
		 * Check for and remove dead objects
		 */
		static void _check_dead()
		{
			if (!_dead)
				return;

			_list()->remove(_dead);
			destroy(Genode::env()->heap(), _dead);
			_dead = 0;
		}

		/**
		 * Get next object to schedule
		 */
		static Routine *_next(bool all)
		{
			/* on schedule all start at first element */
			if (all) {
				_all = true;
				return _list()->first();
			}

			/* disable all at last element */
			if (_all && _current && !_current->next())
				_all = false;

			/* return next element (wrap at the end) */
			return _current && _current->next() ? _current->next() : _list()->first();
		}

	public:

		Routine(int (*func)(void*), void *arg, char const *name, bool started)
		: _started(started), _func(func), _arg(arg), _name(name), _stack(0) { }

		~Routine()
		{
			if (_stack)
				Genode::Thread_base::myself()->free_secondary_stack(_stack);
		}

		/**
		 * Schedule next object
		 *
		 * If all is true, each object will be scheduled once.
		 */
		static void schedule(bool all = false, bool main = false)
			__attribute__((noinline))
		{
			if (!_list()->first() && !_main)
				return;

			if (_current == _main)
				all = true;

			Routine *next = main ? _main : _next(all);

			if (next == _current) {
				_check_dead();
				return;
			}

			/* return when restored */
			if (_current && _setjmp(_current->_env)) {
					_check_dead();
					return;
			}

			_current = next;
			_current->_run();
		}

		/**
		 * Schedule each object once
		 */
		static void schedule_all() { schedule(true); }

		/**
		 * Set current to first object
		 */
		static void make_main_current() { _main = _current = _list()->first(); }

		/**
		 * Add an object
		 */
		static Routine *add(int (*func)(void *), void *arg, char const *name,
		                    bool started = false)
		{
			Routine *r = new (Genode::env()->heap()) Routine(func, arg, name, started);
			_list()->insert(r);
			return r;
		}

		/**
		 * Remove this object
		 */
		static void remove(Routine *r = nullptr)
		{
			if (!_current && !r)
				return;

			_dead = r ? r : _current;

			schedule_main();
		}

		static void main()
		{
			if (!_current)
				return;

			_list()->remove(_current);

			if (_main && _setjmp(_main->_env))
				return;

			schedule();
		}

		static void schedule_main() { schedule(false, true); }

		/**
		 * True when 'schedule_all' has been called and is still in progress
		 */
		static bool all() { return _all; }
};

#endif /* _ROUTINE_H_ */

