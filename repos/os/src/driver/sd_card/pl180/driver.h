/*
 * \brief  PL180-specific implementation of the Block::Driver interface
 * \author Christian Helmuth
 * \author Martin Stein
 * \date   2011-05-19
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVER_H_
#define _DRIVER_H_

/* local includes */
#include <block/driver.h>
#include <platform_session/device.h>
#include <timer_session/connection.h>

namespace Sd_card {

	using namespace Genode;

	class Driver;
}


class Sd_card::Driver : public  Block::Driver,
                        private Platform::Device,
                        private Platform::Device::Mmio<0>
{
	private:

		/*
		 * Noncopyable
		 */
		Driver(Driver const &);
		Driver &operator = (Driver const &);

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

		Platform::Connection & _platform;
		Timer::Connection      _timer;

		uint32_t volatile *_base { local_addr<unsigned volatile>() };

		uint32_t _read_reg(Register reg) const { return _base[reg >> 2]; }

		void _write_reg(Register reg, uint32_t v) { _base[reg >> 2] = v; }

		void _clear_status() { _write_reg(Clear, ~0); }

		void _request(unsigned char  cmd,
		              unsigned      *out_resp);

		void _request(unsigned char  cmd,
		              unsigned       arg,
		              unsigned      *out_resp);

		void _read_request(unsigned char  cmd,
		                   unsigned       arg,
		                   unsigned       length,
		                   unsigned      *out_resp);

		void _write_request(unsigned char  cmd,
		                    unsigned       arg,
		                    unsigned       length,
		                    unsigned      *out_resp);

		void _read_data(unsigned length, char *out_buffer);
		void _write_data(unsigned length, char const *buffer);
		void _write_command(unsigned cmd_index, bool resp);

		/*
		 * TODO report (and support) real capacity not just 512M
		 */
		size_t          const _block_size  = 512;
		Block::sector_t const _block_count = 0x20000000 / _block_size;

	public:

		Driver(Env &env, Platform::Connection & platform);
		~Driver();


		/******************
		 ** Block-driver **
		 ******************/

		Dma_buffer alloc_dma_buffer(size_t size, Cache cache) override
		{
			Ram_dataspace_capability ds =
				_platform.retry_with_upgrade(Ram_quota{4096}, Cap_quota{2},
					[&] () { return _platform.alloc_dma_buffer(size, cache); });

			return { .ds       = ds,
			         .dma_addr = _platform.dma_addr(ds) };
		}

		Block::Session::Info info() const override
		{
			return { .block_size  = _block_size,
			         .block_count = _block_count,
			         .align_log2  = log2(_block_size),
			         .writeable   = true };
		}

		void read(Block::sector_t           block_number,
		          size_t                    block_count,
		          char                     *buffer,
		          Block::Packet_descriptor &packet) override;

		void write(Block::sector_t           block_number,
		           size_t                    block_count,
		           char const               *buffer,
		           Block::Packet_descriptor &packet) override;

};

#endif /* _DRIVER_H_ */
