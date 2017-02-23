/*
 * \brief  OMAP4 GPIO definitions
 * \author Ivan Loskutov <ivan.loskutov@ksyslabs.org>
 * \author Stefan Kalkowski <stefan.kalkowski@genode-labs.com>
 * \date   2012-06-23
 */

/*
 * Copyright (C) 2012 Ksys Labs LLC
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVERS__GPIO__SPEC__OMAP4__GPIO_H_
#define _DRIVERS__GPIO__SPEC__OMAP4__GPIO_H_

/* Genode includes */
#include <base/attached_io_mem_dataspace.h>
#include <util/mmio.h>

struct Gpio_reg : Genode::Attached_io_mem_dataspace, Genode::Mmio
{
	Gpio_reg(Genode::Env &env,
	         Genode::addr_t const mmio_base,
	         Genode::size_t const mmio_size)
	: Genode::Attached_io_mem_dataspace(env, mmio_base, mmio_size),
	  Genode::Mmio((Genode::addr_t)local_addr<void>()) { }

	struct Oe              : Register_array<0x134, 32, 32, 1> {};
	struct Irqstatus_0     : Register<0x02c, 32> {};
	struct Irqstatus_set_0 : Register<0x034, 32> {};
	struct Irqstatus_clr_0 : Register<0x03c, 32> {};
	struct Ctrl            : Register<0x130, 32> {};
	struct Leveldetect0    : Register_array<0x140, 32, 32, 1> {};
	struct Leveldetect1    : Register_array<0x144, 32, 32, 1> {};
	struct Risingdetect    : Register_array<0x148, 32, 32, 1> {};
	struct Fallingdetect   : Register_array<0x14c, 32, 32, 1> {};
	struct Debounceenable  : Register_array<0x150, 32, 32, 1> {};
	struct Debouncingtime  : Register<0x154, 32>
	{
		struct Time : Bitfield<0, 8> {};
	};
	struct Cleardataout    : Register<0x190, 32> {};
	struct Setdataout      : Register<0x194, 32> {};
	struct Datain          : Register_array<0x138, 32, 32, 1> {};
};

#endif /* _DRIVERS__GPIO__SPEC__OMAP4__GPIO_H_ */
