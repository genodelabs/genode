/*
 * \brief  Driver for PL180 multi-media card interface (MMCI)
 * \author Christian Helmuth
 * \date   2011-05-20
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <block/component.h>
#include <os/server.h>

/* local includes */
#include <pl180_defs.h>
#include "pl180.h"
#include "sd_card.h"


struct Main
{
	Server::Entrypoint &ep;

	struct Factory : Block::Driver_factory
	{
		Block::Driver *create()
		{
			Pl180   *pl180   = new (Genode::env()->heap())
			                   Pl180(PL180_PHYS, PL180_SIZE);
			Sd_card *sd_card = new (Genode::env()->heap())
			                   Sd_card(*pl180);

			return sd_card;
		}

		void destroy(Block::Driver *driver)
		{
			Sd_card *sd_card = static_cast<Sd_card *>(driver);
			Pl180   *pl180   = static_cast<Pl180 *>(&sd_card->host_driver());

			Genode::destroy(Genode::env()->heap(), sd_card);
			Genode::destroy(Genode::env()->heap(), pl180);
		}
	} factory;

	Block::Root root;

	Main(Server::Entrypoint &ep)
	: ep(ep), root(ep, Genode::env()->heap(), factory)
	{
		Genode::printf("--- PL180 MMC/SD card driver started ---\n");

		Genode::env()->parent()->announce(ep.manage(root));
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
