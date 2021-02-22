/*
 * \brief  I2C session client implementation
 * \author Jean-Adrien Domage <jean-adrien.domage@gapfruit.com>
 * \date   2021-02-25
 */

/*
 * Copyright (C) 2013-2021 Genode Labs GmbH
 * Copyright (C) 2021 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__I2C_SESSION__I2C_SESSION_H_
#define _INCLUDE__I2C_SESSION__I2C_SESSION_H_

#include <session/session.h>
#include <base/stdint.h>

namespace I2c {
	using namespace Genode;
	struct Session;
}


struct I2c::Session : public Genode::Session
{
	/**
	 * \noapi
	 */
	static char const *service_name() { return "I2c"; }

	enum { CAP_QUOTA = 2 };


	/***************
	 ** Exception **
	 ***************/

	/**
	 * Execption thrown in case of a bus error
	 *
	 * This exception is thrown by the driver incase of a timeout, device missing
	 * acknoledgement and bus arbitration lost. The driver can be configured in the run script
	 * to log descriptive messages incase of errors.
	 */
	class Bus_error : public Exception {};


	/***********************
	 ** Session interface **
	 ***********************/

	/**
	 * Write 8 bits on the bus
	 *
	 * \param byte The 8 bits to be sent
	 *
	 * \throw I2c::Session::Bus_error An error occured while performing an operation on the bus
	 */
	virtual void write_8bits(uint8_t byte) = 0;

	/**
	 * Read 8 bits from the bus
	 *
	 * \throw I2c::Session::Bus_error An error occured while performing an operation on the bus
	 *
	 * \return The 8 received bits
	 */
	virtual uint8_t read_8bits() = 0;

	/**
	 * Write 16 bits on the bus
	 *
	 * \param word The 16 bits to be sent
	 *
	 * \throw I2c::Session::Bus_error An error occured while performing an operation on the bus
	 */
	virtual void write_16bits(uint16_t word) = 0;

	/**
	 * Read 16 bits from the bus
	 *
	 * \throw I2c::Session::Bus_error An error occured while performing an operation on the bus
	 *
	 * \return The 16 received bits
	 */
	virtual uint16_t read_16bits() = 0;

	GENODE_RPC_THROW(Rpc_write_8bits, void, write_8bits,
	                 GENODE_TYPE_LIST(Bus_error),
	                 uint8_t);
	GENODE_RPC_THROW(Rpc_read_8bits, uint8_t, read_8bits,
	                 GENODE_TYPE_LIST(Bus_error));
	GENODE_RPC_THROW(Rpc_write_16bits, void, write_16bits,
	                 GENODE_TYPE_LIST(Bus_error),
	                 uint16_t);
	GENODE_RPC_THROW(Rpc_read_16bits, uint16_t, read_16bits,
	                 GENODE_TYPE_LIST(Bus_error));

	GENODE_RPC_INTERFACE(Rpc_write_8bits, Rpc_read_8bits,
	                     Rpc_write_16bits, Rpc_read_16bits);
};

#endif /* _INCLUDE__I2C_SESSION__I2C_SESSION_H_ */
