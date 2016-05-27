/*
 * \brief  Support for the Linux-specific environment
 * \author Norman Feske
 * \date   2008-12-12
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <util/arg_string.h>
#include <base/thread.h>
#include <linux_dataspace/client.h>
#include <linux_syscalls.h>

/* base-internal includes */
#include <base/internal/platform_env.h>
#include <base/internal/native_thread.h>
#include <base/internal/globals.h>

using namespace Genode;


/****************************************************
 ** Support for Platform_env_base::Rm_session_mmap **
 ****************************************************/

size_t Region_map_mmap::_dataspace_size(Dataspace_capability ds)
{
	if (ds.valid())
		return Dataspace_client(ds).size();

	return Local_capability<Dataspace>::deref(ds)->size();
}


int Region_map_mmap::_dataspace_fd(Dataspace_capability ds)
{
	return Linux_dataspace_client(ds).fd().dst().socket;
}


bool Region_map_mmap::_dataspace_writable(Dataspace_capability ds)
{
	return Dataspace_client(ds).writable();
}



/******************
 ** Local_parent **
 ******************/

Session_capability Local_parent::session(Service_name const &service_name,
                                         Session_args const &args,
                                         Affinity     const &affinity)
{
	if (strcmp(service_name.string(), Rm_session::service_name()) == 0)
	{
		Local_rm_session *session = new (_alloc) Local_rm_session(_alloc);

		return Local_capability<Session>::local_cap(session);
	}

	return Expanding_parent_client::session(service_name, args, affinity);
}


void Local_parent::close(Session_capability session)
{
	/*
	 * Handle non-local capabilities
	 */
	if (session.valid()) {
		Parent_client::close(session);
		return;
	}

	/*
	 * Detect capability to local RM session
	 */
	Capability<Rm_session> rm = static_cap_cast<Rm_session>(session);

	destroy(_alloc, Local_capability<Rm_session>::deref(rm));
}


Local_parent::Local_parent(Parent_capability parent_cap,
                           Emergency_ram_reserve &reserve,
                           Allocator &alloc)
:
	Expanding_parent_client(parent_cap, reserve), _alloc(alloc)
{ }


/******************
 ** Platform_env **
 ******************/

/**
 * List of Unix environment variables, initialized by the startup code
 */
extern char **lx_environ;


/**
 * Read environment variable as long value
 */
static unsigned long get_env_ulong(const char *key)
{
	for (char **curr = lx_environ; curr && *curr; curr++) {

		Arg arg = Arg_string::find_arg(*curr, key);
		if (arg.valid())
			return arg.ulong_value(0);
	}

	return 0;
}


static Parent_capability obtain_parent_cap()
{
	long local_name = get_env_ulong("parent_local_name");

	/* produce typed capability manually */
	typedef Native_capability::Dst Dst;
	Dst const dst(PARENT_SOCKET_HANDLE);
	return reinterpret_cap_cast<Parent>(Native_capability(dst, local_name));
}


Local_parent &Platform_env::_parent()
{
	static Local_parent local_parent(obtain_parent_cap(), *this, _heap);
	return local_parent;
}


Platform_env::Platform_env()
:
	Platform_env_base(static_cap_cast<Ram_session>(_parent().session("Env::ram_session", "")),
	                  static_cap_cast<Cpu_session>(_parent().session("Env::cpu_session", "")),
	                  static_cap_cast<Pd_session> (_parent().session("Env::pd_session",  ""))),
	_heap(Platform_env_base::ram_session(), Platform_env_base::rm_session()),
	_emergency_ram_ds(ram_session()->alloc(_emergency_ram_size()))
{
	/* attach stack area to local address space */
	_local_pd_session._address_space.attach_at(_local_pd_session._stack_area.dataspace(),
	                                           stack_area_virtual_base(),
	                                           stack_area_virtual_size());

	env_stack_area_region_map  = &_local_pd_session._stack_area;
	env_stack_area_ram_session = ram_session();

	/* register TID and PID of the main thread at core */
	Linux_native_cpu_client native_cpu(cpu_session()->native_cpu());
	native_cpu.thread_id(parent()->main_thread_cap(), lx_getpid(), lx_gettid());
}


/*****************************
 ** Support for IPC library **
 *****************************/

namespace Genode {

	Socket_pair server_socket_pair()
	{
		Linux_native_cpu_client native_cpu(env()->cpu_session()->native_cpu());

		Socket_pair socket_pair;

		Thread *thread = Thread::myself();
		if (thread) {
			socket_pair.server_sd = native_cpu.server_sd(thread->cap()).dst().socket;
			socket_pair.client_sd = native_cpu.client_sd(thread->cap()).dst().socket;
			thread->native_thread().socket_pair = socket_pair;
		}
		return socket_pair;
	}

	void destroy_server_socket_pair(Socket_pair socket_pair)
	{
		/* close local file descriptor if it is valid */
		if (socket_pair.server_sd != -1) lx_close(socket_pair.server_sd);
		if (socket_pair.client_sd != -1) lx_close(socket_pair.client_sd);
	}
}
