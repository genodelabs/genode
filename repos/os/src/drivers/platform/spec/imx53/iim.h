/*
 * \brief  IC identification module register description
 * \author Stefan Kalkowski <stefan.kalkowski@genode-labs.com>
 * \date   2013-04-29
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVERS__PLATFORM__SPEC__IMX53__IIM_H_
#define _DRIVERS__PLATFORM__SPEC__IMX53__IIM_H_

/* Genode includes */
#include <util/mmio.h>
#include <drivers/defs/imx53.h>
#include <base/attached_io_mem_dataspace.h>

class Iim : public Genode::Attached_io_mem_dataspace,
            Genode::Mmio
{
	private:

		struct Fuse_bank0_gp6 : Register<0x878, 32> {};

	public:

		Iim(Genode::Env &env)
		: Genode::Attached_io_mem_dataspace(env, Imx53::IIM_BASE,
		                                         Imx53::IIM_SIZE),
		Genode::Mmio((Genode::addr_t)local_addr<void>()) {}

		unsigned long revision() { return read<Fuse_bank0_gp6>() & 0xf; }
};

#endif /* _DRIVERS__PLATFORM__SPEC__IMX53__IIM_H_ */
