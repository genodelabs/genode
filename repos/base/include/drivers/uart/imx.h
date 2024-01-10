/*
 * \brief  Driver for Freescale's i.MX UART
 * \author Norman Feske
 * \author Martin Stein
 * \date   2012-08-30
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__DRIVERS__UART__IMX_H_
#define _INCLUDE__DRIVERS__UART__IMX_H_

/* Genode includes */
#include <util/mmio.h>

namespace Genode { class Imx_uart; }


/**
 * Driver base for i.MX UART-module
 */
class Genode::Imx_uart: Mmio<0xa2>
{
	/**
	 * Control register 1
	 */
	struct Cr1 : Register<0x80, 32>
	{
		struct Uart_en    : Bitfield<0, 1> { }; /* enable UART */
		struct Doze       : Bitfield<1, 1> { }; /* disable on doze */
		struct At_dma_en  : Bitfield<2, 1> { }; /* aging DMA
		                                         * timer on */
		struct Tx_dma_en  : Bitfield<3, 1> { }; /* TX ready DMA on */
		struct Snd_brk    : Bitfield<4, 1> { }; /* send breaks */
		struct Rtsd_en    : Bitfield<5, 1> { }; /* RTS delta IRQ on */
		struct Tx_mpty_en : Bitfield<6, 1> { }; /* TX empty IRQ on */
		struct Ir_en      : Bitfield<7, 1> { }; /* enable infrared */
		struct Rx_dma_en  : Bitfield<8, 1> { }; /* RX ready DMA on */
		struct R_rdy_en   : Bitfield<9, 1> { }; /* RX ready IRQ on */

		struct Icd        : Bitfield<10, 2> /* idle IRQ condition */
		{
			enum { IDLE_4_FRAMES = 0 };
		};

		struct Id_en    : Bitfield<12, 1> { }; /* enable idle IRQ */
		struct T_rdy_en : Bitfield<13, 1> { }; /* TX ready IRQ on */
		struct Adbr     : Bitfield<14, 1> { }; /* enable baud-rate
		                                        * auto detect */
		struct Ad_en    : Bitfield<15, 1> { }; /* enable ADBR IRQ */

		/**
		 * Initialization value
		 */
		static access_t init_value()
		{
			return Uart_en::bits(1) |
				   Doze::bits(0) |
				   At_dma_en::bits(0) |
				   Tx_dma_en::bits(0) |
				   Snd_brk::bits(0) |
				   Rtsd_en::bits(0) |
				   Tx_mpty_en::bits(0) |
				   Ir_en::bits(0) |
				   Rx_dma_en::bits(0) |
				   R_rdy_en::bits(0) |
				   Id_en::bits(0) |
				   T_rdy_en::bits(0) |
				   Adbr::bits(0) |
				   Ad_en::bits(0);
		}

	};

	/**
	 * Control register 2
	 */
	struct Cr2 : Register<0x84, 32>
	{
		struct S_rst : Bitfield<0, 1> /* SW reset bit */
		{
			enum { NO_RESET = 1 };
		};

		struct Rx_en  : Bitfield<1, 1> { }; /* enable receiver */
		struct Tx_en  : Bitfield<2, 1> { }; /* enable transmitter */
		struct At_en  : Bitfield<3, 1> { }; /* enable aging timer */
		struct Rts_en : Bitfield<4, 1> { }; /* send request IRQ on */

		struct Ws : Bitfield<5, 1> /* select word size */
		{
			enum { _8_BITS = 1 };
		};

		struct Stpb : Bitfield<6, 1> /* number of stop bits */
		{
			enum { _1_BIT = 0 };
		};

		struct Pr_en  : Bitfield<8, 1> { };  /* enable parity */
		struct Esc_en : Bitfield<11, 1> { }; /* escape detection on */

		struct Ctsc   : Bitfield<13, 1>      /* select CTS control */
		{
			enum { BY_RECEIVER = 1 };
		};

		struct Irts : Bitfield<14, 1> { }; /* ignore RTS pin */
		struct Esci : Bitfield<15, 1> { }; /* enable escape IRQ */

		/**
		 * Initialization value
		 */
		static access_t init_value()
		{
			return S_rst::bits(S_rst::NO_RESET) |
				   Rx_en::bits(0) |
				   Tx_en::bits(1) |
				   At_en::bits(0) |
				   Rts_en::bits(0) |
				   Ws::bits(Ws::_8_BITS) |
				   Stpb::bits(Stpb::_1_BIT) |
				   Pr_en::bits(0) |
				   Esc_en::bits(0) |
				   Ctsc::bits(Ctsc::BY_RECEIVER) |
				   Irts::bits(1) |
				   Esci::bits(0);
		}
	};

