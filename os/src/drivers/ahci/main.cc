/*
 * \brief  Minimal AHCI-ATA driver
 * \author Sebastian Sumpf <Sebastian.Sumpf@genode-labs.com>
 * \date   2011-08-10
 *
 * This driver currently supports only one command slot, one FIS, and one PRD
 * per FIS, thus limiting the request size to 4MB per request. Since the packet
 * interface currently only supports a synchronous mode of operation the above
 * limitations seems reasonable.
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <block/component.h>
#include <os/server.h>

/* local includes */
#include <ahci_driver.h>

using namespace Genode;


struct Main
{
	Server::Entrypoint &ep;

	struct Factory : Block::Driver_factory
	{
		Block::Driver *create() {
			return new(env()->heap()) Ahci_driver(); }

		void destroy(Block::Driver *driver) {
			Genode::destroy(env()->heap(), static_cast<Ahci_driver *>(driver)); }
	} factory;

	Block::Root root;

	Main(Server::Entrypoint &ep)
	: ep(ep), root(ep, Genode::env()->heap(), factory)
	{
		printf("--- AHCI driver started ---\n");

		Genode::env()->parent()->announce(ep.manage(root));
	}
};


/************
 ** Server **
 ************/

namespace Server {
	char const *name()             { return "ahci_ep";           }
	size_t stack_size()            { return 2*1024*sizeof(long); }
	void construct(Entrypoint &ep) { static Main server(ep);     }
}

