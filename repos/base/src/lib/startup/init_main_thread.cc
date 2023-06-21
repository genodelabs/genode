/*
 * \brief  Setup the thread environment of a programs first thread
 * \author Christian Helmuth
 * \author Christian Prochaska
 * \author Martin Stein
 * \date   2013-12-04
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/construct_at.h>
#include <base/env.h>
#include <base/thread.h>

/* base-internal includes */
#include <base/internal/globals.h>

using namespace Genode;


addr_t init_main_thread_result;

namespace Genode { extern Region_map * env_stack_area_region_map; }

void prepare_init_main_thread();

enum { MAIN_THREAD_STACK_SIZE = 16*1024 };


/**
 * Satisfy crt0.s in static programs, LDSO overrides this symbol
 */
extern "C" void init_rtld() __attribute__((weak));
void init_rtld()
{
	/* init cxa guard mechanism before any local static variables are used */
	init_cxx_guard();
}


/**
 * Lower bound of the stack, solely used for sanity checking
 */
extern unsigned char __initial_stack_base[];


/**
 * The first thread in a program
 */
struct Main_thread : Thread
{
	Main_thread()
	:
		Thread(Weight::DEFAULT_WEIGHT, "main", MAIN_THREAD_STACK_SIZE, Type::MAIN)
	{ }

	/**********************
	 ** Thread interface **
	 **********************/

	void entry() override { }
};


Main_thread * main_thread()
{
	static Main_thread s { };
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

	init_platform();

	/* create a thread object for the main thread */
	main_thread();

	/**
	 * The new stack pointer enables the caller to switch from its current
	 * environment to the those that the thread object provides.
	 */
	addr_t const sp = reinterpret_cast<addr_t>(main_thread()->stack_top());
	init_main_thread_result = sp;

	/*
	 * Sanity check for the usage of the initial stack
	 *
	 * Because the initial stack is located in the BSS, it is zero-initialized.
	 * We check that the stack still contains zeros at its lower boundary after
	 * executing all the initialization code.
	 */
	enum { STACK_PAD = 256U };
	for (unsigned i = 0; i < STACK_PAD; i++) {
		if (__initial_stack_base[i] == 0)
			continue;

		error("initial stack overflow detected");
		for (;;);
	}
}
