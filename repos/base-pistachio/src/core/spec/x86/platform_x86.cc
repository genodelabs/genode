/*
 * \brief  Platform support specific to x86
 * \author Christian Helmuth
 * \date   2006-04-11
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core-local includes */
#include <kip.h>

#include "platform.h"
#include "util.h"

namespace Pistachio {
#include <l4/sigma0.h>
#include <l4/arch.h>
}

using namespace Genode;

void Platform::_setup_io_port_alloc()
{
	/* setup allocator */
	enum { IO_PORT_RANGE_SIZE = 0x10000 };
	_io_port_alloc.add_range(0, IO_PORT_RANGE_SIZE);
}
