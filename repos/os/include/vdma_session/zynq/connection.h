/*
 * \brief  Connection to Zynq VDMA session
 * \author Mark Albers
 * \date   2015-04-13
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__VDMA_SESSION__CONNECTION_H_
#define _INCLUDE__VDMA_SESSION__CONNECTION_H_

#include <vdma_session/zynq/client.h>
#include <base/connection.h>

namespace Vdma {

	struct Connection : Genode::Connection<Session>, Session_client
	{
        Connection(unsigned vdma_number)
        : Genode::Connection<Session>(session("ram_quota=8K, vdma=%zd", vdma_number)),
		  Session_client(cap()) { }
	};
}

#endif /* _INCLUDE__VDMA_SESSION__CONNECTION_H_ */
