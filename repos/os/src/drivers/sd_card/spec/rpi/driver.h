/*
 * \brief  Raspberry Pi implementation of the Block::Driver interface
 * \author Norman Feske
 * \date   2014-09-21
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _DRIVERS__SD_CARD__SPEC__RPI__DRIVER_H_
#define _DRIVERS__SD_CARD__SPEC__RPI__DRIVER_H_

#include <util/mmio.h>
#include <os/attached_io_mem_dataspace.h>
#include <base/printf.h>
#include <timer_session/connection.h>
#include <block/component.h>

/* local includes */
#include <sdhci.h>

namespace Block {
	using namespace Genode;
	class Sdhci_driver;
}


class Block::Sdhci_driver : public Block::Driver
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
			SDHCI_BASE = 0x20300000,
			SDHCI_SIZE = 0x100,
			SDHCI_IRQ  = 62,
		};

		/* display sub system registers */
		Attached_io_mem_dataspace _sdhci_mmio { SDHCI_BASE, SDHCI_SIZE };

		/* host-controller instance */
		Sdhci_controller _controller;

		bool const _use_dma;

	public:

		Sdhci_driver(bool use_dma)
		:
			_controller((addr_t)_sdhci_mmio.local_addr<void>(),
			            _delayer, SDHCI_IRQ, use_dma),
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

#endif /* _DRIVERS__SD_CARD__SPEC__RPI__DRIVER_H_ */
