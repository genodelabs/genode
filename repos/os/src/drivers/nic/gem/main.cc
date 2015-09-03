/*
 * \brief  EMACPS NIC driver for Xilix Zynq-7000
 * \author Timo Wischer
 * \date   2015-03-10
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/sleep.h>
#include <cap_session/connection.h>
#include <nic/component.h>
#include <drivers/board_base.h>

#include "cadence_gem.h"


int main(int, char **)
{
	using namespace Genode;

	printf("--- Xilinx Ethernet MAC PS NIC driver started ---\n");

	/**
	 * Factory used by 'Nic::Root' at session creation/destruction time
	 */
	struct Emacps_driver_factory : Nic::Driver_factory
	{
		Nic::Driver *create(Nic::Rx_buffer_alloc &alloc,
		                    Nic::Driver_notification &notify)
		{
			return new (env()->heap())
				Cadence_gem(Board_base::EMAC_0_MMIO_BASE,
								   Board_base::EMAC_0_MMIO_SIZE,
								   Board_base::EMAC_0_IRQ,
								   alloc, notify);
		}

		void destroy(Nic::Driver *driver)
		{
			Genode::destroy(env()->heap(), static_cast<Cadence_gem*>(driver));
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
