/*
 * \brief  Clock control module register description
 * \author Nikolay Golikov <nik@ksyslabs.org>
 * \author Stefan Kalkowski <stefan.kalkowski@genode-labs.com>
 * \date   2013-04-29
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _DRIVERS__PLATFORM__IMX53__CCM_H_
#define _DRIVERS__PLATFORM__IMX53__CCM_H_

/* Genode includes */
#include <util/mmio.h>
#include <drivers/board_base.h>
#include <os/attached_io_mem_dataspace.h>

class Ccm : public Genode::Attached_io_mem_dataspace,
            Genode::Mmio
{
	private:

		/**
		 * Control divider register
		 */
		struct Ccdr : Register<0x4, 32>
		{
			struct Ipu_hs_mask : Bitfield <21, 1> { };
		};

		/**
		 * Low power control register
		 */
		struct Clpcr : Register<0x54, 32>
		{
			struct Bypass_ipu_hs : Bitfield<18, 1> { };
		};

		struct Cccr5 : Register<0x7c, 32>
		{
			struct Ipu_clk_en : Bitfield<10, 2> { };
		};

	public:

		Ccm()
		: Genode::Attached_io_mem_dataspace(Genode::Board_base::CCM_BASE,
		                                    Genode::Board_base::CCM_SIZE),
		  Genode::Mmio((Genode::addr_t)local_addr<void>()) {}

		void ipu_clk_enable(void)
		{
			write<Cccr5::Ipu_clk_en>(3);
			write<Ccdr::Ipu_hs_mask>(0);
			write<Clpcr::Bypass_ipu_hs>(0);
		}

		void ipu_clk_disable(void)
		{
			write<Cccr5::Ipu_clk_en>(0);
			write<Ccdr::Ipu_hs_mask>(1);
			write<Clpcr::Bypass_ipu_hs>(1);
		}
};

#endif /* _DRIVERS__PLATFORM__IMX53__CCM_H_ */
