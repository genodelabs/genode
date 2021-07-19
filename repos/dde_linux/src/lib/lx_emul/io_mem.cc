/*
 * \brief  Lx_emul backend for I/O memory
 * \author Stefan Kalkowski
 * \date   2021-03-10
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_kit/env.h>
#include <lx_emul/io_mem.h>

void * lx_emul_io_mem_map(unsigned long phys_addr,
                          unsigned long size)
{
	using namespace Lx_kit;
	using namespace Genode;

	void * ret = nullptr;
	env().devices.for_each([&] (Device & d) {
		if (d.io_mem(phys_addr, size))
			ret = d.io_mem_local_addr(phys_addr, size);
	});

	if (!ret)
		error("memory-mapped I/O resource ", Hex(phys_addr),
		      " (size=", Hex(size), ") unavailable");
	return ret;
}
