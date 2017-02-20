/*
 * \brief  Clock control module register description
 * \author Nikolay Golikov <nik@ksyslabs.org>
 * \author Stefan Kalkowski <stefan.kalkowski@genode-labs.com>
 * \date   2013-04-29
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVERS__PLATFORM__SPEC__IMX53__CCM_H_
#define _DRIVERS__PLATFORM__SPEC__IMX53__CCM_H_

/* Genode includes */
#include <util/mmio.h>
#include <drivers/board_base.h>
#include <base/attached_io_mem_dataspace.h>

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
		 * Serial Clock Multiplexer Register 2
		 */
		struct Cscmr2 : Register<0x20, 32> {};

		/**
		 * D1 Clock Divider Register
		 */
		struct Cdcdr : Register<0x30, 32> {};

		/**
		 * Low power control register
		 */
		struct Clpcr : Register<0x54, 32>
		{
			struct Bypass_ipu_hs : Bitfield<18, 1> { };
		};

		struct Ccgr1 : Register<0x6c, 32>
		{
			struct I2c_1 : Bitfield<18, 2> { };
			struct I2c_2 : Bitfield<20, 2> { };
			struct I2c_3 : Bitfield<22, 2> { };
		};

		struct Ccgr5 : Register<0x7c, 32>
		{
			struct Ipu : Bitfield<10, 2> { };
		};

	public:

		Ccm(Genode::Env &env)
		: Genode::Attached_io_mem_dataspace(env, Genode::Board_base::CCM_BASE,
		                                         Genode::Board_base::CCM_SIZE),
		  Genode::Mmio((Genode::addr_t)local_addr<void>()) { }

		void i2c_1_enable(void) { write<Ccgr1::I2c_1>(3); }
		void i2c_2_enable(void) { write<Ccgr1::I2c_2>(3); }
		void i2c_3_enable(void) { write<Ccgr1::I2c_3>(3); }

		void ipu_clk_enable(void)
		{
			write<Ccgr5::Ipu>(3);
			write<Ccdr::Ipu_hs_mask>(0);
			write<Clpcr::Bypass_ipu_hs>(0);
			write<Cscmr2>(0xa2b32f0b);
			write<Cdcdr>(0x14370092);
		}

		void ipu_clk_disable(void)
		{
			write<Ccgr5::Ipu>(0);
			write<Ccdr::Ipu_hs_mask>(1);
			write<Clpcr::Bypass_ipu_hs>(1);
		}
};

#endif /* _DRIVERS__PLATFORM__SPEC__IMX53__CCM_H_ */
