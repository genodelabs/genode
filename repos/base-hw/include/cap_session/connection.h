/*
 * \brief  Connection to CAP service
 * \author Stefan Kalkowski
 * \author Norman Feske
 * \date   2015-05-20
 *
 * This is a shadow copy of the generic header in base,
 * due to higher memory donation requirements in base-hw
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__CAP_SESSION__CONNECTION_H_
#define _INCLUDE__CAP_SESSION__CONNECTION_H_

#include <cap_session/client.h>
#include <base/connection.h>

namespace Genode { struct Cap_connection; }


struct Genode::Cap_connection : Connection<Cap_session>, Cap_session_client
{
	Cap_connection() : Connection<Cap_session>(session("ram_quota=8K")),
	                   Cap_session_client(cap()) { }
};

#endif /* _INCLUDE__CAP_SESSION__CONNECTION_H_ */
