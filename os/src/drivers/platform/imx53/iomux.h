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
	private:

		struct Gpr2 : Register<0x8,32>
		{
			struct Ch1_mode        : Bitfield<2, 2> {
					enum { ROUTED_TO_DI1 = 0x3 }; };
			struct Data_width_ch1  : Bitfield<7, 1> {
					enum { PX_18_BITS, PX_24_BITS }; };
			struct Bit_mapping_ch1 : Bitfield<8, 1> {
					enum { SPWG, JEIDA }; };
			struct Di1_vs_polarity : Bitfield<10,1> { };
		};

	public:

		Iomux()
		: Genode::Attached_io_mem_dataspace(Genode::Board_base::IOMUXC_BASE,
		                                    Genode::Board_base::IOMUXC_SIZE),
		Genode::Mmio((Genode::addr_t)local_addr<void>())
		{
		}

		void enable_di1()
		{
			write<Gpr2::Di1_vs_polarity>(1);
			write<Gpr2::Data_width_ch1>(Gpr2::Data_width_ch1::PX_18_BITS);
			write<Gpr2::Bit_mapping_ch1>(Gpr2::Bit_mapping_ch1::SPWG);
			write<Gpr2::Ch1_mode>(Gpr2::Ch1_mode::ROUTED_TO_DI1);
		}
};

#endif /* _DRIVERS__PLATFORM__IMX53__IOMUX_H_ */
