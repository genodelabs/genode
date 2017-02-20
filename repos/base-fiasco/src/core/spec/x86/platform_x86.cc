/*
 * \brief  Platform support specific to x86
 * \author Christian Helmuth
 * \date    2006-04-11
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/internal/fiasco_thread_helper.h>

#include "platform.h"
#include "util.h"

namespace Fiasco {
#include <l4/sys/ipc.h>
}

using namespace Genode;
using namespace Fiasco;

void Platform::_setup_io_port_alloc()
{
	l4_fpage_t fp;
	l4_umword_t dummy;
	l4_msgdope_t result;
	l4_msgtag_t tag;

	/* get all I/O ports at once */
	int error = l4_ipc_call_tag(Fiasco::sigma0_threadid,
	                            L4_IPC_SHORT_MSG,
	                              l4_iofpage(0, L4_WHOLE_IOADDRESS_SPACE, 0).fpage, 0,
	                            l4_msgtag(L4_MSGTAG_IO_PAGE_FAULT, 0, 0, 0),
	                            L4_IPC_IOMAPMSG(0, L4_WHOLE_IOADDRESS_SPACE),
	                              &dummy, &fp.fpage,
	                            L4_IPC_NEVER, &result, &tag);

	if (error ||
	    !(l4_ipc_fpage_received(result)                /* got something */
	    && fp.iofp.f == 0xf                            /* got IO ports */
	    && fp.iofp.iosize == L4_WHOLE_IOADDRESS_SPACE
	    && fp.iofp.iopage == 0))                       /* got whole IO space */

	panic("Received no I/O ports from sigma0");

	/* setup allocator */
	_io_port_alloc.add_range(0, 0x10000);
}
