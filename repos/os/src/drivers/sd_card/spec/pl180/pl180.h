/*
 * \brief  PL180 MMCI driver
 * \author Christian Helmuth
 * \date   2011-05-19
 */

/*
 * Copyright (C) 2011-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _DRIVERS__SD_CARD__SPEC__PL180__PL180_H_
#define _DRIVERS__SD_CARD__SPEC__PL180__PL180_H_

#include <base/log.h>
#include <os/attached_io_mem_dataspace.h>
#include <timer_session/connection.h>

#include "host_driver.h"

class Pl180 : public Host_driver
{
	private:

		/*
		 * Constants for flags, registers, etc. only visible in this file are
		 * exceptionally named in mixed case to relieve correlation between
		 * implementation and specification.
		 */

		/**
		 * Register offsets
		 *
		 * Registers are read/writable unless explicitly stated.
		 */
		enum Register {
			Power      = 0x000,  /* power control */
			Argument   = 0x008,  /* argument for command */
			Command    = 0x00c,  /* command index and type */
			Response0  = 0x014,  /* command response (card status, read only) */
			DataLength = 0x028,  /* number of bytes in data transfer (block size) */
			DataCtrl   = 0x02c,  /* data transfer control */
			Status     = 0x034,  /* controller status flags (read only) */
			Clear      = 0x038,  /* status clear (write only) */
			Mask0      = 0x03c,  /* interrupt 0 mask */
			Mask1      = 0x040,  /* interrupt 1 mask */
			FifoCnt    = 0x048,  /* data FIFO counter (in words, read only) */
			FIFO       = 0x080,  /* data FIFO */
		};

		/**
		 * Flags
		 */
		enum Flag {
			CmdCrcFail      = 0x000001,  /* command response received (CRC failed) */
			DataCrcFail     = 0x000002,  /* data block sent/received (CRC failed) */
			CmdTimeOut      = 0x000004,  /* command response timeout */
			DataTimeOut     = 0x000008,  /* data timeout */
			TxUnderrun      = 0x000010,  /* tx fifo underrun */
			RxUnderrun      = 0x000020,  /* rx fifo underrun */
			CmdRespEnd      = 0x000040,  /* command response received (CRC ok) */
			CmdSent         = 0x000080,  /* command sent (no response required) */
			DataEnd         = 0x000100,  /* data counter zero */
			StartBitErr     = 0x000200,  /* start bit not detected */
			DataBlockEnd    = 0x000400,  /* data block sent/received (CRC ok) */
			CmdActive       = 0x000800,  /* command transfer in progress */
			TxActive        = 0x001000,  /* data tx in progress */
			RxActive        = 0x002000,  /* data rx in progress */
			TxFifoHalfEmpty = 0x004000,
			RxFifoHalfFull  = 0x008000,
			TxFifoFull      = 0x010000,
			RxFifoFull      = 0x020000,
			TxFifoEmpty     = 0x040000,
			RxFifoEmpty     = 0x080000,
			TxDataAvlbl     = 0x100000,
			RxDataAvlbl     = 0x200000,
		};

		Timer::Connection                  _timer;
		Genode::Attached_io_mem_dataspace  _io_mem;
		Genode::uint32_t volatile         *_base;

		Genode::uint32_t _read_reg(Register reg) const
		{
			return _base[reg >> 2];
		}

		void _write_reg(Register reg, Genode::uint32_t v)
		{
			_base[reg >> 2] = v;
		}

		void _write_command(unsigned cmd_index, bool resp)
		{
			enum Bits {
				CmdIndex = 0x3f,
				Response = 1 << 6,
				Enable   = 1 << 10
			};

			cmd_index  = (cmd_index & CmdIndex) | Enable;
			cmd_index |= (resp ? Response : 0);

			_write_reg(Command, cmd_index);

			while (!_read_reg(Status) & (CmdRespEnd | CmdSent)) ;
		}

		void _clear_status() { _write_reg(Clear, ~0); }

	public:

		Pl180(Genode::addr_t mmio_base, Genode::size_t mmio_size)
		:
			_io_mem(mmio_base, mmio_size),
			_base(_io_mem.local_addr<unsigned volatile>())
		{
			enum { POWER_UP = 2, POWER_ON = 3 };

			_write_reg(Power, POWER_UP);
			_timer.msleep(10);
			_write_reg(Power, POWER_ON);
			_timer.msleep(10);
			_clear_status();
		}


		/***************************
		 ** Host driver interface **
		 ***************************/

		void request(unsigned char  cmd,
		             unsigned      *out_resp)
		{
			_write_reg(Argument, 0);
			_write_command(cmd, (out_resp != 0));
			if (out_resp)
				*out_resp = _read_reg(Response0);
			_clear_status();
		}

		void request(unsigned char  cmd,
		             unsigned       arg,
		             unsigned      *out_resp)
		{
			_write_reg(Argument, arg);
			_write_command(cmd, (out_resp != 0));
			if (out_resp)
				*out_resp = _read_reg(Response0);
			_clear_status();
		}

		void read_request(unsigned char  cmd,
		                  unsigned       arg,
		                  unsigned       length,
		                  unsigned      *out_resp)
		{
			/*
			 * FIXME on real hardware the blocksize must be written into
			 * DataCtrl:BlockSize.
			 */
			enum { CTRL_ENABLE = 0x01, CTRL_READ = 0x02 };

			_write_reg(DataLength, length);
			_write_reg(DataCtrl, CTRL_ENABLE | CTRL_READ);

			_write_reg(Argument, arg);
			_write_command(cmd, (out_resp != 0));
			if (out_resp)
				*out_resp = _read_reg(Response0);
			_clear_status();
		}

		void write_request(unsigned char  cmd,
		                   unsigned       arg,
		                   unsigned       length,
		                   unsigned      *out_resp)
		{
			/*
			 * FIXME on real hardware the blocksize must be written into
			 * DataCtrl:BlockSize.
			 */
			enum { CTRL_ENABLE = 0x01 };

			_write_reg(DataLength, length);
			_write_reg(DataCtrl, CTRL_ENABLE);

			_write_reg(Argument, arg);
			_write_command(cmd, (out_resp != 0));
			if (out_resp)
				*out_resp = _read_reg(Response0);
			_clear_status();
		}

		void read_data(unsigned  length,
		               char     *out_buffer)
		{
			unsigned *buf = reinterpret_cast<unsigned *>(out_buffer);
			for (unsigned count = 0; count < length / 4; ) {
				/*
				 * The FIFO contains at least 'remainder - FifoCnt' words.
				 */
				int chunk  = length / 4 - count - _read_reg(FifoCnt);
				count     += chunk;
				for ( ; chunk > 0; --chunk)
					buf[count - chunk] = _read_reg(FIFO);
			}
			_clear_status();
		}

		void write_data(unsigned    length,
		                char const *buffer)
		{
			enum { FIFO_SIZE = 16 };

			unsigned const *buf = reinterpret_cast<unsigned const *>(buffer);
			for (unsigned count = 0; count < length / 4; ) {
				Genode::uint32_t status;
				while (!((status = _read_reg(Status)) & TxFifoHalfEmpty)) ;

				int chunk  = (status & TxFifoEmpty) ? FIFO_SIZE : FIFO_SIZE / 2;
				count     += chunk;
				for ( ; chunk > 0; --chunk)
					_write_reg(FIFO, buf[count - chunk]);
			}
			_clear_status();
		}
};

#endif /* _DRIVERS__SD_CARD__SPEC__PL180__PL180_H_ */
