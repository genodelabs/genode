/*
 * \brief  Platform support specific to ARM
 * \author Norman Feske
 * \date   2007-10-13
 */

/*
 * Copyright (C) 2007-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include "platform.h"

using namespace Genode;

void Platform::_setup_io_port_alloc()
{
	/*
	 * This is just a dummy init function for the I/O port allocator.
	 * ARM does not I/O port support.
	 */
}
