/*
 * \brief  Exynos5-specific implementation of the Block::Driver interface
 * \author Sebastian Sumpf
 * \date   2013-03-22
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _DRIVERS__SD_CARD__SPEC__EXYNOS5__DRIVER_H_
#define _DRIVERS__SD_CARD__SPEC__EXYNOS5__DRIVER_H_

/* Genode includes */
#include <util/mmio.h>
#include <os/attached_io_mem_dataspace.h>
#include <base/log.h>
#include <timer_session/connection.h>
#include <os/server.h>
#include <regulator_session/connection.h>

/* local includes */
#include <dwmmc.h>

namespace Block {
	using namespace Genode;
	class Sdhci_driver;
}


class Block::Sdhci_driver : public Block::Driver,
                            public Sd_ack_handler
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

				Timer::Connection::usleep(us);
			}
		} _delayer;

		enum {
			MSH_BASE = 0x12200000, /* host controller base for eMMC */
			MSH_SIZE = 0x10000,
		};

		struct Clock_regulator
		{
			Regulator::Connection regulator;

			Clock_regulator(Env &env) : regulator(env, Regulator::CLK_MMC0) {
				regulator.state(true); }

		} _clock_regulator;


		/* display sub system registers */
		Attached_io_mem_dataspace _mmio;

		/* mobile storage host controller instance */
		Exynos5_msh_controller _controller;

		bool const _use_dma;

	public:

		Sdhci_driver(Env &env)
		:
			_clock_regulator(env),
			_mmio(MSH_BASE, MSH_SIZE),
			_controller(env.ep(), (addr_t)_mmio.local_addr<void>(),
			            _delayer, *this, true),
			_use_dma(true)
		{
			Sd_card::Card_info const card_info = _controller.card_info();

			Genode::log("SD/MMC card detected");
			Genode::log("capacity: ", card_info.capacity_mb(), " MiB");
		}

		void handle_ack(Block::Packet_descriptor pkt, bool success) {
			ack_packet(pkt, success); }


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

		void read_dma(Block::sector_t    block_number,
		              Genode::size_t     block_count,
		              Genode::addr_t     phys,
		              Packet_descriptor &packet)
		{
			_controller.read_blocks_dma(block_number, block_count, phys, packet);
		}

		void write_dma(Block::sector_t    block_number,
		               Genode::size_t     block_count,
		               Genode::addr_t     phys,
		               Packet_descriptor &packet)
		{
			_controller.write_blocks_dma(block_number, block_count, phys, packet);
		}

		bool dma_enabled() { return _use_dma; }
};

#endif /* _DRIVERS__SD_CARD__SPEC__EXYNOS5__DRIVER_H_ */
