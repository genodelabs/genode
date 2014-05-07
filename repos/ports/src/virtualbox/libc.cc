/*
 * \brief  VirtualBox runtime (RT)
 * \author Norman Feske
 * \date   2013-08-20
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <util/string.h>

/* libc includes */
#include <stdlib.h>
#include <signal.h>

/* libc memory allocator */
#include <libc_mem_alloc.h>

/* VirtualBox includes */
#include <iprt/mem.h>

using Genode::size_t;


/*
 * We cannot use the libc's version of malloc because it does not satisfies
 * the alignment constraints asserted by 'Runtime/r3/alloc.cpp'.
 */

extern "C" void *malloc(size_t size)
{
	return Libc::mem_alloc()->alloc(size, Genode::log2(RTMEM_ALIGNMENT));
}


extern "C" void *calloc(size_t nmemb, size_t size)
{
	void *ret = malloc(nmemb*size);
	Genode::memset(ret, 0, nmemb*size);
	return ret;
}


extern "C" void free(void *ptr)
{
	Libc::mem_alloc()->free(ptr);
}


extern "C" void *realloc(void *ptr, Genode::size_t size)
{
	if (!ptr)
		return malloc(size);

	if (!size) {
		free(ptr);
		return 0;
	}

	/* determine size of old block content (without header) */
	unsigned long old_size = Libc::mem_alloc()->size_at(ptr);

	/* do not reallocate if new size is less than the current size */
	if (size <= old_size)
		return ptr;

	/* allocate new block */
	void *new_addr = malloc(size);

	/* copy content from old block into new block */
	if (new_addr)
		Genode::memcpy(new_addr, ptr, Genode::min(old_size, (unsigned long)size));

	/* free old block */
	free(ptr);

	return new_addr;
}


extern "C" char *getenv(const char *name)
{
	/*
	 * Logging to the pseudo file '/log' is done via the libc plugin provided
	 * by 'logging.cc'.
	 */
	if (Genode::strcmp(name, "VBOX_LOG_DEST") == 0 ||
	    Genode::strcmp(name, "VBOX_RELEASE_LOG_DEST") == 0)
		return (char *)"file=log";

	if (Genode::strcmp(name, "VBOX_LOG") == 0 ||
	    Genode::strcmp(name, "VBOX_RELEASE_LOG") == 0) 
		return (char *)"+rem_dias.e.l.f"
		               "+rem_printf.e.l.f"
//		               "+rem_run.e.l.f"
//		               "+pgm.e.l.f"
		               "+pdm"
//		               "+dev_pic.e.l.f"
//		               "+dev_apic.e.l.f"
		               ;

	if (Genode::strcmp(name, "VBOX_LOG_FLAGS") == 0 ||
	    Genode::strcmp(name, "VBOX_RELEASE_LOG_FLAGS") == 0)
		return (char *)"thread";

	PWRN("getenv called for non-existent variable \"%s\"", name);
	return 0;
}


extern "C" int sigaction(int signum, const struct sigaction *act,
                         struct sigaction *oldact)
{
	/*
	 * Break infinite loop at 'VBox/Runtime/r3/init.cpp' :451
	 */;
	if (oldact)
		oldact->sa_flags = SA_SIGINFO;

	return 0;
}
