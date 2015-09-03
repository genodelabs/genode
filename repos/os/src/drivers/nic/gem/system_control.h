/*
 * \brief  Base EMAC driver for the Xilinx EMAC PS used on Zynq devices
 * \author Timo Wischer
 * \date   2015-03-10
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__DRIVERS__NIC__GEM__SYSTEM_CONTROL_H_
#define _INCLUDE__DRIVERS__NIC__GEM__SYSTEM_CONTROL_H_

/* Genode includes */
#include <os/attached_mmio.h>
#include <timer_session/connection.h>

#include <drivers/board_base.h>


using namespace Genode;


class System_control : private Genode::Attached_mmio
{
	private:
		struct Lock : Register<0x4, 32> {
			enum { MAGIC = 0x767B };
		};
		struct Unlock : Register<0x8, 32> {
			enum { MAGIC = 0xDF0D };
		};
		struct Gem0_rclk_ctrl : Register<0x138, 32> { };
		struct Gem0_clk_ctrl : Register<0x140, 32> { };

		struct Mio_pin_16 : Register<0x740, 32> {
			struct Tri_state_enable  : Bitfield<0, 1> {};
			struct Level_0_mux  : Bitfield<1, 1> {
				enum { ETH0 = 0b1 };
			};
			struct Fast_cmos_edge  : Bitfield<8, 1> {};
			struct IO_type  : Bitfield<9, 3> {
				enum { LVCMOS18 = 0b001 };
			};

			static access_t FAST_LVCMOS18_ETH0() {
				return Fast_cmos_edge::bits(1) |
					IO_type::bits(IO_type::LVCMOS18) |
					Level_0_mux::bits(Level_0_mux::ETH0);
			}

			static access_t FAST_LVCMOS18_ETH0_TRISTATE() {
				return FAST_LVCMOS18_ETH0() |
					Tri_state_enable::bits(1);
			}
		};
		struct Mio_pin_17 : Register<0x744, 32> { };
		struct Mio_pin_18 : Register<0x748, 32> { };
		struct Mio_pin_19 : Register<0x74C, 32> { };
		struct Mio_pin_20 : Register<0x750, 32> { };
		struct Mio_pin_21 : Register<0x754, 32> { };
		struct Mio_pin_22 : Register<0x758, 32> { };
		struct Mio_pin_23 : Register<0x75C, 32> { };
		struct Mio_pin_24 : Register<0x760, 32> { };
		struct Mio_pin_25 : Register<0x764, 32> { };
		struct Mio_pin_26 : Register<0x768, 32> { };
		struct Mio_pin_27 : Register<0x76C, 32> { };

		struct Mio_pin_52 : Register<0x7D0, 32> {
			struct Level_3_mux  : Bitfield<5, 3> {
				enum { MDIO0 = 0b100 };
			};

			static access_t LVCMOS18_MDIO0() {
				return Mio_pin_16::IO_type::bits(Mio_pin_16::IO_type::LVCMOS18) |
					Level_3_mux::bits(Level_3_mux::MDIO0);
			}
		};
		struct Mio_pin_53 : Register<0x7D4, 32> { };

		struct Gpio_b_ctrl : Register<0xB00, 32> {
			struct Vref_enable  : Bitfield<0, 1> {};
		};


		class Lock_guard
		{
			private:
				System_control& _sys_ctrl;

				void _unlock() { _sys_ctrl.write<Unlock>(Unlock::MAGIC); }
				void _lock() { _sys_ctrl.write<Lock>(Lock::MAGIC); }

			public:
				Lock_guard(System_control& sys_ctrl) : _sys_ctrl(sys_ctrl)
				{
					_unlock();
				}

				~Lock_guard()
				{
					_lock();
				}
		};

		unsigned int old_data[0x300];

	public:
		System_control() :
			Attached_mmio(Board_base::MMIO_1_BASE, 0xB80)
		{
			Lock_guard lock(*this);

			write<Mio_pin_16>(Mio_pin_16::FAST_LVCMOS18_ETH0());
			write<Mio_pin_17>(Mio_pin_16::FAST_LVCMOS18_ETH0());
			write<Mio_pin_18>(Mio_pin_16::FAST_LVCMOS18_ETH0());
			write<Mio_pin_19>(Mio_pin_16::FAST_LVCMOS18_ETH0());
			write<Mio_pin_20>(Mio_pin_16::FAST_LVCMOS18_ETH0());
			write<Mio_pin_21>(Mio_pin_16::FAST_LVCMOS18_ETH0());

			write<Mio_pin_22>(Mio_pin_16::FAST_LVCMOS18_ETH0_TRISTATE());
			write<Mio_pin_23>(Mio_pin_16::FAST_LVCMOS18_ETH0_TRISTATE());
			write<Mio_pin_24>(Mio_pin_16::FAST_LVCMOS18_ETH0_TRISTATE());
			write<Mio_pin_25>(Mio_pin_16::FAST_LVCMOS18_ETH0_TRISTATE());
			write<Mio_pin_26>(Mio_pin_16::FAST_LVCMOS18_ETH0_TRISTATE());
			write<Mio_pin_27>(Mio_pin_16::FAST_LVCMOS18_ETH0_TRISTATE());

			write<Mio_pin_52>(Mio_pin_52::LVCMOS18_MDIO0());
			write<Mio_pin_53>(Mio_pin_52::LVCMOS18_MDIO0());

			// TODO possibly not needed because uboot do not enable this register
			/* enable internel VRef */
			write<Gpio_b_ctrl>(Gpio_b_ctrl::Vref_enable::bits(1));
		}

		void set_clk(uint32_t clk, uint32_t rclk)
		{
			Lock_guard lock(*this);

			write<Gem0_clk_ctrl>(clk);
			write<Gem0_rclk_ctrl>(rclk);

			static Timer::Connection timer;
			timer.msleep(100);
		}
};

#endif /* _INCLUDE__DRIVERS__NIC__GEM__SYSTEM_CONTROL_H_ */
