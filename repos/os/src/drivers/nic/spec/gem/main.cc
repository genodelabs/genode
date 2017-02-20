/*
 * \brief  EMACPS NIC driver for Xilix Zynq-7000
 * \author Timo Wischer
 * \date   2015-03-10
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
/*
 * Needs to be included first because otherwise
 * util/xml_node.h will not pick up the ascii_to
 * overload.
 */
#include <nic/xml_node.h>

#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <drivers/board_base.h>
#include <nic/root.h>

#include "cadence_gem.h"

namespace Server {
	using namespace Genode;

	class Gem_session_component;
	struct Main;
}

class Server::Gem_session_component : public Cadence_gem
{
	private:

		Genode::Attached_rom_dataspace _config_rom;

	public:

		Gem_session_component(Genode::size_t const tx_buf_size,
		                      Genode::size_t const rx_buf_size,
		                      Genode::Allocator   &rx_block_md_alloc,
		                      Genode::Env         &env)
		:
			Cadence_gem(tx_buf_size, rx_buf_size, rx_block_md_alloc, env,
			            Board_base::EMAC_0_MMIO_BASE,
			            Board_base::EMAC_0_MMIO_SIZE,
			            Board_base::EMAC_0_IRQ),
			_config_rom(env, "config")
		{
			Nic::Mac_address mac_addr;

			/* try using configured MAC address */
			try {
				Genode::Xml_node nic_config = _config_rom.xml().sub_node("nic");
				nic_config.attribute("mac").value(&mac_addr);
				Genode::log("Using configured MAC address ", mac_addr);
			} catch (...) {
				/* fall back to fake MAC address (unicast, locally managed) */
				mac_addr.addr[0] = 0x02;
				mac_addr.addr[1] = 0x00;
				mac_addr.addr[2] = 0x00;
				mac_addr.addr[3] = 0x00;
				mac_addr.addr[4] = 0x00;
				mac_addr.addr[5] = 0x01;
			}

			/* set mac address */
			mac_address(mac_addr);
		}
};


struct Server::Main
{
	Env  &_env;
	Heap  _heap { _env.ram(), _env.rm() };

	Nic::Root<Gem_session_component> nic_root{ _env, _heap };

	Main(Env &env) : _env(env)
	{
		_env.parent().announce(_env.ep().manage(nic_root));
	}
};


void Component::construct(Genode::Env &env) { static Server::Main main(env); }
