/*
 * \brief  Driver for the MINI UART for Rpi3
 * \author Tomasz Gajewski
 * \date   2019-01-30
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__DRIVERS__UART__BCM2835_MINI_H_
#define _INCLUDE__DRIVERS__UART__BCM2835_MINI_H_

/* Genode includes */
#include <util/mmio.h>

namespace Genode { class Bcm2835_mini_uart; }


/**
 * Driver base for the PrimeCell UART MINI Revision r1p3
 */
class Genode::Bcm2835_mini_uart : Mmio
{
	protected:

		enum { MAX_BAUD_RATE = 0xfffffff };

		/**
		 * Auxiliary Interrupt status
		 */
		struct AuxIrq : public Register<0x00, 32>
		{
			struct MiniUartIrq : Bitfield<0,1>  { }; /* read only */
			struct Spi1Irq     : Bitfield<1,1>  { }; /* read only */
			struct Spi2Irq     : Bitfield<2,1>  { }; /* read only */
		};

		/**
		 * Auxiliary enables
		 */
		struct AuxEnables : public Register<0x04, 32>
		{
			struct MiniUartEnable : Bitfield<0,1> { };
			struct Spi1Enable     : Bitfield<1,1> { };
			struct Spi2Enable     : Bitfield<2,1> { };
		};

		/**
		 * Mini Uart I/O Data
		 */
		struct AuxMuIoReg : public Register<0x40, 32>
		{
			struct TransmitReceive     : Bitfield<0,8> { };

			struct DLABLS8BitsBaudrate : Bitfield<0,8> { };
		};

		/**
		 * Mini Uart Interrupt Enable
		 */
		struct AuxMuIerReg : public Register<0x44, 32>
		{
			struct EnableReceiveInterrupt    : Bitfield<0,1> { };
			struct EnableTransmitInterrupt   : Bitfield<1,1> { };

			struct DLABMS8BitsBaudrate       : Bitfield<0,8> { };
		};

		/**
		 * Mini Uart Interrupt Identify
		 */
		struct AuxMuIirReg : public Register<0x48, 32>
		{
			struct InterruptPending : Bitfield<0,1> { };
			struct InterruptId      : Bitfield<1,2> { /* read only */
				enum {
					NO_INTERRUPTS = 0,
					TRANSMIT_HOLDING_REGISTER_EMPTY = 1,
					RECEIVER_HOLDS_VALID_BYTE = 2,
				};

			};
			struct FifoClear        : Bitfield<1,2> { /* write only */
				enum {
					CLEAR_RECEIVE_FIFO = 1,
                                        CLEAR_TRANSMIT_FIFO = 2,
					CLEAR_BOTH_FIFOS = 3,
				};
                        };
		};

		/**
		 * Mini Uart Line Control
		 */
		struct AuxMuLcrReg : public Register<0x4C, 32>
		{
			struct DataSize : Bitfield<0,1> { };
			struct Break    : Bitfield<6,1> { };
			struct DLAB     : Bitfield<7,1> { };
		};

		/**
		 * Mini Uart Modem Control
		 */
		struct AuxMuMcrReg : public Register<0x50, 32>
		{
			struct RTS : Bitfield<1,1> { };
		};

		/**
		 * Mini Uart Line Status
		 */
		struct AuxMuLsrReg : public Register<0x54, 32>
		{
			struct DataReady        : Bitfield<0,1> { };
			struct ReceiverOverrun  : Bitfield<1,1> { }; /* read/clear only */
			struct TransmitterEmpty : Bitfield<5,1> { }; /* read only */
			struct TransmitterIdle  : Bitfield<6,1> { }; /* read only */
		};

		/**
		 * Mini Uart Modem Status
		 */
		struct AuxMuMsrReg : public Register<0x58, 32>
		{
			struct CtsStatus : Bitfield<5,1> { }; /* read only */
		};

		/**
		 * Mini Uart Scratch
		 */
		struct AuxMuScratch : public Register<0x5C, 32>
		{
			struct Scratch : Bitfield<0,8> { };
		};

