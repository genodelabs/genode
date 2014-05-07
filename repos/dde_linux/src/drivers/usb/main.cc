/*
 * \brief  Startup USB driver library
 * \author Sebastian Sumpf
 * \date   2013-02-20
 */

#include <os/server.h>

extern void start_usb_driver(Server::Entrypoint &e);

namespace Server {
	char const *name()            { return "usb_drv_ep";        }
	size_t      stack_size()      { return 4*1024*sizeof(long); }
	void construct(Entrypoint &e) { start_usb_driver(e);        }
}
