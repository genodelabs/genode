/*
 * \brief  I2C session connection
 * \author Jean-Adrien Domage <jean-adrien.domage@gapfruit.com>
 * \date   2021-02-26
 */

/*
 * Copyright (C) 2013-2021 Genode Labs GmbH
 * Copyright (C) 2021 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__I2C_SESSION__CONNECTION_H_
#define _INCLUDE__I2C_SESSION__CONNECTION_H_

#include <i2c_session/client.h>
#include <base/connection.h>

namespace I2c { struct Connection; }


struct I2c::Connection : Genode::Connection<I2c::Session>, I2c::Session_client
{
	Connection(Genode::Env &env, char const *label = "")
	:
		Genode::Connection<Session>(env, session(env.parent(), "ram_quota=8K, label=%s", label)),
		Session_client(cap())
	{ }

	void write_8bits(uint8_t byte)
	{
		Transaction t { Message(Message::WRITE, byte) };
		transmit(t);
	}

	uint8_t read_8bits()
	{
		Transaction t { Message(Message::READ, 0) };
		transmit(t);
		return t.value(0).value(0);
	}

	void write_16bits(uint16_t word)
	{
		Transaction t { Message(Message::WRITE, word & 0xff, word >> 8) };
		transmit(t);
	}

	uint16_t read_16bits()
	{
		Transaction t { Message(Message::READ, 0, 0) };
		transmit(t);
		return t.value(0).value(0) | (t.value(0).value(1) << 8);
	}
};

#endif /* _INCLUDE__I2C_SESSION__CONNECTION_H_ */
