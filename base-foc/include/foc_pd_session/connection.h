/*
 * \brief  Connection to Fiasco.OC specific PD service
 * \author Stefan Kalkowski
 * \date   2011-04-14
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__FOC_PD_SESSION__CONNECTION_H_
#define _INCLUDE__FOC_PD_SESSION__CONNECTION_H_

#include <foc_pd_session/client.h>
#include <base/connection.h>

namespace Genode {

	struct Foc_pd_connection : Connection<Foc_pd_session>, Foc_pd_session_client
	{
		/**
		 * Constructor
		 *
		 * \param args  additional session arguments
		 */
		Foc_pd_connection(const char *args = 0)
		:
			Connection<Foc_pd_session>(
				session("ram_quota=4K%s%s",
				        args ? ", " : "",
				        args ? args : "")),
			Foc_pd_session_client(cap())
		{ }
	};
}

#endif /* _INCLUDE__FOC_PD_SESSION__CONNECTION_H_ */
