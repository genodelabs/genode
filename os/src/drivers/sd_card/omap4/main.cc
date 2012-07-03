/*
 * \brief  SD-card driver for OMAP4 platform
 * \author Norman Feske
 * \date   2012-07-03
 */

/* Genode includes */
#include <base/sleep.h>
#include <util/mmio.h>
#include <os/attached_io_mem_dataspace.h>
#include <base/printf.h>
#include <timer_session/connection.h>
#include <cap_session/connection.h>
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

	public:

		Omap4_driver();


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
			throw Io_error();
		}

		void write_dma(Genode::size_t  block_number,
		               Genode::size_t  block_count,
		               Genode::addr_t  phys)
		{
			throw Io_error();
		}

		bool dma_enabled() { return false; }
};


Block::Omap4_driver::Omap4_driver()
:
	_mmchs1_mmio(MMCHS1_MMIO_BASE, MMCHS1_MMIO_SIZE),
	_controller((addr_t)_mmchs1_mmio.local_addr<void>(), _delayer)
{
	Sd_card::Card_info const card_info = _controller.card_info();

	PLOG("SD card detected");
	PLOG("capacity: %zd MiB", card_info.capacity_mb());
}


/*
 * MMC1: IRQ 83
 */

int main(int argc, char **argv)
{
	using namespace Genode;

	printf("--- OMAP4 SD card driver ---\n");

	/**
	 * Factory used by 'Block::Root' at session creation/destruction time
	 */
	struct Driver_factory : Block::Driver_factory
	{
		Block::Driver *create()
		{
			return new (env()->heap()) Block::Omap4_driver();
		}

		void destroy(Block::Driver *driver)
		{
			Genode::destroy(env()->heap(),
			                static_cast<Block::Omap4_driver *>(driver));
		}

	} driver_factory;

	enum { STACK_SIZE = 4096 };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "block_ep");

	static Block::Root block_root(&ep, env()->heap(), driver_factory);
	env()->parent()->announce(ep.manage(&block_root));

	sleep_forever();
	return 0;
}
