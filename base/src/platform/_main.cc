/*
 * \brief   Startup code
 * \author  Christian Helmuth
 * \author  Christian Prochaska
 * \date    2006-04-12
 *
 * The startup code calls constructors for static objects before calling
 * main(). Furthermore, this file contains the support of exit handlers
 * and destructors.
 *
 * Some code within this file is based on 'atexit.c' of FreeBSD's libc.
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/crt0.h>
#include <base/env.h>
#include <base/sleep.h>
#include <base/printf.h>

/* platform-specific local helper functions */
#include <_main_helper.h>
#include <_main_parent_cap.h>


using namespace Genode;

extern int main(int argc, char **argv, char **envp);
extern void init_exception_handling();  /* implemented in base/cxx */

enum { ATEXIT_SIZE = 256 };


/***************
 ** C++ stuff **
 ***************/

enum Atexit_fn_type { ATEXIT_FN_EMPTY, ATEXIT_FN_STD, ATEXIT_FN_CXA };

struct atexit_fn
{
	Atexit_fn_type fn_type;
	union
	{
		void (*std_func)(void);
		void (*cxa_func)(void *);
	} fn_ptr;		/* function pointer */
	void *fn_arg;	/* argument for CXA callback */
	void *fn_dso;	/* shared module handle */
};

static struct atexit
{
	int index;
	struct atexit_fn fns[ATEXIT_SIZE];
} _atexit;


static Lock *atexit_lock()
{
	static Lock _atexit_lock;
	return &_atexit_lock;
}


static int atexit_register(struct atexit_fn *fn)
{
	atexit_lock()->lock();

	if (_atexit.index >= ATEXIT_SIZE) {
		PERR("Cannot register exit handler - ATEXIT_SIZE reached");
		atexit_lock()->unlock();
		return -1;
	}

	_atexit.fns[_atexit.index++] = *fn;

	atexit_lock()->unlock();
	return 0;
}


/**
 * Register a function to be performed at exit
 */
int genode_atexit(void (*func)(void))
{
	struct atexit_fn fn;
	int error;

	fn.fn_type = ATEXIT_FN_STD;
	fn.fn_ptr.std_func = func;;
	fn.fn_arg = 0;
	fn.fn_dso = 0;

	error = atexit_register(&fn);
	return (error);
}


/**
 * Register a function to be performed at exit or when an shared object
 * with given dso handle is unloaded dynamically.
 *
 * This function is called directly by compiler generated code, so
 * it needs to be declared as extern "C" and cannot be local to
 * the cxx lib.
 */
int genode___cxa_atexit(void (*func)(void*), void *arg, void *dso)
{
	struct atexit_fn fn;
	int error;

	fn.fn_type = ATEXIT_FN_CXA;
	fn.fn_ptr.cxa_func = func;;
	fn.fn_arg = arg;
	fn.fn_dso = dso;

 	error = atexit_register(&fn);
	return (error);
}


/*
 * Call all handlers registered with __cxa_atexit for the shared
 * object owning 'dso'.  Note: if 'dso' is NULL, then all remaining
 * handlers are called.
 */
void genode___cxa_finalize(void *dso)
{
	struct atexit_fn fn;
	int n = 0;

	atexit_lock()->lock();

	for (n = _atexit.index; --n >= 0;) {
		if (_atexit.fns[n].fn_type == ATEXIT_FN_EMPTY)
			continue; /* already been called */
		if (dso != 0 && dso != _atexit.fns[n].fn_dso)
			continue; /* wrong DSO */
		fn = _atexit.fns[n];

		/*
		 * Mark entry to indicate that this particular handler
		 * has already been called.
		 */
		_atexit.fns[n].fn_type = ATEXIT_FN_EMPTY;
		atexit_lock()->unlock();

		/* call the function of correct type */
		if (fn.fn_type == ATEXIT_FN_CXA)
			fn.fn_ptr.cxa_func(fn.fn_arg);
		else if (fn.fn_type == ATEXIT_FN_STD)
			fn.fn_ptr.std_func();
		atexit_lock()->lock();
	}

	atexit_lock()->unlock();
}


/**
 * Terminate the process.
 */
void genode_exit(int status)
{
	/* inform parent about the exit status */
	env()->parent()->exit(status);

	/*
	 * Call destructors for static objects.
	 *
	 * It happened that a function from the dtors list (namely
	 * __clean_env_destructor() from the libc) called another function, which
	 * depended on the Genode environment. Since the Genode environment gets
	 * destroyed by genode___cxa_finalize(), the functions from the dtors list
	 * are called before genode___cxa_finalize().
	 *
	 */
	void (**func)();
	for (func = &_dtors_start; func != &_dtors_end; (*func++)());

	/* call all handlers registered with atexit() or __cxa_atexit() */
	genode___cxa_finalize(0);

	/*
	 * Wait for destruction by the parent who was supposed to be notified by 
	 * the destructor of the static Genode::Env instance.
	 */
	sleep_forever();
}


/**
 * Dummy default arguments for main function
 */
static char  argv0[] = { '_', 'm', 'a', 'i', 'n', 0};
static char *argv[1] = { argv0 };


/**
 * Arguments for main function
 *
 * These global variables may be initialized by a constructor provided by an
 * external library.
 */
char **genode_argv = argv;
int    genode_argc = 1;
char **genode_envp = 0;


/**
 * C entry function called by the crt0 startup code
 */
extern "C" int _main()
{
	main_thread_bootstrap();

	/* initialize exception handling */
	init_exception_handling();

	/*
	 * Trigger first exception. This step has two purposes.
	 * First, it enables us to detect problems related to exception handling as
	 * early as possible. If there are problems with the C++ support library,
	 * it is much easier to debug them at this early stage. Otherwise problems
	 * with half-working exception handling cause subtle failures that are hard
	 * to interpret.
	 *
	 * Second, the C++ support library allocates data structures lazily on the
	 * first occurrence of an exception. This allocation traverses into
	 * Genode's heap and, in some corner cases, consumes several KB of stack.
	 * This is usually not a problem when the first exception is triggered from
	 * the main thread but it becomes an issue when the first exception is
	 * thrown from the context of a thread with a specially tailored (and
	 * otherwise sufficient) stack size. By throwing an exception here, we
	 * mitigate this issue by eagerly performing those allocations.
	 */
	try { throw 1; } catch (...) { }

	/* call constructors for static objects */
	void (**func)();
	for (func = &_ctors_end; func != &_ctors_start; (*--func)());

	/* now, it is save to call printf */

	/* call real main function */
	int ret = main(genode_argc, genode_argv, genode_envp);

	genode_exit(ret);

	/* not reached */
	return ret;
}
