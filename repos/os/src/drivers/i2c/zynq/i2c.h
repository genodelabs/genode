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

#ifndef _I2C_H_
#define _I2C_H_

#include <os/attached_io_mem_dataspace.h>
#include <util/mmio.h>

/* Transfer direction */
#define SENDING	0
#define RECEIVING 1

/* Interrupt masks */
#define INTERRUPT_ARB_LOST_MASK  0x00000200
#define INTERRUPT_RX_UNF_MASK    0x00000080
#define INTERRUPT_TX_OVR_MASK    0x00000040
#define INTERRUPT_RX_OVR_MASK    0x00000020
#define INTERRUPT_SLV_RDY_MASK   0x00000010
#define INTERRUPT_TO_MASK        0x00000008
#define INTERRUPT_NACK_MASK      0x00000004
#define INTERRUPT_DATA_MASK      0x00000002
#define INTERRUPT_COMP_MASK      0x00000001
#define ALL_INTERRUPTS_MASK     0x000002FF

/* Maximal transfer size */
#define I2C_MAX_TRANSFER_SIZE 252

/* FIFO size */
#define I2C_FIFO_DEPTH 16

/* Number of bytes at data intr */
#define I2C_DATA_INTR_DEPTH 14

namespace I2C {
	using namespace Genode;
	class Zynq_I2C;
}

struct I2C::Zynq_I2C : Attached_io_mem_dataspace, Mmio
{
	Zynq_I2C(Genode::addr_t const mmio_base, Genode::size_t const mmio_size) :
		Genode::Attached_io_mem_dataspace(mmio_base, mmio_size),
	  	Genode::Mmio((Genode::addr_t)local_addr<void>())
	{

	}

	~Zynq_I2C() 
	{
	
	}

	/*
	 * Registers
	 */

	struct Control : Register<0x0, 16>
	{
		struct divisor_a 	: Bitfield<14,2> {};
		struct divisor_b 	: Bitfield<8,6> {};
		struct CLR_FIFO	 	: Bitfield<6,1> {};
		struct SLVMON	 	: Bitfield<5,1> {};
		struct HOLD	 	: Bitfield<4,1> {};
		struct ACK_EN	 	: Bitfield<3,1> {};
		struct NEA	 	: Bitfield<2,1> {};
		struct MS	 	: Bitfield<1,1> {};
		struct RW	 	: Bitfield<0,1> {};
	};

	struct Status : Register<0x4, 16>
	{
		struct BA 		: Bitfield<8,1> {};
		struct RXOVF 		: Bitfield<7,1> {};
		struct TXDV 		: Bitfield<6,1> {};
		struct RXDV 		: Bitfield<5,1> {};
		struct RXRW 		: Bitfield<3,1> {};
	};

	struct I2C_address : Register<0x8, 16>
	{
		struct ADD 		: Bitfield<0,10> {};
	};

	struct I2C_data : Register<0xC, 16>
	{
		struct DATA 		: Bitfield<0,8> {};
	};

	struct Interrupt_status : Register<0x10, 16>
	{
		struct ARB_LOST 	: Bitfield<9,1> {};
		struct RX_UNF 		: Bitfield<7,1> {};
		struct TX_OVF 		: Bitfield<6,1> {};
		struct RX_OVF 		: Bitfield<5,1> {};
		struct SLV_RDY 		: Bitfield<4,1> {};
		struct TO 		: Bitfield<3,1> {};
		struct NACK 		: Bitfield<2,1> {};
		struct DATA 		: Bitfield<1,1> {};
		struct COMP 		: Bitfield<0,1> {};
	};

	struct Transfer_size : Register<0x14, 8>
	{
		struct SIZE		: Bitfield<0,8> {};
	};

	struct Slave_mon_pause : Register<0x18, 8>
	{
		struct PAUSE 		: Bitfield<0,4> {};
	};

	struct Time_out : Register<0x1C, 8>
	{
		struct TO 		: Bitfield<0,8> {};
	};

	struct Interrupt_mask : Register<0x20, 16>
	{
		struct ARB_LOST 	: Bitfield<9,1> {};
		struct RX_UNF 		: Bitfield<7,1> {};
		struct TX_OVF 		: Bitfield<6,1> {};
		struct RX_OVF 		: Bitfield<5,1> {};
		struct SLV_RDY 		: Bitfield<4,1> {};
		struct TO 		: Bitfield<3,1> {};
		struct NACK 		: Bitfield<2,1> {};
		struct DATA 		: Bitfield<1,1> {};
		struct COMP 		: Bitfield<0,1> {};
	};

	struct Interrupt_enable : Register<0x24, 16>
	{
		struct ARB_LOST 	: Bitfield<9,1> {};
		struct RX_UNF 		: Bitfield<7,1> {};
		struct TX_OVF 		: Bitfield<6,1> {};
		struct RX_OVF 		: Bitfield<5,1> {};
		struct SLV_RDY 		: Bitfield<4,1> {};
		struct TO 		: Bitfield<3,1> {};
		struct NACK 		: Bitfield<2,1> {};
		struct DATA 		: Bitfield<1,1> {};
		struct COMP 		: Bitfield<0,1> {};
	};

