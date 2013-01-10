/**
 * \brief   Startup code for Fiasco/ARM
 * \author  Norman Feske
 * \date    2007-04-30
 *
 * Call constructors for static objects before calling main().
 */

/*
 * Copyright (C) 2007-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

namespace Fiasco {
#include <l4/sys/kdebug.h>
}

/* Genode */
#include <base/crt0.h>
#include <base/env.h>
#include <base/sleep.h>
#include <base/printf.h>

namespace Genode {

	/**
	 * Return constructed parent capability
	 */
	Parent_capability parent_cap()
	{
		Fiasco::l4_threadid_t tid = *(Fiasco::l4_threadid_t *)&_parent_cap_thread_id;
		return Parent_capability(Native_capability(tid, _parent_cap_local_name));
	}
}

using namespace Genode;


/***************
 ** C++ stuff **
 ***************/

/*
 * This symbol must be defined when exception
 * headers are defined in the linker script.
 */
extern "C" __attribute__((weak)) void *__gxx_personality_v0(void)
{
	Fiasco::outstring("What a surprise! This function is really used? Sorry - not implemented\n");
	return 0;
}


/**
 * Resolve symbols needed by libsupc++ to make
 * the linker happy.
 *
 * FIXME: implement us!
 */
extern "C" __attribute__((weak)) int atexit(void) {
	Fiasco::outstring("atexit() called - not implemented!\n");
	return 0;
};
extern "C" __attribute__((weak)) int memcmp(void) {
	Fiasco::outstring("memcmp() called - not implemented!\n");
	return 0;
};
extern "C" __attribute__((weak)) int strncmp(void) {
	Fiasco::outstring("strncmp() called - not implemented!\n");
	return 0;
};


extern int main(int argc, char **argv);

extern void init_exception_handling();  /* implemented in base/cxx */


/* FIXME no support for commandline
 * ask parent for argc and argv */
static char argv0[] = { '_', 'm', 'a', 'i', 'n'};
static char *argv[1] = { argv0 };


/*
 * Define 'environ' pointer that is supposed to be exported by
 * the startup code and relied on by any libC. Because we have no
 * UNIX environment, however, we set this pointer to NULL.
 */
__attribute__((weak)) char **environ = (char **)0;


/**
 * C entry function called by the crt0 startup code
 */
extern "C" int _main()
{
	/* call constructors for static objects */
	void (**func)();
	for (func = &_ctors_end; func != &_ctors_start; (*--func)());

	/* initialize exception handling */
	init_exception_handling();

	/* completely map program image by touching all pages read-only */
	volatile const char *beg, *end;
	beg = (const char *)(((unsigned)&_prog_img_beg) & L4_PAGEMASK);
	end = (const char *)&_prog_img_end;
	for ( ; beg < end; beg += L4_PAGESIZE) (void)(*beg);

	/* call real main function */
	/* XXX no support for commandline */
	int ret = main(1, argv);

	/* inform parent about program exit */
	env()->parent()->exit(ret);

	PDBG("main() returned %d", ret);
	sleep_forever();

	return ret;
}
