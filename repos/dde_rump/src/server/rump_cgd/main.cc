/**
 * \brief  Block device encryption via cgd
 * \author Josef Soentgen
 * \date   2014-04-11
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <os/server.h>

/* local includes */
#include "block_driver.h"
#include "cgd.h"


struct Main
{
	Server::Entrypoint &ep;

	struct Factory : Block::Driver_factory
	{
		Server::Entrypoint &ep;

		Factory(Server::Entrypoint &ep) : ep(ep) { }

		Block::Driver *create()
		{
			return new (Genode::env()->heap()) Driver(ep);
		}

		void destroy(Block::Driver *driver)
		{
			Genode::destroy(Genode::env()->heap(), driver);
		}

	} factory;

	Block::Root root;

	Main(Server::Entrypoint &ep)
	:
		ep(ep), factory(ep), root(ep, Genode::env()->heap(), factory)
	{
		Genode::env()->parent()->announce(ep.manage(root));
	}
};


/**********************
 ** Server framework **
 **********************/

namespace Server {
	char const *name()                    { return "rump_cgd_ep"; }
	size_t      stack_size()              { return 4 * 1024 * sizeof(long); }
	void        construct(Entrypoint &ep) { static Main inst(ep); }
}
