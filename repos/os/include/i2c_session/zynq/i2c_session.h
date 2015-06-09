/*
 * \brief  I2C session interface
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

#ifndef _INCLUDE__I2C_SESSION__I2C_SESSION_H_
#define _INCLUDE__I2C_SESSION__I2C_SESSION_H_

#include <base/signal.h>
#include <session/session.h>

namespace I2C {

	struct Session : Genode::Session
	{
		static const char *service_name() { return "I2C"; }

		virtual ~Session() { }

		/**
		 * Read a single byte from a 16 bit register of the device
		 *
		 * \param adr the address of the device on the bus
		 * \param reg the register to read
		 * \param data the read value
		 *
		 */
		virtual bool read_byte_16bit_reg(Genode::uint8_t adr, Genode::uint16_t reg, Genode::uint8_t *data) = 0;
		
		/**
		 * Write a single data byte to a 16 bit register
		 *
		 * \param adr the address of the device on the bus
		 * \param reg the register to write
		 * \param data the value to write
		 *
		 */
		virtual bool write_16bit_reg(Genode::uint8_t adr, Genode::uint16_t reg,
			Genode::uint8_t data) = 0;


		/*********************
		 ** RPC declaration **
		 *********************/

		GENODE_RPC(Rpc_read_byte_16bit_reg, bool, read_byte_16bit_reg,
			Genode::uint8_t, Genode::uint16_t, Genode::uint8_t*);
		GENODE_RPC(Rpc_write_16bit_reg, bool, write_16bit_reg,
			Genode::uint8_t, Genode::uint16_t, Genode::uint8_t);

		GENODE_RPC_INTERFACE(
			Rpc_read_byte_16bit_reg,
			Rpc_write_16bit_reg
		);
	};
}

#endif /* _INCLUDE__I2C_SESSION__I2C_SESSION_H_ */
