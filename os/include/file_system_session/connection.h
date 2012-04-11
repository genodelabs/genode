/*
 * \brief  Connection to file-system service
 * \author Norman Feske
 * \date   2012-04-05
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__FILE_SYSTEM_SESSION__CONNECTION_H_
#define _INCLUDE__FILE_SYSTEM_SESSION__CONNECTION_H_

#include <file_system_session/client.h>
#include <base/connection.h>
#include <base/allocator.h>

namespace File_system {

	struct Connection : Genode::Connection<Session>, Session_client
	{
		/**
		 * Constructor
		 *
		 * \param tx_buffer_alloc  allocator used for managing the
		 *                         transmission buffer
		 * \param tx_buf_size      size of transmission buffer in bytes
		 */
		Connection(Range_allocator &tx_block_alloc,
		           size_t           tx_buf_size = 128*1024,
		           const char      *label = "")
		:
			Genode::Connection<Session>(
				session("ram_quota=%zd, tx_buf_size=%zd, label=\"%s\"",
				        3*4096 + tx_buf_size, tx_buf_size, label)),
			Session_client(cap(), tx_block_alloc) { }
	};
}

#endif /* _INCLUDE__FILE_SYSTEM_SESSION__CONNECTION_H_ */
