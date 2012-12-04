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
#include <linux_dataspace/client.h>

/* local includes */
#include "platform.h"
#include "core_env.h"
#include "server_socket_pair.h"

/* Linux includes */
#include <core_linux_syscalls.h>


using namespace Genode;


/**
 * Memory pool used for for core-local meta data
 */
static char _core_mem[80*1024*1024];
static Lock _wait_for_exit_lock(Lock::LOCKED);  /* exit() sync */


static void signal_handler(int signum)
{
	_wait_for_exit_lock.unlock();
}


Platform::Platform()
: _core_mem_alloc(0)
{
	/* catch control-c */
	lx_sigaction(2, signal_handler);

	/* create resource directory under /tmp */
	lx_mkdir(resource_path(), S_IRWXU);

	_core_mem_alloc.add_range((addr_t)_core_mem, sizeof(_core_mem));

	/*
	 * Occupy the socket handle that will be used to propagate the parent
	 * capability new processes. Otherwise, there may be the chance that the
	 * parent capability as supplied by the process creator will be assigned to
	 * this handle, which would result in a 'dup2' syscall taking
	 * PARENT_SOCKET_HANDLE as both source and target descriptor.
	 */
	lx_dup2(0, PARENT_SOCKET_HANDLE);
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

	void destroy_server_socket_pair(Native_connection_state const &ncs)
	{
		/*
		 * As entrypoints in core are never destructed, this function is only
		 * called on IPC-client destruction. In this case, it's a no-op in core
		 * as well as in Genode processes.
		 */
		if (ncs.server_sd != -1 || ncs.client_sd != -1)
			PERR("%s called for IPC server which should never happen", __func__);
	}
}


/****************************************************
 ** Support for Platform_env_base::Rm_session_mmap **
 ****************************************************/

Genode::size_t
Platform_env_base::Rm_session_mmap::_dataspace_size(Capability<Dataspace> ds_cap)
{
	if (!ds_cap.valid())
		return Dataspace_capability::deref(ds_cap)->size();

	/* use RPC if called from a different thread */
	if (!core_env()->entrypoint()->is_myself())
		return Dataspace_client(ds_cap).size();

	/* use local function call if called from the entrypoint */
	Dataspace *ds = core_env()->entrypoint()->lookup(ds_cap);
	return ds ? ds->size() : 0;
}


int Platform_env_base::Rm_session_mmap::_dataspace_fd(Capability<Dataspace> ds_cap)
{
	if (!core_env()->entrypoint()->is_myself())
		return Linux_dataspace_client(ds_cap).fd().dst().socket;

	Capability<Linux_dataspace> lx_ds_cap = static_cap_cast<Linux_dataspace>(ds_cap);

	Linux_dataspace *ds = core_env()->entrypoint()->lookup(lx_ds_cap);

	return ds ? ds->fd().dst().socket : -1;
}


bool Platform_env_base::Rm_session_mmap::_dataspace_writable(Dataspace_capability ds_cap)
{
	if (!core_env()->entrypoint()->is_myself())
		return Dataspace_client(ds_cap).writable();

	Dataspace *ds = core_env()->entrypoint()->lookup(ds_cap);

	return ds ? ds->writable() : false;
}
