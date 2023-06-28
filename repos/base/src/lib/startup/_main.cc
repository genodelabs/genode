/*
 * \brief   Startup code
 * \author  Christian Helmuth
 * \author  Christian Prochaska
 * \author  Norman Feske
 * \date    2006-04-12
 */

/*
 * Copyright (C) 2006-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/thread.h>

/* platform-specific local helper functions */
#include <base/internal/globals.h>
#include <base/internal/crt0.h>

using namespace Genode;


addr_t init_main_thread_result;

static Platform *platform_ptr;


/**
 * Satisfy crt0.s in static programs, LDSO overrides this symbol
 */
extern "C" void init_rtld() __attribute__((weak));
void init_rtld()
{
	/* init cxa guard mechanism before any local static variables are used */
	init_cxx_guard();
}

void * __dso_handle = 0;


/**
 * Lower bound of the stack, solely used for sanity checking
 */
extern unsigned char __initial_stack_base[];


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
	prepare_init_main_thread();

	platform_ptr = &init_platform();

	/*
	 * Create a 'Thread' object for the main thread
	 */
	static constexpr size_t STACK_SIZE = 16*1024;

	struct Main_thread : Thread
	{
		Main_thread()
		:
			Thread(Weight::DEFAULT_WEIGHT, "main", STACK_SIZE, Type::MAIN)
		{ }

		void entry() override { /* never executed */ }
	};

	static Main_thread main_thread { };

	/*
	 * The new stack pointer enables the caller to switch from its current
	 * environment to the those that the thread object provides.
	 */
	addr_t const sp = reinterpret_cast<addr_t>(main_thread.stack_top());
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


namespace Genode {

	/*
	 * To be called from the context of the initial entrypoiny before
	 * passing control to the 'Component::construct' function.
	 */
	void call_global_static_constructors()
	{
		/* Don't do anything if there are no constructors to call */
		addr_t const ctors_size = (addr_t)&_ctors_end - (addr_t)&_ctors_start;
		if (ctors_size == 0)
			return;

		void (**func)();
		for (func = &_ctors_end; func != &_ctors_start; (*--func)());
	}
}


extern "C" int _main() /* executed with the stack within the stack area */
{
	bootstrap_component(*platform_ptr);

	/* never reached */
	return 0;
}
