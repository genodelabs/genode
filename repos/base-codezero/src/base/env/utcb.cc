/*
 * \brief  Helper functions UTCB access on Codezero
 * \author Norman Feske
 * \date   2012-03-01
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/thread.h>


/**
 * Resolve 'Thread_base::myself' when not linking the thread library
 *
 * This weak symbol is primarily used by test cases. Most other Genode programs
 * use the thread library. If the thread library is not used, 'myself' can only
 * be called by the main thread, for which 'myself' is defined as zero.
 */
Genode::Thread_base * __attribute__((weak)) Genode::Thread_base::myself() { return 0; }


Genode::Native_utcb *Genode::Thread_base::utcb()
{
	/*
	 * If 'utcb' is called on the object returned by 'myself',
	 * the 'this' pointer may be NULL (if the calling thread is
	 * the main thread). Therefore we handle this special case
	 * here.
	 */
	if (this == 0) return 0;

	return &_context->utcb;
}

