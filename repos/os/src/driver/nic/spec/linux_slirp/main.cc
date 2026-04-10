/*
 * \brief  NIC driver for Linux using libslirp
 * \author Johannes Schlatow
 * \date   2026-04-09
 */

/*
 * Copyright (C) 2026 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/thread.h>
#include <base/log.h>
#include <base/blockade.h>
#include <os/reporter.h>
#include <net/port.h>
#include <net/ipv4.h>

/* local includes */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <libslirp_context.h>
#pragma GCC diagnostic pop

/* NIC driver includes */
#include <drivers/nic/uplink_client_base.h>

using namespace Genode;

using Mac_address = Net::Mac_address;


class Uplink_client : public Uplink_client_base
{
	private:

		struct Poll_thread : Thread, Libslirp::Action
		{
			Uplink_client &uplink;
			Blockade       blockade { };

			const void    *send_buf { nullptr };
			size_t         send_len { 0 };

			Thread const  *main_context { Thread::myself() };

			Poll_thread(Poll_thread const &) = delete;
			Poll_thread & operator = (Poll_thread const &) = delete;

			Poll_thread(Env &env, Uplink_client &uplink)
			: Thread(env, "poll", Stack_size { 0x1000 }), uplink(uplink) { }

			size_t send(const void *buf, size_t len) override
			{
				/* block on re-entry until _handle_rx() processed the last packet */
				blockade.block();

				/* store buf and len */
				send_buf = buf;
				send_len = len;

				if (Thread::myself() == main_context)
					uplink._handle_rx();
				else
					uplink._rx_handler.local_submit();

				return len;
			}

			void entry() override {
				blockade.wakeup();
				uplink._slirp.poll_loop(); }
		};

		bool                          _verbose;

		Signal_handler<Uplink_client> _rx_handler  { _env.ep(), *this, &Uplink_client::_handle_rx };
		Poll_thread                   _poll_thread { _env, *this };

		Libslirp::Context             _slirp;

		void _handle_rx()
		{
			size_t const max_pkt_size {
				Nic::Packet_allocator::OFFSET_PACKET_SIZE };

			/* transmit packet to uplink */
			_drv_rx_handle_pkt(
				max_pkt_size,
				[&] (void   *conn_tx_pkt_base,
				     size_t &adjusted_conn_tx_pkt_size)
			{
				if (!_poll_thread.send_buf || !_poll_thread.send_len) {
					return Write_result::WRITE_FAILED;
				}

				memcpy(conn_tx_pkt_base, _poll_thread.send_buf, _poll_thread.send_len);
				adjusted_conn_tx_pkt_size = _poll_thread.send_len;

				_poll_thread.send_buf = nullptr;
				_poll_thread.send_len = 0;
				return Write_result::WRITE_SUCCEEDED;
			});

			/* wakeup poll thread */
			_poll_thread.blockade.wakeup();
		}


		/************************
		 ** Uplink_client_base **
		 ************************/

		Transmit_result
		_drv_transmit_pkt(const char *conn_rx_pkt_base,
		                  size_t      conn_rx_pkt_size) override
		{
	
			if (_slirp.input((const uint8_t *)conn_rx_pkt_base, (int)conn_rx_pkt_size))
				return Transmit_result::ACCEPTED;

			return Transmit_result::REJECTED;
		}

	public:

		Uplink_client(Env               &env,
		              Allocator         &alloc,
		              Mac_address const &mac_address,
		              Net::Ipv4_address &network,
		              bool               verbose)
		:
			Uplink_client_base { env, alloc, mac_address },
			_verbose { verbose },
			_slirp   { _poll_thread, network, _verbose }
		{
			_drv_handle_link_state(true);
			_poll_thread.start();
		}

		void add_udp_forward(Net::Port port, Net::Ipv4_address to, Net::Port to_port) {
			_slirp.add_hostfwd(port, to, to_port, true); }

		void add_tcp_forward(Net::Port port, Net::Ipv4_address to, Net::Port to_port) {
			_slirp.add_hostfwd(port, to, to_port, false);
		}

		Net::Ipv4_address guest_ip() const {
			return _slirp.guest_ip(); }
};


struct Main
{
	Env                   &_env;
	Heap                   _heap       { _env.ram(), _env.rm() };
	Attached_rom_dataspace _config_rom { _env, "config" };

	static Mac_address _default_mac_address()
	{
		Mac_address mac_addr { (uint8_t)0 };

		mac_addr.addr[0] = 0x02; /* unicast, locally managed MAC address */
		mac_addr.addr[5] = 0x01; /* arbitrary index */

		return mac_addr;
	}

	Mac_address _mac_address {
		_config_rom.node().attribute_value("mac", _default_mac_address()) };

	Net::Ipv4_address _network {
		_config_rom.node().attribute_value("network", Net::Ipv4_packet::ip_from_string("10.0.2.0")) };

	bool _verbose {
		_config_rom.node().attribute_value("verbose", false) };

	Uplink_client _uplink { _env, _heap, _mac_address, _network, _verbose };

	Constructible<Expanding_reporter> _reporter { };

	Main(Env &env) : _env(env)
	{
		/* report mac address if desired */
		_config_rom.node().with_optional_sub_node("report", [&] (Node const &node) {
			bool const report_mac_address =
				node.attribute_value("mac_address", false);

			if (!report_mac_address)
				return;

			_reporter.construct(_env, "devices");

			_reporter->generate([&] (Generator &g) {
				g.node("nic", [&] () {
					g.attribute("mac_address", String<32>(_mac_address));
				});
			});
		});

		/* parse forwarding rules */
		_config_rom.node().for_each_sub_node([&] (Node const &node) {
			if (node.type() != "udp_forward" && node.type() != "tcp_forward")
				return;

			using namespace Net;

			Port const port    = node.attribute_value("port",    Port(0));
			Port const to_port = node.attribute_value("to_port", port);

			if (port.value == 0) {
				error("Missing or invalid port attribute for ", node.type(), " node");
				return;
			}

			Net::Ipv4_address to = node.attribute_value("to", _uplink.guest_ip());

			if (node.type() == "udp_forward")
				_uplink.add_udp_forward(port, to, to_port);
			else
				_uplink.add_tcp_forward(port, to, to_port);
		});
	}
};


void Component::construct(Env &env) { static Main main(env); }
