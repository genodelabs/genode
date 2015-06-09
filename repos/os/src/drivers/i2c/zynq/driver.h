/*
 * \brief  I2C Driver for Zynq
 * \author Mark Albers
 * \date   2015-03-12
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _DRIVER_H_
#define _DRIVER_H_

#include <io_mem_session/connection.h>
#include <timer_session/connection.h>
#include <util/mmio.h>

#include <platform/zynq/drivers/board_base.h>

#include "i2c.h"

namespace I2C {
	using namespace Genode;
	class Driver;
}

class I2C::Driver
{
	private:
		class I2C_bank
		{
		private:
			Zynq_I2C _i2c;

		public:

			I2C_bank(Genode::addr_t base, Genode::size_t size) : _i2c(base, size) {	}

			Zynq_I2C* regs() { return &_i2c; }
		};

		static I2C_bank _i2c_bank[2];

		Driver() {}
		~Driver() {}

	public:
		static Driver& factory();

		bool read_byte_16bit_reg(unsigned bus, Genode::uint8_t adr, Genode::uint16_t reg, Genode::uint8_t *data)
		{
			Zynq_I2C *i2c_reg = _i2c_bank[bus].regs();
			Genode::uint8_t buf[2];
			buf[0]=reg >> 8;
			buf[1]=reg;
			if (i2c_reg->i2c_write(adr, buf, 2) != 0) 
			{
				PERR("Zynq i2c: read failed");
				return false;
			}
			if (i2c_reg->i2c_read_byte(adr, data) != 0) 
			{
				PERR("Zynq i2c: read failed");
				return false;
			}
			return true;
		}

		bool write_16bit_reg(unsigned bus, Genode::uint8_t adr, Genode::uint16_t reg,
			Genode::uint8_t data)
		{
			Zynq_I2C *i2c_reg = _i2c_bank[bus].regs();
			Genode::uint8_t buf[3];
			buf[0]=reg >> 8;
			buf[1]=reg;
			buf[2]=data;
			if (i2c_reg->i2c_write(adr, buf, 3) != 0) 
			{
				PERR("Zynq i2c: write failed");
				return false;
			}
			return true;
		}
};

I2C::Driver::I2C_bank I2C::Driver::_i2c_bank[2] = {
	{Genode::Board_base::I2C0_MMIO_BASE, Genode::Board_base::I2C_MMIO_SIZE},
	{Genode::Board_base::I2C1_MMIO_BASE, Genode::Board_base::I2C_MMIO_SIZE},
};

I2C::Driver& I2C::Driver::factory()
{
	static I2C::Driver driver;
	return driver;
}

#endif /* _DRIVER_H_ */
