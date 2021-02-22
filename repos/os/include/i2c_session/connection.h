/*
 * \brief  I2c session connection
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

	Connection(Genode::Env &env, const char* label = "")
	:
		Genode::Connection<Session>(env, session(env.parent(), "ram_quota=8K, label=%s", label)),
		Session_client(cap())
	{}

};

#endif /* _INCLUDE__I2C_SESSION__CONNECTION_H_ */