	/**
	 * Control register 3
	 */
	struct Cr3 : Register<0x88, 32>
	{
		struct Rxdmux_sel : Bitfield<2, 1> { }; /* use muxed RXD */
		struct Aci_en     : Bitfield<0, 1> { }; /* autobaud count IRQ on */
		struct Dtrd_en    : Bitfield<3, 1> { }; /* data terminal ready
		                                         * delta IRQ on */
		struct Awak_en    : Bitfield<4, 1> { }; /* wake IRQ on */
		struct Air_int_en : Bitfield<5, 1> { }; /* IR wake IRQ on */
		struct Rx_ds_en   : Bitfield<6, 1> { }; /* RX status IRQ on */
		struct Ad_nimp    : Bitfield<7, 1> { }; /* autobaud detect off */
		struct Ri_en      : Bitfield<8, 1> { }; /* ring indicator IRQ on */
		struct Dcd_en     : Bitfield<9, 1> { }; /* data carrier detect
		                                         * IRQ on */
		struct Dsr        : Bitfield<10,1> { }; /* DSR/DTR output */
		struct Frame_en   : Bitfield<11,1> { }; /* frame error IRQ on */
		struct Parity_en  : Bitfield<12,1> { }; /* parity error IRQ on */
		struct Dtr_en     : Bitfield<13,1> { }; /* data terminal ready
		                                         * IRQ on */
		struct Dpec_ctrl  : Bitfield<14,2> { }; /* DTR/DSR IRQ edge
		                                         * control */

		/**
		 * Initialization value
		 */
		static access_t init_value()
		{
			return Aci_en::bits(0) |
				   Rxdmux_sel::bits(0) |
				   Dtrd_en::bits(0) |
				   Awak_en::bits(0) |
				   Air_int_en::bits(0) |
				   Rx_ds_en::bits(0) |
				   Ad_nimp::bits(1) |
				   Ri_en::bits(0) |
				   Dcd_en::bits(0) |
				   Dsr::bits(0) |
				   Frame_en::bits(0) |
				   Parity_en::bits(0) |
				   Dtr_en::bits(0) |
				   Dpec_ctrl::bits(0);
		}
	};

	/**
	 * Control register 4
	 */
	struct Cr4 : Register<0x8c, 32>
	{
		struct Dr_en       : Bitfield<0, 1> { }; /* RX data ready IRQ on */
		struct Or_en       : Bitfield<1, 1> { }; /* RX overrun IRQ on */
		struct Bk_en       : Bitfield<2, 1> { }; /* BREAK IRQ on */
		struct Tc_en       : Bitfield<3, 1> { }; /* TX complete IRQ on */
		struct Lp_dis      : Bitfield<4, 1> { }; /* low power off */
		struct IR_sc       : Bitfield<5, 1> { }; /* use UART ref clock
		                                          * for vote logic */
		struct Id_dma_en   : Bitfield<6, 1> { }; /* idle DMA IRQ on */
		struct Wake_en     : Bitfield<7, 1> { }; /* WAKE IRQ on */
		struct IR_en       : Bitfield<8, 1> { }; /* serial IR IRQ on */
		struct Cts_level   : Bitfield<10,6> { }; /* CTS trigger level*/

		/**
		 * Initialization value
		 */
		static access_t init_value()
		{
			return Dr_en::bits(0) |
			       Or_en::bits(0) |
			       Bk_en::bits(0) |
			       Tc_en::bits(0) |
			       Lp_dis::bits(0) |
			       IR_sc::bits(0) |
			       Id_dma_en::bits(0) |
			       Wake_en::bits(0) |
			       IR_en::bits(0) |
			       Cts_level::bits(0);
		}
	};

	/**
	 * Status register 2
	 */
	struct Sr2 : Register<0x98, 32>
	{
		struct Txdc : Bitfield<3, 1> { }; /* transmission complete */
	};

	/**
	 * Transmitter register
	 */
	struct Txd : Register<0x40, 32>
	{
		struct Tx_data : Bitfield<0, 8> { }; /* transmit data */
	};

	/**
	 * Transmit character 'c' without care about its type
	 */
	void _put_char(char c)
	{
		while (!read<Sr2::Txdc>()) ;
		write<Txd::Tx_data>(c);
	}

	public:

		/**
		 * Constructor
		 *
		 * \param base  device MMIO base
		 */
		Imx_uart(addr_t base, uint32_t, uint32_t) : Mmio({(char*)base, Mmio::SIZE})
		{
			init();
		}

		void init()
		{
			write<Cr1>(Cr1::init_value());
			write<Cr2>(Cr2::init_value());
			write<Cr3>(Cr3::init_value());
			write<Cr4>(Cr4::init_value());
		}

		/**
		 * Print character 'c' through the UART
		 */
		void put_char(char c)
		{
			/* transmit character */
			_put_char(c);
		}
};

#endif /* _INCLUDE__DRIVERS__UART__IMX_H_ */
