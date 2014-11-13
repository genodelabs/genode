/*
 * \brief  Startup USB driver library
 * \author Sebastian Sumpf
 * \date   2013-02-20
 */

/*
 * Copyright (C) 2013-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <os/server.h>

extern void start_usb_driver(Server::Entrypoint &e);

namespace Server {
	char const *name()            { return "usb_drv_ep";        }
	size_t      stack_size()      { return 4*1024*sizeof(long); }
	void construct(Entrypoint &e) { start_usb_driver(e);        }
}
