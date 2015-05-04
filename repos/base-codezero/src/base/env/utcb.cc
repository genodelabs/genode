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

