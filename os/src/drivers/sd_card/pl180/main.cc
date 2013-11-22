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

#include <base/printf.h>

#include <cap_session/connection.h>
#include <block/component.h>
#include <pl180_defs.h>

#include "pl180.h"
#include "sd_card.h"


int main(int argc, char **argv)
{
	using namespace Genode;

	printf("--- PL180 MMC/SD card driver started ---\n");

	/**
	 * Factory used by 'Block::Root' at session creation/destruction time
	 */
	struct Pl180_driver_factory : Block::Driver_factory
	{
		Block::Driver *create()
		{
			Pl180   *pl180   = new (env()->heap())
			                   Pl180(PL180_PHYS, PL180_SIZE);
			Sd_card *sd_card = new (env()->heap())
			                   Sd_card(*pl180);

			return sd_card;
		}

		void destroy(Block::Driver *driver)
		{
			Sd_card *sd_card = static_cast<Sd_card *>(driver);
			Pl180   *pl180   = static_cast<Pl180 *>(&sd_card->host_driver());

			Genode::destroy(env()->heap(), sd_card);
			Genode::destroy(env()->heap(), pl180);
		}

	} driver_factory;

	enum { STACK_SIZE = 4096 };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "block_ep");

	static Signal_receiver receiver;
	static Block::Root block_root(&ep, env()->heap(), driver_factory, receiver);
	env()->parent()->announce(ep.manage(&block_root));

	while (true) {
		Signal s = receiver.wait_for_signal();
		static_cast<Signal_dispatcher_base *>(s.context())->dispatch(s.num());
	}

	return 0;
}
