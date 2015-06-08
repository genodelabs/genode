/*
 * \brief  Zynq Gpio session interface
 * \author Mark Albers
 * \date   2015-04-02
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__GPIO_SESSION__GPIO_SESSION_H_
#define _INCLUDE__GPIO_SESSION__GPIO_SESSION_H_

#include <base/signal.h>
#include <session/session.h>

namespace Gpio {

	struct Session : Genode::Session
	{
		static const char *service_name() { return "Gpio"; }

		virtual ~Session() { }

		/**
         * Write
		 *
         * \param data
		 */
        virtual bool write(Genode::uint32_t data, bool isChannel2) = 0;

		/**
         * Read
		 *
         * \return data
		 */
        virtual Genode::uint32_t read(bool isChannel2) = 0;


		/*******************
		 ** RPC interface **
		 *******************/

        GENODE_RPC(Rpc_write, bool, write, Genode::uint32_t, bool);
        GENODE_RPC(Rpc_read, Genode::uint32_t, read, bool);

        GENODE_RPC_INTERFACE(Rpc_write, Rpc_read);
	};
}

#endif /* _INCLUDE__GPIO_SESSION__GPIO_SESSION_H_ */
