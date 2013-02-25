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

/* Codezero includes */
#include <codezero/syscalls.h>


void Genode::Thread_base::_thread_bootstrap()
{
	Codezero::l4_mutex_init(utcb()->running_lock());
	Codezero::l4_mutex_lock(utcb()->running_lock()); /* block on first mutex lock */
}
