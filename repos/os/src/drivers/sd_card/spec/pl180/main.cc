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
#include <base/component.h>
#include <base/log.h>
#include <block/component.h>

/* local includes */
#include <pl180_defs.h>
#include "pl180.h"
#include "sd_card.h"


struct Main
{
	Genode::Env &env;
	Genode::Heap heap { env.ram(), env.rm() };

	struct Factory : Block::Driver_factory
	{
		Genode::Entrypoint &ep;
		Genode::Heap       &heap;

		Factory(Genode::Entrypoint &ep, Genode::Heap &heap)
		: ep(ep), heap(heap) { }

		Block::Driver *create() {
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
	} factory { env.ep(), heap };

	Block::Root root { env.ep(), heap, factory  };

	Main(Genode::Env &env) : env(env)
	{
		Genode::log("--- PL180 MMC/SD card driver started ---");

		env.parent().announce(env.ep().manage(root));
	}
};


void Component::construct(Genode::Env &env) { static Main m(env); }
