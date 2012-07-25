/*
 * \brief  SD-card driver for OMAP4 platform
 * \author Norman Feske
 * \date   2012-07-03
 */

/* Genode includes */
#include <base/sleep.h>
#include <base/printf.h>
#include <cap_session/connection.h>

/* local includes */
#include <driver.h>


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
			bool use_dma = true;
			return new (env()->heap()) Block::Omap4_driver(use_dma);
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
