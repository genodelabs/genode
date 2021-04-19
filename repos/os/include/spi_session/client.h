/*
 * \brief  SPI rpc client
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

#ifndef _INCLUDE__SPI_CLIENT__CLIENT_H_
#define _INCLUDE__SPI_CLIENT__CLIENT_H_

#include <base/mutex.h>
#include <base/rpc_client.h>
#include <base/attached_dataspace.h>
#include <util/string.h>

#include <spi_session/spi_session.h>

namespace Spi {

	using namespace Genode;

	class Session_client;
}

class Spi::Session_client : public Rpc_client<Session>
{
private:

	Attached_dataspace _io_buffer;

public:

	Session_client(Region_map &rm, Capability<Session> cap)
	:
		Rpc_client<Session> { cap },
		_io_buffer          { rm, call<Rpc_dataspace>() }
	{ }


	/* transfer() is NOT thread-safe */
	size_t transfer(char *buffer, size_t buffer_size) override {
		if (buffer_size > _io_buffer.size()) {
			throw Spi::Io_buffer_too_small();
		}
		memcpy(_io_buffer.local_addr<char>(), buffer, buffer_size);
		size_t const bytes_count = call<Rpc_transfer>(buffer_size);
		memcpy(buffer, _io_buffer.local_addr<char>(), buffer_size);
		return bytes_count;
	}


	void settings(Settings setting) override {
		call<Rpc_set_setting>(setting);
	}


	Settings settings() override {
		return call<Rpc_get_setting>();
	}
};

#endif // _INCLUDE__SPI_CLIENT__CLIENT_H_
