/*
 * \brief  Connection to OKL4-specific PD service
 * \author Stefan Kalkowski
 * \date   2009-06-22
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__OKL4_PD_SESSION__CONNECTION_H_
#define _INCLUDE__OKL4_PD_SESSION__CONNECTION_H_

#include <okl4_pd_session/client.h>
#include <base/connection.h>

namespace Genode {

	struct Pd_connection : Connection<Pd_session>, Okl4_pd_session_client
	{
		Pd_connection()
		:
			Connection<Pd_session>(session("ram_quota=4K")),
			Okl4_pd_session_client(cap())
		{ }
	};
}

#endif /* _INCLUDE__OKL4_PD_SESSION__CONNECTION_H_ */
