/*
 * \brief  Lx_emul backend for I/O memory
 * \author Stefan Kalkowski
 * \date   2021-03-10
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2 or later.
 */

#include <lx_kit/env.h>
#include <lx_emul/io_mem.h>

void * lx_emul_io_mem_map(unsigned long phys_addr, unsigned long size, int wc)
{
	auto wc_uc = [] (bool wc) { return wc ? "write-combined" : "uncached"; };

	using namespace Lx_kit;
	using namespace Genode;

	void * ret = nullptr;
	env().devices.for_each([&] (Device &d) {
		d.for_each_io_mem([&] (Device::Io_mem &io) {
			if (!io.match(phys_addr, size)) return;

			ret = d.io_mem_local_addr(phys_addr, size);

			if (io.wc != !!wc)
				warning("can't map ", wc_uc(io.wc), " IOMEM ",
				        (void *)phys_addr, "-", (void *)(phys_addr + size - 1),
				        " ", wc_uc(wc));
		});
	});

	if (!ret)
		error("memory-mapped I/O resource ", Hex(phys_addr),
		      " (size=", Hex(size), ") unavailable");
	return ret;
}
