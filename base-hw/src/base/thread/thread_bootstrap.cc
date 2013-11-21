/*
 * \brief  Thread bootstrap code
 * \author Christian Prochaska
 * \date   2013-02-15
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/thread.h>

/* base-hw includes */
#include <kernel/interface.h>


void Genode::Thread_base::_thread_bootstrap()
{
	Native_utcb * const utcb = Thread_base::myself()->utcb();
	_tid.thread_id = utcb->start_info.thread_id();
}
