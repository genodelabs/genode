/*
 * \brief  Kernel-specific thread meta data
 * \author Norman Feske
 * \date   2016-03-11
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__NATIVE_THREAD_H_
#define _INCLUDE__BASE__INTERNAL__NATIVE_THREAD_H_

#include <base/stdint.h>
#include <base/internal/server_socket_pair.h>

namespace Genode { struct Native_thread; }

struct Genode::Native_thread
{
	/*
	 * Unfortunately, both - PID and TID - are needed for lx_tgkill()
	 */
	unsigned int tid = 0;  /* Native thread ID type as returned by the
	                          'clone' system call */
	unsigned int pid = 0;  /* process ID (resp. thread-group ID) */

	bool is_ipc_server = false;

	/**
	 * Natively aligned memory location used in the lock implementation
	 */
	int futex_counter __attribute__((aligned(sizeof(Genode::addr_t)))) = 0;

	struct Meta_data;

	/**
	 * Opaque pointer to additional thread-specific meta data
	 *
	 * This pointer is used by hybrid Linux/Genode programs to maintain
	 * POSIX-thread-related meta data. For non-hybrid Genode programs, it
	 * remains unused.
	 */
	Meta_data *meta_data = nullptr;

	Socket_pair socket_pair { };

	Native_thread() { }
};

#endif /* _INCLUDE__BASE__INTERNAL__NATIVE_THREAD_H_ */
