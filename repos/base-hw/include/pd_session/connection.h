/*
 * \brief  Connection to PD service
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

#ifndef _INCLUDE__PD_SESSION__CONNECTION_H_
#define _INCLUDE__PD_SESSION__CONNECTION_H_

#include <pd_session/client.h>
#include <base/connection.h>

namespace Genode { struct Pd_connection; }


struct Genode::Pd_connection : Connection<Pd_session>, Pd_session_client
{
	enum { RAM_QUOTA = 20*1024 };

	/**
	 * Constructor
	 *
	 * \param label  session label
	 */
	Pd_connection(char const *label = "", Native_pd_args const *pd_args = 0)
	: Connection<Pd_session>(session("ram_quota=%u, label=\"%s\"",
	                                 RAM_QUOTA, label)),
	  Pd_session_client(cap()) { }
};

#endif /* _INCLUDE__PD_SESSION__CONNECTION_H_ */
