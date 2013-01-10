/*
 * \brief  OMAP4 GPIO definitions
 * \author Ivan Loskutov <ivan.loskutov@ksyslabs.org>
 * \date   2012-06-23
 */

/*
 * Copyright (C) 2012 Ksys Labs LLC
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _GPIO_H_
#define _GPIO_H_

/* Genode includes */
#include <util/mmio.h>

struct Gpio_reg : Genode::Mmio
{
	Gpio_reg(Genode::addr_t const mmio_base) : Genode::Mmio(mmio_base) { }

	struct Oe              : Register<0x134, 32> {};
	struct Irqstatus_0     : Register<0x02c, 32> {};
	struct Irqstatus_set_0 : Register<0x034, 32> {};
	struct Irqstatus_clr_0 : Register<0x03c, 32> {};
	struct Ctrl            : Register<0x130, 32> {};
	struct Leveldetect0    : Register<0x140, 32> {};
	struct Leveldetect1    : Register<0x144, 32> {};
	struct Risingdetect    : Register<0x148, 32> {};
	struct Fallingdetect   : Register<0x14c, 32> {};
	struct Debounceenable  : Register<0x150, 32> {};
	struct Debouncingtime  : Register<0x154, 32>
	{
		struct Time : Bitfield<0, 8> {};
	};
	struct Cleardataout    : Register<0x190, 32> {};
	struct Setdataout      : Register<0x194, 32> {};
	struct Datain          : Register<0x138, 32> {};
};

#endif /* _GPIO_H_ */
