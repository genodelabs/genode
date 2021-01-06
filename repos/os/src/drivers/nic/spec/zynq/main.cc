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
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <drivers/defs/zynq.h>
#include <nic/root.h>

/* NIC driver includes */
#include <drivers/nic/mode.h>

/* local includes */
#include "cadence_gem.h"

namespace Server {
	using namespace Genode;

	class Gem_session_component;
	struct Main;
}


Nic::Mac_address
read_mac_addr_from_config(Genode::Attached_rom_dataspace &config_rom)
{
	Nic::Mac_address mac_addr;

	/* fall back to fake MAC address (unicast, locally managed) */
	mac_addr.addr[0] = 0x02;
	mac_addr.addr[1] = 0x00;
	mac_addr.addr[2] = 0x00;
	mac_addr.addr[3] = 0x00;
	mac_addr.addr[4] = 0x00;
	mac_addr.addr[5] = 0x01;

	/* try using configured MAC address */
	try {
		Genode::Xml_node nic_config = config_rom.xml().sub_node("nic");
		mac_addr = nic_config.attribute_value("mac", mac_addr);
		Genode::log("Using configured MAC address ", mac_addr);
	} catch (...) { }

	return mac_addr;
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
			            Zynq::EMAC_0_MMIO_BASE,
			            Zynq::EMAC_0_MMIO_SIZE,
			            Zynq::EMAC_0_IRQ),
			_config_rom(env, "config")
		{
			mac_address(read_mac_addr_from_config(_config_rom));
		}
};


struct Server::Main
{
	Env                                              &_env;
	Heap                                              _heap          { _env.ram(), _env.rm() };
	Constructible<Nic::Root<Gem_session_component> >  _nic_root      { };
	Constructible<Uplink_client>                      _uplink_client { };

	Main(Env &env) : _env(env)
	{
		Attached_rom_dataspace config_rom { _env, "config" };
		Nic_driver_mode const mode {
			read_nic_driver_mode(config_rom.xml()) };

		switch (mode) {
		case Nic_driver_mode::NIC_SERVER:

			_nic_root.construct(_env, _heap );
			_env.parent().announce(_env.ep().manage(*_nic_root));
			break;

		case Nic_driver_mode::UPLINK_CLIENT:

			_uplink_client.construct(
				_env, _heap, Zynq::EMAC_0_MMIO_BASE, Zynq::EMAC_0_MMIO_SIZE,
				Zynq::EMAC_0_IRQ, read_mac_addr_from_config(config_rom));

			break;
		}
	}
};


void Component::construct(Genode::Env &env) { static Server::Main main(env); }