		/**
		 * Mini Uart Extra Control
		 */
		struct AuxMuCntlReg : public Register<0x60, 32>
		{
			struct ReceiverEnable                   : Bitfield<0,1> { };
			struct TransmitterEnable                : Bitfield<1,1> { };
			struct EnableReceiveAutoFlowRTSControl  : Bitfield<2,1> { };
			struct EnableTransmitAutoFlowCTSControl : Bitfield<3,1> { };
			struct RTSAutoFlowLevel                 : Bitfield<4,2> {
				enum {
					DE_ASSERT_RTS_RECEIVE_FIFO_3 = 0,
					DE_ASSERT_RTS_RECEIVE_FIFO_2 = 1,
					DE_ASSERT_RTS_RECEIVE_FIFO_1 = 2,
					DE_ASSERT_RTS_RECEIVE_FIFO_4 = 3,
				};
                        };
			struct RTSAssertLevel                   : Bitfield<6,1> { };
			struct CTSAssertLevel                   : Bitfield<7,1> { };
		};

		/**
		 * Mini Uart Extra Status
		 */
		struct AuxMuStatReg : public Register<0x64, 32>
		{
			struct SymbolAvailable        : Bitfield<0, 1> { }; /* read only */
			struct SpaceAvailable         : Bitfield<1, 1> { }; /* read only */
			struct ReceiverIsIdle         : Bitfield<2, 1> { }; /* read only */
			struct TransmitterIsIdle      : Bitfield<3, 1> { }; /* read only */
			struct ReceiverOverrun        : Bitfield<4, 1> { }; /* read only */
			struct TransmitFifoIsFull     : Bitfield<5, 1> { }; /* read only */
			struct RTSStatus              : Bitfield<6, 1> { }; /* read only */
			struct CTSLine                : Bitfield<7, 1> { }; /* read only */
			struct TransmitFifoIsEmpty    : Bitfield<8, 1> { }; /* read only */
			struct TransmitterDone        : Bitfield<9, 1> { }; /* read only */
			struct ReceiveFifoFillLevel   : Bitfield<16,4> { }; /* read only */
			struct TransmitFifoFillLevel  : Bitfield<24,4> { }; /* read only */
		};

		/**
		 * Mini Uart Baudrate
		 */
		struct AuxMuBaudReg : public Register<0x68, 32>
		{
			struct Baudrate   : Bitfield<0,16> { };
		};

		/**
		 * Idle until the device is ready for action
		 */
		void _wait_until_ready() {
			do {
				asm volatile("nop");
			} while (!read<AuxMuLsrReg::TransmitterEmpty>()) ;
		}

	public:

		/**
		 * Constructor
		 * \param  base       device MMIO base
		 * \param  clock      device reference clock frequency
		 * \param  baud_rate  targeted UART baud rate
		 */
		inline Bcm2835_mini_uart(addr_t const base, uint32_t const clock,
		                         uint32_t const baud_rate);

		/**
		 * Send ASCII char 'c' over the UART interface
		 */
		inline void put_char(char const c);
};


Genode::Bcm2835_mini_uart::Bcm2835_mini_uart(addr_t const base,
                                             uint32_t const clock,
                                             uint32_t const baud_rate)
: Mmio(base)
{
	/* enable UART1, AUX mini uart */
	write<AuxEnables>(read<AuxEnables::MiniUartEnable>() |
	                  AuxEnables::MiniUartEnable::bits(1));

	write<AuxMuCntlReg>(0);
	write<AuxMuLcrReg>(3);
	write<AuxMuMcrReg>(0);
	write<AuxMuIerReg>(0);
	write<AuxMuIirReg>(0xc6);
	uint32_t const adjusted_br = ((clock / baud_rate) / 8) - 1;
	write<AuxMuBaudReg>(AuxMuBaudReg::Baudrate::bits(adjusted_br));
	write<AuxMuCntlReg>(3);
	_wait_until_ready();
}


void Genode::Bcm2835_mini_uart::put_char(char const c)
{
	_wait_until_ready();

	/* transmit character */
	write<AuxMuIoReg::TransmitReceive>(c);
	_wait_until_ready();
}


#endif /* _INCLUDE__DRIVERS__UART__BCM2835_MINI_H_ */
