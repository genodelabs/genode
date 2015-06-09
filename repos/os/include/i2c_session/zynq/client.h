/*
 * \brief  Client-side i2c interface
 * \author Mark Albers
 * \date   2015-04-13
 * \author Alexander Tarasikov <alexander.tarasikov@gmail.com>
 * \date   2012-09-18
 */

/*
 * Copyright (C) 2012 Ksys Labs LLC
 * Copyright (C) 2012-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__I2C_SESSION__CLIENT_H_
#define _INCLUDE__I2C_SESSION__CLIENT_H_

#include <i2c_session/zynq/capability.h>
#include <base/rpc_client.h>

namespace I2C {

	struct Session_client : Genode::Rpc_client<Session>
	{
		explicit Session_client(Session_capability session)
		: Genode::Rpc_client<Session>(session) { }

		bool read_byte_16bit_reg(Genode::uint8_t adr, Genode::uint16_t reg, Genode::uint8_t *data)
		{
			return call<Rpc_read_byte_16bit_reg>(adr, reg, data);
		}

		bool write_16bit_reg(Genode::uint8_t adr, Genode::uint16_t reg,
			Genode::uint8_t data)
		{
			return call<Rpc_write_16bit_reg>(adr, reg, data);
		}
	};
}

#endif /* _INCLUDE__I2C_SESSION__CLIENT_H_ */
