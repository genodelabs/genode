/*
 * \brief  Driver for the Trustzone Protection Controller BP147
 * \author Stefan Kalkowski
 * \date   2012-07-04
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _BASE_HW__SRC__SERVER__VMM__BP_147_H_
#define _BASE_HW__SRC__SERVER__VMM__BP_147_H_

/* Genode includes */
#include <util/mmio.h>

namespace Genode
{

	class Bp_147 : Mmio
	{
		private:

			/**
			 * Secure RAM Region Size Register
			 */
			struct Tzpcr0size : public Register<0x00, 32>
			{
				struct R0size : Bitfield<0,10>  { };
			};

			/**
			 * Decode Protection 0 Registers
			 */
			template <off_t OFF>
			struct Tzpcdecprot0 : public Register<OFF, 32>
			{
				struct Pl341_apb      : Register<OFF, 32>::template Bitfield<0,1>  {};
				struct Pl354_apb      : Register<OFF, 32>::template Bitfield<1,1>  {};
				struct Scc            : Register<OFF, 32>::template Bitfield<2,1>  {};
				struct Dual_timer     : Register<OFF, 32>::template Bitfield<4,1>  {};
				struct Watchdog       : Register<OFF, 32>::template Bitfield<5,1>  {};
				struct Tzpc           : Register<OFF, 32>::template Bitfield<6,1>  {};
				struct Pl351_apb      : Register<OFF, 32>::template Bitfield<7,1>  {};
				struct Fast_pl301_apb : Register<OFF, 32>::template Bitfield<9,1>  {};
				struct Slow_pl301_apb : Register<OFF, 32>::template Bitfield<10,1> {};
				struct Dmc_tzasc      : Register<OFF, 32>::template Bitfield<12,1> {};
				struct Nmc_tzasc      : Register<OFF, 32>::template Bitfield<12,1> {};
				struct Smc_tzasc      : Register<OFF, 32>::template Bitfield<13,1> {};
				struct Debug_apb_phs  : Register<OFF, 32>::template Bitfield<14,1> {};
			};

			/**
			 * Decode Protection 1 Registers
			 */
			template <off_t OFF>
			struct Tzpcdecprot1 : public Register<OFF, 32>
			{
				struct External_axi_slave_port
					: Register<OFF, 32>::template Bitfield<0,1> {};
				/* SMC access */
				struct Pl354_axi
					: Register<OFF, 32>::template Bitfield<1,1> {};
				struct Pl351_axi
					: Register<OFF, 32>::template Bitfield<2,1> {};
				struct Entire_apb
					: Register<OFF, 32>::template Bitfield<3,1> {};
				struct Pl111_configuration_port
					: Register<OFF, 32>::template Bitfield<4,1> {};
				struct Axi_ram
					: Register<OFF, 32>::template Bitfield<5,1> {};
				/* DDR RAM access */
				struct Pl341_axi
					: Register<OFF, 32>::template Bitfield<6,1> {};
				/* ACP access */
				struct Cortexa9_coherency_port
					: Register<OFF, 32>::template Bitfield<8,1> {};
				struct Entire_slow_axi_system
					: Register<OFF, 32>::template Bitfield<9,1> {};
			};

			/**
			 * Decode Protection 2 Registers
			 */
			template <off_t OFF>
			struct Tzpcdecprot2 : public Register<OFF, 32>
			{
			struct External_master_tz : Register<OFF, 32>::template Bitfield<0,1> {};
			struct Dap_tz_override    : Register<OFF, 32>::template Bitfield<1,1> {};
			struct Pl111_master_tz    : Register<OFF, 32>::template Bitfield<2,1> {};
			struct Dmc_tzasc_lockdown : Register<OFF, 32>::template Bitfield<3,1> {};
			struct Nmc_tzasc_lockdown : Register<OFF, 32>::template Bitfield<4,1> {};
			struct Smc_tzasc_lockdown : Register<OFF, 32>::template Bitfield<5,1> {};
			};

			struct Tzpcdecprot0stat : Tzpcdecprot0<0x800> {};
			struct Tzpcdecprot0set  : Tzpcdecprot0<0x804> {};
			struct Tzpcdecprot0clr  : Tzpcdecprot0<0x808> {};
			struct Tzpcdecprot1stat : Tzpcdecprot1<0x80c> {};
			struct Tzpcdecprot1set  : Tzpcdecprot1<0x810> {};
			struct Tzpcdecprot1clr  : Tzpcdecprot1<0x814> {};
			struct Tzpcdecprot2stat : Tzpcdecprot2<0x818> {};
			struct Tzpcdecprot2set  : Tzpcdecprot2<0x81c> {};
			struct Tzpcdecprot2clr  : Tzpcdecprot2<0x820> {};

		public:

			Bp_147(addr_t const base) : Mmio(base)
			{
				/**
				 * Configure TZPC to allow non-secure AXI signals to
				 * Static Memory Controller (SMC),
				 * Dynamic Memory Controller (DMC),
				 * Accelerator Coherency Port (ACP), and
				 * PL111 configuration registers
				 */
				write<Tzpcdecprot1set>(
					Tzpcdecprot1set::Pl341_axi::bits(1)               |
					Tzpcdecprot1set::Pl354_axi::bits(1)               |
					Tzpcdecprot1set::Cortexa9_coherency_port::bits(1) |
					Tzpcdecprot1set::Pl111_configuration_port::bits(1));
			}
	};

}

#endif /* _BASE_HW__SRC__SERVER__VMM__BP_147_H_ */
