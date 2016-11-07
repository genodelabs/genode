/*
 * \brief  OMAP4-specific implementation of the Block::Driver interface
 * \author Norman Feske
 * \date   2012-07-19
 */

/*
 * Copyright (C) 2012-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _DRIVERS__SD_CARD__SPEC__OMAP4__DRIVER_H_
#define _DRIVERS__SD_CARD__SPEC__OMAP4__DRIVER_H_

#include <util/mmio.h>
#include <os/attached_io_mem_dataspace.h>
#include <base/log.h>
#include <timer_session/connection.h>
#include <block/component.h>
#include <os/server.h>

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
			void usleep(unsigned us) { Timer::Connection::usleep(us); }
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

			Genode::log("SD card detected");
			Genode::log("capacity: ", card_info.capacity_mb(), " MiB");
		}


		/*****************************
		 ** Block::Driver interface **
		 *****************************/

		Genode::size_t block_size() { return 512; }

		virtual Block::sector_t block_count()
		{
			return _controller.card_info().capacity_mb() * 1024 * 2;
		}

		Block::Session::Operations ops()
		{
			Block::Session::Operations o;
			o.set_operation(Block::Packet_descriptor::READ);
			o.set_operation(Block::Packet_descriptor::WRITE);
			return o;
		}

		void read(Block::sector_t    block_number,
		          Genode::size_t     block_count,
		          char              *out_buffer,
		          Packet_descriptor &packet)
		{
			if (!_controller.read_blocks(block_number, block_count, out_buffer))
				throw Io_error();
			ack_packet(packet);
		}

		void write(Block::sector_t    block_number,
		           Genode::size_t     block_count,
		           char const        *buffer,
		           Packet_descriptor &packet)
		{
			if (!_controller.write_blocks(block_number, block_count, buffer))
				throw Io_error();
			ack_packet(packet);
		}

		void read_dma(Block::sector_t    block_number,
		              Genode::size_t     block_count,
		              Genode::addr_t     phys,
		              Packet_descriptor &packet)
		{
			if (!_controller.read_blocks_dma(block_number, block_count, phys))
				throw Io_error();
			ack_packet(packet);
		}

		void write_dma(Block::sector_t    block_number,
		               Genode::size_t     block_count,
		               Genode::addr_t     phys,
		               Packet_descriptor &packet)
		{
			if (!_controller.write_blocks_dma(block_number, block_count, phys))
				throw Io_error();
			ack_packet(packet);
		}

		bool dma_enabled() { return _use_dma; }

		Genode::Ram_dataspace_capability alloc_dma_buffer(Genode::size_t size) {
			return Genode::env()->ram_session()->alloc(size, UNCACHED); }

		void free_dma_buffer(Genode::Ram_dataspace_capability c) {
			return Genode::env()->ram_session()->free(c); }
};

#endif /* _DRIVERS__SD_CARD__SPEC__OMAP4__DRIVER_H_ */
