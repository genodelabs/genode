/*
 * \brief  Block interface for ATA driver
 * \author Stefan Kalkowski <stefan.kalkowski@genode-labs.com>
 * \author Sebastian Sumpf  <sebastian.sumpf@genode-labs.com>
 * \date   2010-07-16
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <os/config.h>
#include <os/server.h>

/* local includes */
#include "ata_device.h"
#include "mindrvr.h"

using namespace Genode;
using namespace Ata;

struct Factory : Block::Driver_factory
{
	Ata::Device *device;

	Factory() : device(0)
	{
		/* determine if we probe for ATA or ATAPI */
		bool ata = false;
		try {
			ata = config()->xml_node().attribute("ata").has_value("yes");
		} catch (...) {}

		int type = ata ? REG_CONFIG_TYPE_ATA : REG_CONFIG_TYPE_ATAPI;

		/*
		 * Probe for ATA(PI) device, but only once
		 */
		device = Ata::Device::probe_legacy(type);
		device->read_capacity();
	}

	Block::Driver *create()
	{
		if (!device) {
			PERR("No device present");
			throw Root::Unavailable();
		}

		if (Atapi_device *atapi_device = dynamic_cast<Atapi_device *>(device))
			if (!atapi_device->test_unit_ready()) {
				PERR("No disc present");
				throw Root::Unavailable();
			}

		return device;
	}

	void destroy(Block::Driver *driver) { }
};


struct Main
{
	Server::Entrypoint &ep;
	struct Factory      factory;
	Block::Root         root;

	Main(Server::Entrypoint &ep)
	: ep(ep), root(ep, Genode::env()->heap(), factory) {
		Genode::env()->parent()->announce(ep.manage(root)); }
};


/************
 ** Server **
 ************/

namespace Server {
	char const *name()             { return "atapi_ep";          }
	size_t stack_size()            { return 2*1024*sizeof(long); }
	void construct(Entrypoint &ep) { static Main server(ep);     }
}
