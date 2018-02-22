/*
 * \brief  Gpio driver for Freescale
 * \author Nikolay Golikov <nik@ksyslabs.org>
 * \author Stefan Kalkowski <stefan.kalkowski@genode-labs.com>
 * \date   2012-12-06
 */

/*
 * Copyright (C) 2012 Ksys Labs LLC
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVERS__GPIO__SPEC__IMX__GPIO_H_
#define _DRIVERS__GPIO__SPEC__IMX__GPIO_H_

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

	struct Data     : Register_array<0x0, 32, 32, 1> {};
	struct Dir      : Register_array<0x4, 32, 32, 1> {};
	struct Pad_stat : Register_array<0x8, 32, 32, 1> {};
	struct Int_conf : Register_array<0xc, 32, 32, 2>
	{
		enum {
			LOW_LEVEL,
			HIGH_LEVEL,
			RIS_EDGE,
			FAL_EDGE
		};
	};

	struct Int_mask : Register_array<0x14, 32, 32, 1> {};
	struct Int_stat : Register_array<0x18, 32, 32, 1> {};
	struct Edge_sel : Register_array<0x1c, 32, 32, 1> {};
};

#endif /* _DRIVERS__GPIO__SPEC__IMX__GPIO_H_ */
