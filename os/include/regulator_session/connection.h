/*
 * \brief  Connection to regulator service
 * \author Alexander Tarasikov <tarasikov@ksyslabs.org>
 * \date   2012-02-15
 */

/*
 * Copyright (C) 2012 Ksys Labs LLC
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__REGULATOR_SESSION__CONNECTION_H_
#define _INCLUDE__REGULATOR_SESSION__CONNECTION_H_

#include <regulator_session/client.h>
#include <base/connection.h>

namespace Regulator {

	class Connection : public Genode::Connection<Session>,
	                   public Session_client
	{
		public:
			Connection(const char *label="")
			:
				Genode::Connection<Session>(
					session("ram_quota=4K, label=\"%s\"", label)
				),
				Session_client(cap())
			{ }
	};
}

#endif /* _INCLUDE__REGULATOR_SESSION__CONNECTION_H_ */
