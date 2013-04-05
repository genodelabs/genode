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
#include <cap_session/connection.h>
#include <block/component.h>
#include <base/sleep.h>

/* local includes */
#include <ahci_driver.h>

using namespace Genode;

int main()
{
	printf("--- AHCI driver started ---\n");

	struct Ahci_driver_factory : Block::Driver_factory
	{
		Block::Driver *create() {
			return new(env()->heap()) Ahci_driver(); }

		void destroy(Block::Driver *driver) {
			Genode::destroy(env()->heap(), static_cast<Ahci_driver *>(driver)); }
	} driver_factory;

	enum { STACK_SIZE = 8128 };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "block_ep");

	static Block::Root block_root(&ep, env()->heap(), driver_factory);
	env()->parent()->announce(ep.manage(&block_root));

	sleep_forever();
	return 0;
}

