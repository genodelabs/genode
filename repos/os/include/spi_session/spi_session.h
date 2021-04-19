/*
 * \brief  SPI session client implementation
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

#ifndef _INCLUDE__SPI_SESSION__SPI_SESSION_H_
#define _INCLUDE__SPI_SESSION__SPI_SESSION_H_

#include <base/rpc.h>
#include <base/fixed_stdint.h>
#include <dataspace/capability.h>
#include <session/session.h>

namespace Spi {

	using namespace Genode;
	struct Settings;
	class  Session;

	class Io_buffer_too_small : public Exception { };
	class Bus_error : public Exception { };

}

/**
 * Spi settings
 *
 * \details An union is used to make it easier to manipulate settings,
 *          the bitfield struct is intended to be used to change settings,
 *          and a double word is used easily to pass through the RPC interface.
 */
struct Spi::Settings {

	enum {
		STATE_HIGH = 1,
		STATE_LOW  = 0,
	};

	/*
	 * Spi mode are as follow:
	 *  - MODE 0 (value 0): clk line POLARITY: 0 PHASE: 0
	 *  - MODE 1 (value 1): clk line POLARITY: 0 PHASE: 1
	 *  - MODE 2 (value 2): clk line POLARITY: 1 PHASE: 0
	 *  - MODE 3 (value 3): clk line POLARITY: 1 PHASE: 1
	 */
	uint32_t mode:                      2;

	/*
	 * Spi clock idle state control. This control if the clock must
	 * stay HIGH or stay LOW while it is idle
	 */
	uint32_t clock_idle_state:          1;

	/*
	 * Spi data lines state control. This control if the clock must
	 * stay HIGH or stay LOW while it is idle
	 */
	uint32_t data_lines_idle_state:   1;

	/*
	 * Spi slave select line active state, determinate which state has to be
	 * considered the active state.
	 */
	uint32_t ss_line_active_state:      1;
};


class Spi::Session: public Genode::Session
{
public:

	enum { CAP_QUOTA = 7 };

	/**
	 * \return c string containing the service name
	 */
	static const char *service_name() { return "Spi"; }

	/**
	 * Transfer a burst to the endpoint slave device of the current session
	 * \param buffer - buffer containing the data to be transferred
	 * \param buffer_size - size of the buffer
	 * \return byte count that has be read from the slave device
	 *
	 * \details The client uses the buffer to transfer his desired data,
	 * the driver will use that same buffer to write the data read during
	 * the transaction. The data read can not contain more bytes than sent. The
	 * number of bytes read is returned and must be considered as the new size
	 * of the buffer.
	 *
	 * \attention This methode is not thread-safe
	 */
	virtual size_t transfer(char *buffer, size_t buffer_size) = 0;

	/**
	 * \return The session setting
	 */
	virtual Settings settings() = 0;

	/**
	 * \param setting - New setting to be applied to the session
	 */
	virtual void settings(Settings settings) = 0;

	GENODE_RPC_THROW(Rpc_transfer, size_t, spi_transfer,
	                 GENODE_TYPE_LIST(Io_buffer_too_small, Bus_error),
	                 size_t);
	GENODE_RPC(Rpc_get_setting, Settings, settings);
	GENODE_RPC(Rpc_set_setting, void, settings, Settings);
	GENODE_RPC(Rpc_dataspace, Dataspace_capability, io_buffer_dataspace);

	GENODE_RPC_INTERFACE(Rpc_transfer,
	                     Rpc_set_setting,
	                     Rpc_get_setting,
	                     Rpc_dataspace);
};

#endif // _INCLUDE__SPI_SESSION__SPI_SESSION_H_
