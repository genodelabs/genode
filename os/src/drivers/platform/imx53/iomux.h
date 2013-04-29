/*
 * \brief  IOMUX controller register description
 * \author Stefan Kalkowski <stefan.kalkowski@genode-labs.com>
 * \date   2013-04-29
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _DRIVERS__PLATFORM__IMX53__IOMUX_H_
#define _DRIVERS__PLATFORM__IMX53__IOMUX_H_

/* Genode includes */
#include <util/mmio.h>
#include <drivers/board_base.h>
#include <os/attached_io_mem_dataspace.h>

class Iomux : public Genode::Attached_io_mem_dataspace,
               Genode::Mmio
{
	public:

		Iomux()
		: Genode::Attached_io_mem_dataspace(Genode::Board_base::IOMUXC_BASE,
		                                    Genode::Board_base::IOMUXC_SIZE),
		Genode::Mmio((Genode::addr_t)local_addr<void>()) {}
};

#endif /* _DRIVERS__PLATFORM__IMX53__IOMUX_H_ */
