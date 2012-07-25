/*
 * \brief  OMAP4-specific implementation of the Block::Driver interface
 * \author Norman Feske
 * \date   2012-07-19
 */

#ifndef _DRIVER_H_
#define _DRIVER_H_

#include <util/mmio.h>
#include <os/attached_io_mem_dataspace.h>
#include <base/printf.h>
#include <timer_session/connection.h>
#include <block/component.h>

/* local includes */
#include <mmchs.h>

namespace Block {
	using namespace Genode;
	class Omap4_driver;
}


class Block::Omap4_driver : public Block::Driver
{
	private:

		struct Timer_delayer : Timer::Connection, Mmio::Delayer
		{
			/**
			 * Implementation of 'Delayer' interface
			 */
			void usleep(unsigned us)
			{
				/* polling */
				if (us == 0)
					return;

				unsigned ms = us / 1000;
				if (ms == 0)
					ms = 1;

				Timer::Connection::msleep(ms);
			}
		} _delayer;

		/* memory map */
		enum {
			MMCHS1_MMIO_BASE = 0x4809c000,
			MMCHS1_MMIO_SIZE = 0x00001000,
		};

		/* display sub system registers */
		Attached_io_mem_dataspace _mmchs1_mmio;

		/* hsmmc controller instance */
		Omap4_hsmmc_controller _controller;

		bool const _use_dma;

	public:

		Omap4_driver(bool use_dma)
		:
			_mmchs1_mmio(MMCHS1_MMIO_BASE, MMCHS1_MMIO_SIZE),
			_controller((addr_t)_mmchs1_mmio.local_addr<void>(),
			            _delayer, use_dma),
			_use_dma(use_dma)
		{
			Sd_card::Card_info const card_info = _controller.card_info();

			PLOG("SD card detected");
			PLOG("capacity: %zd MiB", card_info.capacity_mb());
		}


		/*****************************
		 ** Block::Driver interface **
		 *****************************/

		Genode::size_t block_size() { return 512; }

		virtual Genode::size_t block_count()
		{
			return _controller.card_info().capacity_mb() * 1024 * 2;
		}

		void read(Genode::size_t  block_number,
		          Genode::size_t  block_count,
		          char           *out_buffer)
		{
			if (!_controller.read_blocks(block_number, block_count, out_buffer))
				throw Io_error();
		}

		void write(Genode::size_t  block_number,
		           Genode::size_t  block_count,
		           char const     *buffer)
		{
			if (!_controller.write_blocks(block_number, block_count, buffer))
				throw Io_error();
		}

		void read_dma(Genode::size_t block_number,
		              Genode::size_t block_count,
		              Genode::addr_t phys)
		{
			if (!_controller.read_blocks_dma(block_number, block_count, phys))
				throw Io_error();
		}

		void write_dma(Genode::size_t  block_number,
		               Genode::size_t  block_count,
		               Genode::addr_t  phys)
		{
			if (!_controller.write_blocks_dma(block_number, block_count, phys))
				throw Io_error();
		}

		bool dma_enabled() { return _use_dma; }
};

#endif /* _DRIVER_H_ */
