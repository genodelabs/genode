/*
 * \brief  Driver for the Xilinx LogiCORE IP XPS UART Lite 1.01a
 * \author Martin stein
 * \date   2011-05-06
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__DEVICES__XILINX_XPS_UARTL_H_ 
#define _INCLUDE__DEVICES__XILINX_XPS_UARTL_H_ 

#include <cpu/config.h>

namespace Xilinx {

	/**
	 * Driver for the Xilinx LogiCORE IP XPS UART Lite 1.01a
	 */
	class Xps_uartl
	{
		public:

			/**
			 * Constructor
			 */
			Xps_uartl(Cpu::addr_t const & base);

			/**
			 * Send one ASCII char over the UART interface
			 */
			inline void send(char const & c);

		private:

			/**
			 * Relative MMIO structure 
			 */

			typedef Cpu::uint32_t Register;

			enum {
				RX_FIFO_OFF  = 0 * Cpu::WORD_SIZE,
				TX_FIFO_OFF  = 1 * Cpu::WORD_SIZE,
				STAT_REG_OFF = 2 * Cpu::WORD_SIZE,
				CTRL_REG_OFF = 3 * Cpu::WORD_SIZE,
			};

			struct Rx_fifo {
				enum {
					BITFIELD_ENUMS(RX_DATA, 0, 8)
				};
			};

			struct Tx_fifo {
				enum {
					BITFIELD_ENUMS(TX_DATA, 0, 8)
				};
			};

			struct Ctrl_reg {
				enum {
					BITFIELD_ENUMS(RST_TX_FIFO, 0, 1)
					BITFIELD_ENUMS(RST_RX_FIFO, 1, 1)
					BITFIELD_ENUMS(ENABLE_INTR, 4, 1)
				};
			};

			struct Stat_reg {
				enum {
					BITFIELD_ENUMS(RX_FIFO_VALID_DATA, 0, 1)
					BITFIELD_ENUMS(RX_FIFO_FULL,       1, 1)
					BITFIELD_ENUMS(TX_FIFO_EMPTY,      2, 1)
					BITFIELD_ENUMS(TX_FIFO_FULL,       3, 1)
					BITFIELD_ENUMS(INTR_ENABLED,       4, 1)
					BITFIELD_ENUMS(OVERRUN_ERROR,      5, 1)
					BITFIELD_ENUMS(FRAME_ERROR,        6, 1)
					BITFIELD_ENUMS(PARITY_ERROR,       7, 1)
				};
			};

			/**
			 * Absolute register pointers
			 */
			volatile Register* const _rx_fifo;
			volatile Register* const _tx_fifo;
			volatile Register* const _stat_reg;
			volatile Register* const _ctrl_reg;
	};
}


Xilinx::Xps_uartl::Xps_uartl(Cpu::addr_t const & base) :
	_rx_fifo((Register*)(base + RX_FIFO_OFF)),
	_tx_fifo((Register*)(base + TX_FIFO_OFF)),
	_stat_reg((Register*)(base + STAT_REG_OFF)),
	_ctrl_reg((Register*)(base + CTRL_REG_OFF))
{}


void Xilinx::Xps_uartl::send(char const & c){
	while(*_stat_reg & Stat_reg::TX_FIFO_FULL_MSK);
	*_tx_fifo = c;
}


#endif /* _INCLUDE__DEVICES__XILINX_XPS_UARTL_H_ */ 
