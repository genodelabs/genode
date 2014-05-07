/*
 * \brief  LAN9118 NIC driver
 * \author Norman Feske
 * \date   2011-05-19
 *
 * Note, this driver is only tested on Qemu. At the current stage, it is not
 * expected to function properly on hardware.
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/sleep.h>
#include <cap_session/connection.h>
#include <nic/component.h>

/* device definitions */
#include <lan9118_defs.h>

/* driver code */
#include <lan9118.h>


int main(int, char **)
{
	using namespace Genode;

	printf("--- LAN9118 NIC driver started ---\n");

	/**
	 * Factory used by 'Nic::Root' at session creation/destruction time
	 */
	struct Lan9118_driver_factory : Nic::Driver_factory
	{
		Nic::Driver *create(Nic::Rx_buffer_alloc &alloc)
		{
			return new (env()->heap())
				Lan9118(LAN9118_PHYS, LAN9118_SIZE, LAN9118_IRQ, alloc);
		}

		void destroy(Nic::Driver *driver)
		{
			Genode::destroy(env()->heap(), static_cast<Lan9118 *>(driver));
		}

	} driver_factory;

	enum { STACK_SIZE = 4096 };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "nic_ep");

	static Nic::Root nic_root(&ep, env()->heap(), driver_factory);
	env()->parent()->announce(ep.manage(&nic_root));

	Genode::sleep_forever();
	return 0;
}
