/*
 * \brief  Native types
 * \author Norman Feske
 * \date   2007-10-15
 */

/*
 * Copyright (C) 2007-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__NATIVE_TYPES_H_
#define _INCLUDE__BASE__NATIVE_TYPES_H_

#include <util/string.h>
#include <base/native_capability.h>
#include <base/stdint.h>

namespace Genode {

	/**
	 * Native thread contains more thread-local data than just the ID
	 */
	struct Native_thread
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

		Native_thread() { }
	};

	struct Cap_dst_policy
	{
		struct Dst
		{
			int socket;

			/**
			 * Default constructor creates invalid destination
			 */
			Dst() : socket(-1) { }

			explicit Dst(int socket) : socket(socket) { }
		};

		static bool valid(Dst id) { return id.socket != -1; }
		static Dst  invalid()     { return Dst(); }
		static void copy(void* dst, Native_capability_tpl<Cap_dst_policy>* src);
	};

	typedef Native_capability_tpl<Cap_dst_policy> Native_capability;

	/**
	 * The connection state is the socket handle of the RPC entrypoint
	 */
	struct Native_connection_state
	{
		int server_sd;
		int client_sd;

		Native_connection_state() : server_sd(-1), client_sd(-1) { }
	};

	enum { PARENT_SOCKET_HANDLE = 100 };
}

#endif /* _INCLUDE__BASE__NATIVE_TYPES_H_ */
