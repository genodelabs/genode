/*
 * \brief  Connection to Loader service
 * \author Christian Prochaska
 * \author Norman Feske
 * \date   2009-10-05
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__LOADER_SESSION__CONNECTION_H_
#define _INCLUDE__LOADER_SESSION__CONNECTION_H_

#include <loader_session/client.h>
#include <base/connection.h>

namespace Loader { struct Connection; }


struct Loader::Connection : Genode::Connection<Session>, Session_client
{
	/**
	 * Constructor
	 */
	Connection(Genode::Env &env, Ram_quota ram_quota, Cap_quota cap_quota)
	:
		Genode::Connection<Session>(env, session(env.parent(),
		                                         "ram_quota=%ld, cap_quota=%ld",
		                                         ram_quota.value,
		                                         CAP_QUOTA + cap_quota.value)),
		Session_client(cap())
	{ }
};

#endif /* _INCLUDE__LOADER_SESSION__CONNECTION_H_ */
