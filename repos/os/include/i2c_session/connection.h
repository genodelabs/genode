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
	Connection(Genode::Env &env, Label const &label = Label())
	:
		Genode::Connection<Session>(env, label, Ram_quota { 8*1024 }, Args()),
		Session_client(cap())
	{ }

	void write_8bits(uint8_t byte)
	{
		Transaction t { Message(Message::WRITE, byte) };
		transmit(t);
	}

	uint8_t read_8bits()
	{
		Transaction t { Message(Message::READ, (uint8_t)0) };
		transmit(t);
		return t.value(0).value(0);
	}

	void write_16bits(uint16_t word)
	{
		Transaction t { Message(Message::WRITE, (uint8_t)(word & 0xff),
		                                        (uint8_t)(word >> 8)) };
		transmit(t);
	}

	uint16_t read_16bits()
	{
		Transaction t { Message(Message::READ, (uint8_t)0, (uint8_t)0) };
		transmit(t);
		return (uint16_t)(t.value(0).value(0) | (t.value(0).value(1) << 8));
	}
};

#endif /* _INCLUDE__I2C_SESSION__CONNECTION_H_ */
