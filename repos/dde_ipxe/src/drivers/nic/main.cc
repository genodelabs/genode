/*
 * \brief  NIC driver based on iPXE
 * \author Christian Helmuth
 * \author Sebastian Sumpf
 * \date   2011-11-17
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/component.h>
#include <base/env.h>
#include <base/heap.h>
#include <base/log.h>
#include <nic/component.h>
#include <nic/root.h>
#include <uplink_session/connection.h>
#include <base/attached_rom_dataspace.h>

/* NIC driver includes */
#include <drivers/nic/uplink_client_base.h>
#include <drivers/nic/mode.h>

/* DDE iPXE includes */
#include <dde_ipxe/support.h>
#include <dde_ipxe/nic.h>

using namespace Genode;

class Ipxe_session_component  : public Nic::Session_component
{
	public:

		static Ipxe_session_component *instance;

	private:

		Nic::Mac_address _mac_addr;

		static void _rx_callback(unsigned    if_index,
		                         const char *packet,
		                         unsigned    packet_len)
		{
			if (instance)
				instance->_receive(packet, packet_len);
		}

		static void _link_callback()
		{
			if (instance)
				instance->_link_state_changed();
		}

		bool _send()
		{
			using namespace Genode;

			if (!_tx.sink()->ready_to_ack())
				return false;

			if (!_tx.sink()->packet_avail())
				return false;

			Packet_descriptor packet = _tx.sink()->get_packet();
			if (!packet.size()) {
				warning("Invalid tx packet");
				return true;
			}

			if (link_state()) {
				if (dde_ipxe_nic_tx(1, _tx.sink()->packet_content(packet), packet.size()))
					warning("Sending packet failed!");
			}

			_tx.sink()->acknowledge_packet(packet);
			return true;
		}

		void _receive(const char *packet, unsigned packet_len)
		{
			_handle_packet_stream();

			if (!_rx.source()->ready_to_submit())
				return;

			try {
				Nic::Packet_descriptor p = _rx.source()->alloc_packet(packet_len);
				memcpy(_rx.source()->packet_content(p), packet, packet_len);
				_rx.source()->submit_packet(p);
			} catch (...) {
				warning(__func__, ": failed to process received packet");
			}
		}

		void _handle_packet_stream() override
		{
			while (_rx.source()->ack_avail())
				_rx.source()->release_packet(_rx.source()->get_acked_packet());

			while (_send()) ;
		}

	public:

		Ipxe_session_component(size_t const tx_buf_size,
		                       size_t const rx_buf_size,
		                       Allocator   &rx_block_md_alloc,
		                       Env         &env)
		: Session_component(tx_buf_size, rx_buf_size, CACHED,
		                    rx_block_md_alloc, env)
		{
			instance = this;

			dde_ipxe_nic_register_callbacks(_rx_callback, _link_callback);

			dde_ipxe_nic_get_mac_addr(1, _mac_addr.addr);

			log("MAC address ", _mac_addr);
		}

		~Ipxe_session_component()
		{
			instance = nullptr;
			dde_ipxe_nic_unregister_callbacks();
		}

		/**************************************
		 ** Nic::Session_component interface **
		 **************************************/

		Nic::Mac_address mac_address() override { return _mac_addr; }

		bool link_state() override
		{
			return dde_ipxe_nic_link_state(1);
		}
};


Ipxe_session_component *Ipxe_session_component::instance;


class Uplink_client : public Uplink_client_base
{
	public:

		static Uplink_client *instance;

	private:

		Nic::Mac_address _init_drv_mac_addr()
		{
			instance = this;
			dde_ipxe_nic_register_callbacks(
				_drv_rx_callback, _drv_link_callback);

			Nic::Mac_address mac_addr { };
			dde_ipxe_nic_get_mac_addr(1, mac_addr.addr);
			return mac_addr;
		}


		/***********************************
		 ** Interface towards iPXE driver **
		 ***********************************/

		static void _drv_rx_callback(unsigned    interface_idx,
		                             const char *drv_rx_pkt_base,
		                             unsigned    drv_rx_pkt_size)
		{
			instance->_drv_rx_handle_pkt(
				drv_rx_pkt_size,
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
};


Uplink_client *Uplink_client::instance;


struct Main
{
	Env                    &_env;
	Heap                    _heap       { _env.ram(), _env.rm() };
	Attached_rom_dataspace  _config_rom { _env, "config" };

	Main(Env &env) : _env(env)
	{
		log("--- iPXE NIC driver started ---");

		log("-- init iPXE NIC");

		/* pass Env to backend */
		dde_support_init(_env, _heap);

		if (!dde_ipxe_nic_init()) {
			error("could not find usable NIC device");
		}
		Nic_driver_mode const mode {
			read_nic_driver_mode(_config_rom.xml()) };

		switch (mode) {
		case Nic_driver_mode::NIC_SERVER:
			{
				Nic::Root<Ipxe_session_component> &root {
					*new (_heap)
						Nic::Root<Ipxe_session_component>(_env, _heap) };

				_env.parent().announce(_env.ep().manage(root));
				break;
			}
		case Nic_driver_mode::UPLINK_CLIENT:

			new (_heap) Uplink_client(_env, _heap);
			break;
		}
	}
};


void Component::construct(Env &env) { static Main main(env); }
