/*
 * \brief  Setup the thread environment of a programs first thread
 * \author Christian Helmuth
 * \author Christian Prochaska
 * \author Martin Stein
 * \date   2013-12-04
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <util/construct_at.h>
#include <base/env.h>
#include <base/thread.h>

/* base-internal includes */
#include <base/internal/globals.h>

using namespace Genode;


addr_t init_main_thread_result;

extern void init_exception_handling();

namespace Genode { extern Region_map * env_stack_area_region_map; }

void prepare_init_main_thread();

enum { MAIN_THREAD_STACK_SIZE = 16UL * 1024 * sizeof(Genode::addr_t) };

/**
 * Satisfy crt0.s in static programs, LDSO overrides this symbol
 */
extern "C" void init_rtld() __attribute__((weak));
void init_rtld() { }

/**
 * The first thread in a program
 */
class Main_thread : public Thread_deprecated<MAIN_THREAD_STACK_SIZE>
{
	public:

		/**
		 * Constructor
		 *
		 * \param reinit  wether this is called for reinitialization
		 */
		Main_thread(bool reinit)
		:
			Thread_deprecated("main", reinit ? REINITIALIZED_MAIN : MAIN)
		{ }

		/**********************
		 ** Thread interface **
		 **********************/

		void entry() { }
};


Main_thread * main_thread()
{
	static Main_thread s(false);
	return &s;
}


/**
 * Create a thread object for the main thread
 *
 * \return  stack pointer of the new environment via init_main_thread_result
 *
 * This function must be called only once per program and before the _main
 * function. It can be called as soon as a temporary environment provides
 * some stack space and inter-process communication. At this stage, global
 * static objects are not registered for implicit destruction at program exit.
 */
extern "C" void init_main_thread()
{
	/* do platform specific preparation */
	prepare_init_main_thread();

	/*
	 * Explicitly setup program environment at this point to ensure that its
	 * destructor won't be registered for the atexit routine.
	 */
	(void*)env();
	init_log();

	/* create a thread object for the main thread */
	main_thread();

	/**
	 * The new stack pointer enables the caller to switch from its current
	 * environment to the those that the thread object provides.
	 */
	addr_t sp = reinterpret_cast<addr_t>(main_thread()->stack_top());
	init_main_thread_result = sp;
}


/**
 * Reinitialize main-thread object according to a reinitialized environment
 */
void reinit_main_thread() { construct_at<Main_thread>(main_thread(), true); }
