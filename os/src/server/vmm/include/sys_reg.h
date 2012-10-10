/*
 * \brief  Driver for the Motherboard Express system registers
 * \author Stefan Kalkowski
 * \date   2012-09-21
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _BASE_HW__SRC__SERVER__VMM__SYS_REG_H_
#define _BASE_HW__SRC__SERVER__VMM__SYS_REG_H_

/* Genode includes */
#include <util/mmio.h>

namespace Genode
{

	class Sys_reg : Mmio
	{
		private:

			struct Sys_mci : public Register<0x48, 32> {};

			struct Sys_24mhz : public Register<0x5c, 32> {};

			struct Sys_misc  : public Register<0x60, 32> {};

			struct Sys_cfg_data : public Register<0xa0, 32, true> {};

			struct Sys_cfg_ctrl : public Register<0xa4, 32, true>
			{
				struct Device   : Bitfield<0,12> { };
				struct Position : Bitfield<12,4> { };
				struct Site     : Bitfield<16,2> { };
				struct Function : Bitfield<20,6> { };
				struct Write    : Bitfield<30,1> { };
				struct Start    : Bitfield<31,1> { };
			};

			struct Sys_cfg_stat : public Register<0xa8, 32>
			{
				struct Complete : Bitfield<0,1> { };
				struct Error    : Bitfield<1,1> { };
			};

		public:

			Sys_reg(addr_t const base) : Mmio(base) {}

			uint32_t counter() { return read<Sys_24mhz>(); }

			uint32_t misc_flags() { return read<Sys_misc>(); }

			void osc1(uint32_t mhz)
			{
				write<Sys_cfg_stat::Complete>(0);
				write<Sys_cfg_data>(mhz);
				write<Sys_cfg_ctrl>(Sys_cfg_ctrl::Device::bits(1)   |
				                    Sys_cfg_ctrl::Site::bits(1)     |
				                    Sys_cfg_ctrl::Function::bits(1) |
				                    Sys_cfg_ctrl::Write::bits(1)    |
				                    Sys_cfg_ctrl::Start::bits(1));
				while (!read<Sys_cfg_stat::Complete>()) ;
			}

			void dvi_source(uint32_t site)
			{
				if (site > 2) {
					PERR("Invalid site value %u ignored", site);
					return;
				}
				write<Sys_cfg_stat::Complete>(0);
				write<Sys_cfg_data>(site);
				write<Sys_cfg_ctrl>(Sys_cfg_ctrl::Site::bits(1)       |
				                    Sys_cfg_ctrl::Function::bits(0x7) |
				                    Sys_cfg_ctrl::Write::bits(1)      |
				                    Sys_cfg_ctrl::Start::bits(1));
				while (!read<Sys_cfg_stat::Complete>()) ;
			}

			void dvi_mode(uint32_t mode)
			{
				if (mode > 4) {
					PERR("Invalid dvi mode %u ignored", mode);
					return;
				}
				write<Sys_cfg_stat::Complete>(0);
				write<Sys_cfg_data>(mode);
				write<Sys_cfg_ctrl>(Sys_cfg_ctrl::Function::bits(0xb) |
				                    Sys_cfg_ctrl::Write::bits(1)      |
				                    Sys_cfg_ctrl::Start::bits(1));
				while (!read<Sys_cfg_stat::Complete>()) ;
			}

			uint32_t mci_status() { return read<Sys_mci>(); }
	};
}

#endif /* _BASE_HW__SRC__SERVER__VMM__SYS_REG_H_ */
