/*
 * \brief  eMMC driver for Arndale/Exynos5 platform
 * \author Sebastian Sumpf
 * \date   2013-03-06
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <regulator_session/connection.h>
#include <os/server.h>

/* local includes */
#include <driver.h>


struct Main
{
	Server::Entrypoint &ep;

	struct Factory : Block::Driver_factory
	{
		Block::Driver *create() {
			return new (Genode::env()->heap()) Block::Exynos5_driver(true); }

		void destroy(Block::Driver *driver) {
			Genode::destroy(Genode::env()->heap(),
			                static_cast<Block::Exynos5_driver *>(driver)); }
	} factory;

	Regulator::Connection regulator;
	Block::Root           root;

	Main(Server::Entrypoint &ep)
	: ep(ep), regulator(Regulator::CLK_MMC0),
	  root(ep, Genode::env()->heap(), factory)
	{
		Genode::printf("--- Arndale eMMC card driver ---\n");

		Genode::env()->parent()->announce(ep.manage(root));
		regulator.state(true);
	}
};


/************
 ** Server **
 ************/

namespace Server {
	char const *name()             { return "sd_card_ep";        }
	size_t stack_size()            { return 2*1024*sizeof(long); }
	void construct(Entrypoint &ep) { static Main server(ep);     }
}
