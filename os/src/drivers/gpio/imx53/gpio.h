/*
 * \brief  Gpio driver for the i.MX53
 * \author Nikolay Golikov <nik@ksyslabs.org>
 * \date   2012-12-06
 */

/*
 * Copyright (C) 2012 Ksys Labs LLC
 * Copyright (C) 2012 Genode Labs GmbH
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

	struct Data : Register_array<0x0, 32, 32, 1>
	{
		struct Pin : Bitfield<0, 1> { };
	};

	struct Dir : Register_array<0x4, 32, 32, 1>
	{
		struct Pin : Bitfield<0, 1> { };
	};

	struct Pad_stat : Register_array<0x8, 32, 32, 1>
	{
		struct Pin : Bitfield<0, 1> { };
	};

	struct Int_conf : Register_array<0xc, 32, 32, 2>
	{
		enum {
			LOW_LEVEL,
			HIGH_LEVEL,
			RIS_EDGE,
			FAL_EDGE
		};

		struct Pin : Bitfield<0, 2> { };
	};

	struct Int_mask : Register_array<0x14, 32, 32, 1>
	{
		struct Pin : Bitfield<0, 1> { };
	};

	struct Int_stat : Register_array<0x18, 32, 32, 1>
	{
		struct Pin : Bitfield<0, 1> { };
	};

	struct Edge_sel : Register_array<0x1c, 32, 32, 1>
	{
		struct Pin : Bitfield<0, 1> { };
	};
};

#endif
