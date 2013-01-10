/*
 * \brief  Thread-context specific part of the thread library
 * \author Norman Feske
 * \date   2010-01-19
 *
 * This part of the thread library is required by the IPC framework
 * also if no threads are used.
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/thread.h>

using namespace Genode;


Native_utcb *main_thread_utcb();


Native_utcb *Thread_base::utcb()
{
	/*
	 * If 'utcb' is called on the object returned by 'myself',
	 * the 'this' pointer may be NULL (if the calling thread is
	 * the main thread). Therefore we allow this special case
	 * here.
	 */
	if (this == 0) return main_thread_utcb();

	return &_context->utcb;
}


