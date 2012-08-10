/*
 * \brief   Linux platform interface implementation
 * \author  Norman Feske
 * \date    2006-06-13
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/lock.h>

/* local includes */
#include "platform.h"
#include "core_env.h"
#include "server_socket_pair.h"

/* Linux includes */
#include <core_linux_syscalls.h>


using namespace Genode;


static char _some_mem[80*1024*1024];
static Lock _wait_for_exit_lock(Lock::LOCKED);  /* exit() sync */


static void signal_handler(int signum)
{
	_wait_for_exit_lock.unlock();
}


Platform::Platform()
: _ram_alloc(0)
{
	/* catch control-c */
	lx_sigaction(2, signal_handler);

	/* create resource directory under /tmp */
	lx_mkdir(resource_path(), S_IRWXU);

	_ram_alloc.add_range((addr_t)_some_mem, sizeof(_some_mem));
}


void Platform::wait_for_exit()
{
	/* block until exit condition is satisfied */
	try { _wait_for_exit_lock.lock(); }
	catch (Blocking_canceled) { };
}


void Core_parent::exit(int exit_value)
{
	lx_exit_group(exit_value);
}


/*****************************
 ** Support for IPC library **
 *****************************/

namespace Genode {

	Native_connection_state server_socket_pair()
	{
		return create_server_socket_pair(Thread_base::myself()->tid().tid);
	}
}

