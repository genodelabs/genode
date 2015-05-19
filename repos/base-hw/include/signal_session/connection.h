/*
 * \brief  Connection to signal service
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

#ifndef _INCLUDE__SIGNAL_SESSION__CONNECTION_H_
#define _INCLUDE__SIGNAL_SESSION__CONNECTION_H_

#include <signal_session/client.h>
#include <base/connection.h>

namespace Genode { struct Signal_connection; }


struct Genode::Signal_connection : Connection<Signal_session>,
                                   Signal_session_client
{
	Signal_connection()
	: Connection<Signal_session>(session("ram_quota=32K")),
	  Signal_session_client(cap()) { }
};

#endif /* _INCLUDE__CAP_SESSION__CONNECTION_H_ */
