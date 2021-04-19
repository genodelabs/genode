/*
 * \brief  Ecspi's mmio for imx8q
 * \author Jean-Adrien Domage <jean-adrien.domage@gapfruit.com>
 * \date   2021-04-19
 */

/*
 * Copyright (C) 2013-2021 Genode Labs GmbH
 * Copyright (C) 2021 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__IMX8Q_ECSPI__MMIO_H_
#define _INCLUDE__IMX8Q_ECSPI__MMIO_H_

/* Genode include */
#include <util/mmio.h>

namespace Spi {
		using namespace Genode;
		struct Mmio;
}


/**
 * \attention Keep in mind that on ARM a word usually refer to 4 bytes.
 */
struct Spi::Mmio: Genode::Mmio
{
	/**
	 * \attention Data registers must be accessed via word operation.
	 */

	using Data_rx_register = Mmio::Register<0x0, 32>;
	struct Data_rx: Data_rx_register { };

	using Data_tx_register = Mmio::Register<0x4, 32>;
	struct Data_tx: Data_tx_register { };

	using Control_register = Mmio::Register<0x8, 32>;
	struct Control: Control_register {
		struct Enable           : Control_register ::Bitfield<0, 1>  { };
		struct Hardware_trigger : Control_register ::Bitfield<1, 1>  { };
		struct Exchange         : Control_register ::Bitfield<2, 1>  { };
		struct Start_mode_ctl   : Control_register ::Bitfield<3, 1>  { };
		struct Channel_mode     : Control_register ::Bitfield<4, 4>  { };
		struct Post_divider     : Control_register ::Bitfield<8, 4>  { };
		struct Pre_divider      : Control_register ::Bitfield<12, 4> { };
		struct Data_ready_ctl   : Control_register ::Bitfield<16, 2> { };
		struct Channel_select   : Control_register ::Bitfield<19, 2> { };

		/* Burst_length in bits start counting at 0x0 = 1bits, so 4096 = 0xFFF, BURST_SIZE - 1*/
		struct Burst_length     : Control_register ::Bitfield<20, 12> { };
	};

	using Config_register = Mmio::Register<0xC, 32>;
	struct Config: Config_register {
		struct Clock_phase            : Config_register ::Bitfield<0, 4>  { };
		struct Clock_polarity         : Config_register ::Bitfield<4, 4>  { };
		struct Slave_select_wave_form : Config_register ::Bitfield<8, 4>  { };
		struct Slave_select_polarity  : Config_register ::Bitfield<12, 4> { };
		struct Data_idle_state        : Config_register ::Bitfield<16, 4> { };
		struct Clock_idle_state       : Config_register ::Bitfield<20, 4> { };
		struct HT_Length              : Config_register ::Bitfield<24, 5> { };
	};

	using Interrupt_register = Mmio::Register<0x10, 32>;
	struct Interrupt: Interrupt_register {
		struct Tx_empty_enable        : Interrupt_register::Bitfield<0, 1> { };
		struct Tx_data_request_enable : Interrupt_register::Bitfield<1, 1> { };
		struct Tx_full_enable         : Interrupt_register::Bitfield<2, 1> { };
		struct Rx_ready_enable        : Interrupt_register::Bitfield<3, 1> { };
		struct Rx_data_request_enable : Interrupt_register::Bitfield<4, 1> { };
		struct Rx_full_enable         : Interrupt_register::Bitfield<5, 1> { };
		struct Rx_overflow_enable     : Interrupt_register::Bitfield<6, 1> { };
		struct Tx_completed_enable    : Interrupt_register::Bitfield<7, 1> { };
	};

	using Status_register = Mmio::Register<0x18, 32>;
	struct Status: Status_register {
		struct Tx_fifo_empty    : Status_register::Bitfield<0, 1> { };
		struct Tx_data_request  : Status_register::Bitfield<1, 1> { };
		struct Tx_fifo_full     : Status_register::Bitfield<2, 1> { };
		struct Rx_fifo_ready    : Status_register::Bitfield<3, 1> { };
		struct Rx_data_request  : Status_register::Bitfield<4, 1> { };
		struct Rx_fifo_full     : Status_register::Bitfield<5, 1> { };
		struct Rx_fifo_overflow : Status_register::Bitfield<6, 1> { };
		struct Tx_complete      : Status_register::Bitfield<7, 1> { };
	};

	using Test_register = Mmio::Register<0x20, 32>;
	struct Test: Test_register {
		struct Tx_fifo_counter : Test_register::Bitfield<0, 7> { };
		struct Rx_fifo_counter : Test_register::Bitfield<8, 7> { };
		struct Loop_back_ctl   : Test_register::Bitfield<31, 1> { };
	};

	explicit Mmio(addr_t base): Genode::Mmio(base) { }

};

#endif // _INCLUDE__IMX8Q_ECSPI__MMIO_H_
