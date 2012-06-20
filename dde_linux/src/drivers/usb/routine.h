/*
 * \brief  Pseudo-thread implementation using setjmp/longjmp
 * \author Sebastian Sumpf <Sebastian.Sumpf@genode-labs.com>
 * \date   2012-04-25
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _ROUTINE_H_
#define _ROUTINE_H_

#include <base/env.h>
#include <util/list.h>
#include <libc/setjmp.h>

extern "C" {
#include <dde_kit/memory.h>
}

#include <platform/platform.h>

static const bool verbose = false;


/**
 * Allows pseudo-parallel execution of functions
 */
class Routine : public Genode::List<Routine>::Element
{
	private:

		enum { STACK_SIZE = 0x2000 };
		bool            _started;        /* true if already started */
		jmp_buf         _env;            /* state */
		int           (*_func)(void *);  /* function to call*/
		void           *_arg;            /* argument for function */
		char const     *_name;           /* name of this object */
		char           *_stack;          /* stack pointer */
		static Routine *_current;        /* currently scheduled object */
		static Routine *_dead;           /* object to remove */
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
				_stack = (char *)dde_kit_simple_malloc(STACK_SIZE);

				if (verbose)
					PDBG("Start func %s (%p) sp: %p", _name, _func, (_stack + STACK_SIZE));

				/* XXX  move to platform code */

				/* switch stack and call '_func(_arg)' */
				platform_execute((void *)(_stack + STACK_SIZE), (void *)_func, _arg);
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
				dde_kit_simple_free(_stack);
		}

		/**
		 * Schedule next object
		 *
		 * If all is true, each object will be scheduled once.
		 */
		static void schedule(bool all = false) __attribute__((noinline))
		{
			if (!_list()->first())
				return;

			Routine *next = _next(all);

			if (next == _current)
				return;

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
		static void current_use_first() { _current = _list()->first(); }

		/**
		 * Add an object
		 */
		static void add(int (*func)(void *), void *arg, char const *name,
		                bool started = false)
		{
			_list()->insert(new (Genode::env()->heap())
			                Routine(func, arg, name, started));
		}

		/**
		 * Remove this object
		 */
		static void remove()
		{
			if (!_current)
				return;

			_dead = _current;

			schedule();
		}

		/**
		 * True when 'schedule_all' has been called and is still in progress
		 */
		static bool all() { return _all; }
};

#endif /* _ROUTINE_H_ */

