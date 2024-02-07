/*
 * \brief  NIC driver based on iPXE
 * \author Christian Helmuth
 * \author Sebastian Sumpf
 * \date   2011-11-17
 */

/*
 * Copyright (C) 2011-2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/component.h>
#include <base/env.h>
#include <base/heap.h>
#include <base/log.h>
#include <uplink_session/connection.h>
#include <base/attached_rom_dataspace.h>
#include <os/reporter.h>

/* NIC driver includes */
#include <drivers/nic/uplink_client_base.h>

/* DDE iPXE includes */
#include <dde_ipxe/support.h>
#include <dde_ipxe/nic.h>

using namespace Genode;


class Uplink_client : public Uplink_client_base
{
	public:

		static Uplink_client *instance;

	private:

		Net::Mac_address _init_drv_mac_addr()
		{
			instance = this;
			dde_ipxe_nic_register_callbacks(
				_drv_rx_callback, _drv_link_callback, _drv_rx_done);

			Net::Mac_address mac_addr { };
			dde_ipxe_nic_get_mac_addr(1, mac_addr.addr);
			return mac_addr;
		}


		/***********************************
		 ** Interface towards iPXE driver **
		 ***********************************/

		static void _drv_rx_done()
		{
			instance->_rx_done();
		}

		static void _drv_rx_callback(unsigned    interface_idx,
		                             const char *drv_rx_pkt_base,
		                             unsigned    drv_rx_pkt_size)
		{
			instance->_drv_rx_handle_pkt_try(drv_rx_pkt_size,
			                                 [&] (void   *conn_tx_pkt_base,
			                                      size_t &)
			{
				memcpy(conn_tx_pkt_base, drv_rx_pkt_base, drv_rx_pkt_size);
				return Write_result::WRITE_SUCCEEDED;
			});
		}

		static void _drv_link_callback()
		{
			instance->_drv_handle_link_state(dde_ipxe_nic_link_state(1));
		}


		/************************
		 ** Uplink_client_base **
		 ************************/

		Transmit_result
		_drv_transmit_pkt(const char *conn_rx_pkt_base,
		                  size_t      conn_rx_pkt_size) override
		{
			if (dde_ipxe_nic_tx(1, conn_rx_pkt_base, conn_rx_pkt_size) == 0) {

				return Transmit_result::ACCEPTED;
			}
			return Transmit_result::REJECTED;
		}

	public:

		Uplink_client(Env       &env,
		              Allocator &alloc)
		:
			Uplink_client_base { env, alloc, _init_drv_mac_addr() }
		{
			_drv_handle_link_state(dde_ipxe_nic_link_state(1));
		}

		~Uplink_client()
		{
			dde_ipxe_nic_unregister_callbacks();
			instance = nullptr;
		}

		Net::Mac_address mac_address() const { return _drv_mac_addr; }
};


Uplink_client *Uplink_client::instance;


struct Main
{
	Env                         &_env;
	Heap                         _heap       { _env.ram(), _env.rm() };
	Attached_rom_dataspace       _config_rom { _env, "config" };
	Constructible<Uplink_client> _uplink     { };
	Constructible<Reporter>      _reporter   { };

	Main(Env &env) : _env(env)
	{
		log("--- iPXE NIC driver started ---");

		dde_support_init(_env, _heap);

		if (!dde_ipxe_nic_init()) {
			error("could not find usable NIC device");
		}

		_uplink.construct(_env, _heap);

		_config_rom.xml().with_optional_sub_node("report", [&] (Xml_node const &xml) {
			bool const report_mac_address =
				xml.attribute_value("mac_address", false);

			if (!report_mac_address)
				return;

			_reporter.construct(_env, "devices");
			_reporter->enabled(true);

			Reporter::Xml_generator report(*_reporter, [&] () {
				report.node("nic", [&] () {
					report.attribute("mac_address", String<32>(_uplink->mac_address()));
				});
			});
		});
	}
};


void Component::construct(Env &env) { static Main main(env); }
