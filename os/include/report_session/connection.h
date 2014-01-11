/*
 * \brief  Connection to Report service
 * \author Norman Feske
 * \date   2014-01-10
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__REPORT_SESSION__CONNECTION_H_
#define _INCLUDE__REPORT_SESSION__CONNECTION_H_

#include <report_session/client.h>
#include <base/connection.h>

namespace Report { struct Connection; }


struct Report::Connection : Genode::Connection<Session>, Session_client
{
	Connection(char const *label, size_t buffer_size = 4096)
	:
		Genode::Connection<Session>(
			session("label=\"%s\", ram_quota=%zd, buffer_size=%zd",
			        label, 2*4096 + buffer_size, buffer_size)),
		Session_client(cap())
	{ }
};

#endif /* _INCLUDE__REPORT_SESSION__CONNECTION_H_ */
