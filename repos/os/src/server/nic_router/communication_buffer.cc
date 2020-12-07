/*
 * \brief  Buffer for network communication
 * \author Martin Stein
 * \date   2020-11-30
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <communication_buffer.h>

using namespace Net;
using namespace Genode;


Communication_buffer::Communication_buffer(Ram_allocator &ram_alloc,
                                           size_t const   size)
:
	_ram_alloc { ram_alloc },
	_ram_ds    { ram_alloc.alloc(size) }
{ }
