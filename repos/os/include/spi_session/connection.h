/*
 * \brief  SPI connection
 * \author Jean-Adrien Domage <jean-adrien.domage@gapfruit.com>
 * \date   2021-04-28
 */

/*
 * Copyright (C) 2013-2021 Genode Labs GmbH
 * Copyright (C) 2021 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPI_SESSION__CONNECTION_H_
#define _INCLUDE__SPI_SESSION__CONNECTION_H_

#include <base/connection.h>
#include <spi_session/client.h>

namespace Spi { struct Connection; }

struct Spi::Connection : Genode::Connection<Session>, Session_client
{

	enum {
		RAM_QUOTA = 32 * 1024 * sizeof(long),
		DEFAULT_IO_BUFFER_SIZE = 8 * 1024,
	};

	explicit Connection(Genode::Env& env,  size_t io_buffer_size = DEFAULT_IO_BUFFER_SIZE, const char* label = "")
	:
		Genode::Connection<Session>(env,
		                            session(env.parent(),
		                                    "ram_quota=%ld, cap_quota=%ld, label=\"%s\", io_buffer_size=%ld",
		                                    RAM_QUOTA + io_buffer_size, Session::CAP_QUOTA, label, io_buffer_size)),
		Session_client(env.rm(), cap())
	{ }

};

#endif // _INCLUDE__SPI_SESSION__CONNECTION_H_