	struct Interrupt_disable : Register<0x28, 16>
	{
		struct ARB_LOST 	: Bitfield<9,1> {};
		struct RX_UNF 		: Bitfield<7,1> {};
		struct TX_OVF 		: Bitfield<6,1> {};
		struct RX_OVF 		: Bitfield<5,1> {};
		struct SLV_RDY 		: Bitfield<4,1> {};
		struct TO 		: Bitfield<3,1> {};
		struct NACK 		: Bitfield<2,1> {};
		struct DATA 		: Bitfield<1,1> {};
		struct COMP 		: Bitfield<0,1> {};
	};


	Timer::Connection _timer;
	int sendByteCount;
	uint8_t *sendBufferPtr;

	void init(int direction)
	{
		write<Control>(	Control::divisor_a::bits(2) |
				Control::divisor_b::bits(16) |
				Control::ACK_EN::bits(1) |
				Control::CLR_FIFO::bits(1) |
				Control::NEA::bits(1) |
				Control::MS::bits(1));
	write<Control::RW>(direction);
	}

	void transmitFifoFill()
	{
		uint8_t availBytes;
		int loopCnt;
		int numBytesToSend;

		/*
		 * Determine number of bytes to write to FIFO.
		 */
		availBytes = I2C_FIFO_DEPTH - read<Transfer_size::SIZE>();
		numBytesToSend = sendByteCount > availBytes ? availBytes : sendByteCount;

		/*
		 * Fill FIFO with amount determined above.
		 */
		for (loopCnt = 0; loopCnt < numBytesToSend; loopCnt++) 
		{
			write<I2C_data::DATA>(*sendBufferPtr);
			sendBufferPtr++;
			sendByteCount--;
		}
	}

	int i2c_write(uint8_t slaveAddr, uint8_t *msgPtr, int byteCount)
	{
		uint32_t intrs, intrStatusReg;

		sendByteCount = byteCount;
		sendBufferPtr = msgPtr;

		/*
		 * Set HOLD bit if byteCount is bigger than FIFO.
		 */
		if (byteCount > I2C_FIFO_DEPTH) write<Control::HOLD>(1);

		/*
		 * Init sending master.
		 */
		init(SENDING);

		/*
		 * intrs keeps all the error-related interrupts.
		 */
		intrs = INTERRUPT_ARB_LOST_MASK | INTERRUPT_TX_OVR_MASK | INTERRUPT_NACK_MASK;
		
		/*
		 * Clear the interrupt status register before use it to monitor.
		 */
		write<Interrupt_status>(read<Interrupt_status>());

		/*
		 * Transmit first FIFO full of data.
		 */
		transmitFifoFill();
		write<I2C_address::ADD>(slaveAddr);
		intrStatusReg = read<Interrupt_status>();
			
		/*
		 * Continue sending as long as there is more data and there are no errors.
		 */
		while ((sendByteCount > 0) && ((intrStatusReg & intrs) == 0)) 
		{
			/*
			 * Wait until transmit FIFO is empty.
			 */
			if (read<Status::TXDV>() != 0) 
			{
				intrStatusReg = read<Interrupt_status>();
				continue;
			}

			/*
			 * Send more data out through transmit FIFO.
			 */
			transmitFifoFill();
		}

		/*
		 * Check for completion of transfer.
		 */
		while ((intrStatusReg & INTERRUPT_COMP_MASK) != INTERRUPT_COMP_MASK)
		{

			intrStatusReg = read<Interrupt_status>();
			/*
			 * If there is an error, tell the caller.
			 */
			if ((intrStatusReg & intrs) != 0) 
			{
				return 1;
			}
		}

		write<Control::HOLD>(0);

		return 0;
	}

	int i2c_read_byte(uint8_t slaveAddr, uint8_t *msgPtr)
	{
		uint32_t intrs, intrStatusReg;

		/*
		 * Init receiving master.
		 */
		init(RECEIVING);

		/*
		 * Clear the interrupt status register before use it to monitor.
		 */
		write<Interrupt_status>(read<Interrupt_status>());

		/*
		 * Set up the transfer size register so the slave knows how much
		 * to send to us.
		 */
		write<Transfer_size::SIZE>(1);

		/*
		 * Set slave address.
		 */
		write<I2C_address::ADD>(slaveAddr);

		/*
		 * intrs keeps all the error-related interrupts.
		 */
		intrs = INTERRUPT_ARB_LOST_MASK | INTERRUPT_RX_OVR_MASK | 
			INTERRUPT_RX_UNF_MASK | INTERRUPT_NACK_MASK;

		/*
		 * Poll the interrupt status register to find the errors.
		 */
		intrStatusReg = read<Interrupt_status>();

		while (read<Status::RXDV>() != 1)
		{
			if (intrStatusReg & intrs) return 1;
		}

		if (intrStatusReg & intrs) return 1;

		*msgPtr = read<I2C_data::DATA>();

		while (read<Interrupt_status::COMP>() != 1) {}

		return 0;
	}

};

#endif // _I2C_H_
