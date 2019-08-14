/*
 * \brief  Session interface for fetching the content of cloned libc process
 * \author Norman Feske
 * \date   2019-08-14
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CLONE_SESSION_H_
#define _CLONE_SESSION_H_

/* Genode includes */
#include <base/rpc_server.h>
#include <base/connection.h>
#include <base/attached_dataspace.h>
#include <util/misc_math.h>

/* libc includes */
#include <string.h>

namespace Libc {
	struct Clone_session;
	struct Clone_connection;
}


struct Libc::Clone_session : Genode::Session
{
	static const char *service_name() { return "Clone"; }

	enum { BUFFER_SIZE = 512*1024UL,
	       RAM_QUOTA   = BUFFER_SIZE + 4096,
	       CAP_QUOTA   = 2 };

	struct Memory_range
	{
		void *start;
		size_t size;
	};

	GENODE_RPC(Rpc_dataspace, Genode::Dataspace_capability, dataspace);
	GENODE_RPC(Rpc_memory_content, void, memory_content, Memory_range);

	GENODE_RPC_INTERFACE(Rpc_dataspace, Rpc_memory_content);
};


struct Libc::Clone_connection : Genode::Connection<Clone_session>,
                                Genode::Rpc_client<Clone_session>
{
	Genode::Attached_dataspace const _buffer;

	Clone_connection(Genode::Env &env)
	:
		Genode::Connection<Clone_session>(env,
		                                  session(env.parent(),
		                                          "ram_quota=%ld, cap_quota=%ld",
		                                          RAM_QUOTA, CAP_QUOTA)),
		Genode::Rpc_client<Clone_session>(cap()),
		_buffer(env.rm(), call<Rpc_dataspace>())
	{ }

	/**
	 * Obtain memory content from cloned address space
	 */
	void memory_content(void *dst, size_t const len)
	{
		size_t remaining = len;
		char  *ptr       = (char *)dst;

		while (remaining > 0) {

			size_t const chunk_len = Genode::min((size_t)BUFFER_SIZE, remaining);

			/* instruct server to fill shared buffer */
			call<Rpc_memory_content>(Memory_range{ ptr, chunk_len });

			/* copy-out data from shared buffer to local address space */
			::memcpy(ptr, _buffer.local_addr<char>(), chunk_len);

			remaining -= chunk_len;
			ptr       += chunk_len;
		}
	}

	template <typename OBJ>
	void object_content(OBJ &obj) { memory_content(&obj, sizeof(obj)); }
};

#endif /* _CLONE_SESSION_H_ */
