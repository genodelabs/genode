/*
 * \brief  Connection to file-system service
 * \author Norman Feske
 * \date   2012-04-05
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
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

	struct Connection;

	/* recommended packet transmission buffer size */
	enum { DEFAULT_TX_BUF_SIZE = 128*1024 };

}


struct File_system::Connection : Genode::Connection<Session>, Session_client
{
	/**
	 * Constructor
	 *
	 * \param tx_buffer_alloc  allocator used for managing the
	 *                         transmission buffer
	 * \param tx_buf_size      size of transmission buffer in bytes
	 */
	Connection(Genode::Range_allocator &tx_block_alloc,
	           size_t                   tx_buf_size = DEFAULT_TX_BUF_SIZE,
	           char const              *label       = "",
	           char const              *root        = "/",
	           bool                     writeable   = true)
	:
		Genode::Connection<Session>(
			session("ram_quota=%zd, "
			        "tx_buf_size=%zd, "
			        "label=\"%s\", "
			        "root=\"%s\", "
			        "writeable=%d",
			        4*1024*sizeof(long) + tx_buf_size,
			        tx_buf_size,
			        label, root, writeable)),
		Session_client(cap(), tx_block_alloc)
	{ }
};

#endif /* _INCLUDE__FILE_SYSTEM_SESSION__CONNECTION_H_ */
