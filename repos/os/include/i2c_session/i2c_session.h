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
#include <util/array.h>

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

	/**
	 * Exception thrown in case of a bus error
	 *
	 * This exception is thrown by the driver in case of a timeout, device missing
	 * acknowledgement and bus arbitration lost.
	 */
	class Bus_error : public Exception {};

	using Byte_array = Array<uint8_t, 8>;

	/**
	 * A message to an I2C slave is either a read or write of one or more bytes
	 */
	struct Message : Byte_array
	{
		enum Type { READ, WRITE };

		Type type { READ };

		Message() {}

		template<typename ... ARGS>
		Message(Type type, ARGS ... args)
		: Byte_array(args...), type(type) {}
	};

	/**
	 * A transaction to an I2C slave consists of one, or several messages
	 */
	struct Transaction : Array<Message,2>
	{
		using Base = Array<Message,2>;
		using Base::Base;
	};


	/***********************
	 ** Session interface **
	 ***********************/

	/**
	 * Initiate a transaction on the bus
	 *
	 * \param transaction The transaction to be transmitted to the I2c host
	 *
	 * \throw I2c::Session::Bus_error An error occured while performing an operation on the bus
	 */
	virtual void transmit(Transaction & transaction) = 0;

	GENODE_RPC_THROW(Rpc_transmit, void, transmit,
	                 GENODE_TYPE_LIST(Bus_error), Transaction &);

	GENODE_RPC_INTERFACE(Rpc_transmit);
};

#endif /* _INCLUDE__I2C_SESSION__I2C_SESSION_H_ */
