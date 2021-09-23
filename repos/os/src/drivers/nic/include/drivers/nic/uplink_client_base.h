/*
 * \brief  Generic base class for the Uplink client role of NIC drivers
 * \author Martin Stein
 * \date   2020-12-07
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVERS__NIC__UPLINK_CLIENT_BASE_H_
#define _DRIVERS__NIC__UPLINK_CLIENT_BASE_H_

/* Genode includes */
#include <net/mac_address.h>
#include <nic/packet_allocator.h>
#include <uplink_session/connection.h>

namespace Genode {

	class Uplink_client_base;
}

class Genode::Uplink_client_base : Noncopyable
{
	protected:

		enum class Transmit_result { ACCEPTED, REJECTED, RETRY };

		enum class Write_result { WRITE_SUCCEEDED, WRITE_FAILED };

		enum class Burst_result
		{
			BURST_SUCCEEDED,
			BURST_FAILED,
			BURST_CONTINUE
		};

		enum { PKT_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE };
		enum { BUF_SIZE = Uplink::Session::QUEUE_SIZE * PKT_SIZE };

		Env                                &_env;
		Allocator                          &_alloc;
		Net::Mac_address                    _drv_mac_addr;
		bool                                _drv_mac_addr_used               { false };
		bool                                _drv_link_state                  { false };
		Constructible<Uplink::Connection>   _conn                            { };
		Nic::Packet_allocator               _conn_pkt_alloc                  { &_alloc };
		Signal_handler<Uplink_client_base>  _conn_rx_ready_to_ack_handler    { _env.ep(), *this, &Uplink_client_base::_conn_rx_handle_ready_to_ack };
		Signal_handler<Uplink_client_base>  _conn_rx_packet_avail_handler    { _env.ep(), *this, &Uplink_client_base::_conn_rx_handle_packet_avail };
		Signal_handler<Uplink_client_base>  _conn_tx_ack_avail_handler       { _env.ep(), *this, &Uplink_client_base::_conn_tx_handle_ack_avail };
		Signal_handler<Uplink_client_base>  _conn_tx_ready_to_submit_handler { _env.ep(), *this, &Uplink_client_base::_conn_tx_handle_ready_to_submit };
		Packet_descriptor                   _save                            { };


		/*****************************************
		 ** Interface towards Uplink connection **
		 *****************************************/

		void _conn_tx_handle_ready_to_submit() { }

		void _conn_rx_handle_ready_to_ack() { }

		void _conn_tx_handle_ack_avail()
		{
			while (_conn->tx()->ack_avail()) {

				_conn->tx()->release_packet(_conn->tx()->get_acked_packet());
			}
		}

		void _conn_rx_handle_packet_avail()
		{
			if (_custom_conn_rx_packet_avail_handler()) {

				_custom_conn_rx_handle_packet_avail();

			} else if (_drv_supports_transmit_pkt_burst()) {

				_drv_transmit_pkt_burst_prepare();

				/* submit received packets to lower layer */
				while (((_conn->rx()->packet_avail() || _save.size()) && _conn->rx()->ready_to_ack())) {

					Packet_descriptor packet = _save.size() ? _save : _conn->rx()->get_packet();
					_save                    = Packet_descriptor();

					if (!packet.size()) {

						class Invalid_packet { };
						throw Invalid_packet { };
					}

					char const *packet_base {
						_conn->rx()->packet_content(packet) };

					Burst_result const result {
						_drv_transmit_pkt_burst_step(
							packet, packet_base, _save) };

					switch (result) {
					case Burst_result::BURST_FAILED:
						return;
					case Burst_result::BURST_CONTINUE:
						continue;
					case Burst_result::BURST_SUCCEEDED:
						break;
					}

					/* acknowledge to client */
					_conn->rx()->acknowledge_packet(packet);
				}
				_drv_transmit_pkt_burst_finish();

			} else {

				bool drv_ready_to_transmit_pkt { _drv_link_state };
				bool pkts_transmitted          { false };

				while (drv_ready_to_transmit_pkt &&
				       _conn->rx()->packet_avail() &&
				       _conn->rx()->ready_to_ack()) {

					Packet_descriptor const conn_rx_pkt {
						_conn->rx()->get_packet() };

					if (conn_rx_pkt.size() > 0 &&
					    _conn->rx()->packet_valid(conn_rx_pkt)) {

						const char *const conn_rx_pkt_base {
							_conn->rx()->packet_content(conn_rx_pkt) };

						switch (_drv_transmit_pkt(conn_rx_pkt_base,
						                          conn_rx_pkt.size())) {

						case Transmit_result::ACCEPTED:

							pkts_transmitted = true;
							_conn->rx()->acknowledge_packet(conn_rx_pkt);
							break;

						case Transmit_result::REJECTED:

							warning("failed to forward packet from "
							        "Uplink-connection RX to driver");

							_conn->rx()->acknowledge_packet(conn_rx_pkt);
							break;

						case Transmit_result::RETRY:

							drv_ready_to_transmit_pkt = false;
							break;
						}

					} else {

						warning("ignoring invalid packet from Uplink-"
						        "connection RX");
					}
				}

				if (pkts_transmitted) {

					_drv_finish_transmitted_pkts();
				}
			}
		}


		/***************************************************
		 ** Generic back end for interface towards driver **
		 ***************************************************/

		template <typename FUNC>
		void _drv_rx_handle_pkt(size_t  conn_tx_pkt_size,
		                        FUNC && write_to_conn_tx_pkt)
		{
			if (!_conn.constructed()) {
				return;
			}
			_conn_tx_handle_ack_avail();

			if (!_conn->tx()->ready_to_submit()) {
				return;
			}
			try {
				Packet_descriptor conn_tx_pkt {
					_conn->tx()->alloc_packet(conn_tx_pkt_size) };

				void *conn_tx_pkt_base {
					_conn->tx()->packet_content(conn_tx_pkt) };

				size_t adjusted_conn_tx_pkt_size {
					conn_tx_pkt_size };

				Write_result write_result {
					write_to_conn_tx_pkt(
						conn_tx_pkt_base,
						adjusted_conn_tx_pkt_size) };

				switch (write_result) {
				case Write_result::WRITE_SUCCEEDED:

					if (adjusted_conn_tx_pkt_size == conn_tx_pkt_size) {

						_conn->tx()->submit_packet(conn_tx_pkt);

					} else if (adjusted_conn_tx_pkt_size < conn_tx_pkt_size) {

						Packet_descriptor adjusted_conn_tx_pkt {
							conn_tx_pkt.offset(), adjusted_conn_tx_pkt_size };

						_conn->tx()->submit_packet(adjusted_conn_tx_pkt);

					} else {

						class Bad_size { };
						throw Bad_size { };
					}
					break;

				case Write_result::WRITE_FAILED:

					_conn->tx()->release_packet(conn_tx_pkt);
				}

			} catch (...) {

				warning("exception while trying to forward packet from driver "
				        "to Uplink connection TX");

				return;
			}
		}

		void _drv_handle_link_state(bool drv_link_state)
		{
			if (_drv_link_state == drv_link_state) {
				return;
			}
			_drv_link_state = drv_link_state;
			if (drv_link_state) {

				/* create connection */
				_drv_mac_addr_used = true;
				_conn.construct(
					_env, &_conn_pkt_alloc, BUF_SIZE, BUF_SIZE,
					_drv_mac_addr);

				/* install signal handlers at connection */
				_conn->rx_channel()->sigh_ready_to_ack(
					_conn_rx_ready_to_ack_handler);

				_conn->rx_channel()->sigh_packet_avail(
					_conn_rx_packet_avail_handler);

				_conn->tx_channel()->sigh_ack_avail(
					_conn_tx_ack_avail_handler);

				_conn->tx_channel()->sigh_ready_to_submit(
					_conn_tx_ready_to_submit_handler);
			} else {

				_conn.destruct();
			}
		}

		virtual void _drv_finish_transmitted_pkts() { }

		virtual bool _drv_supports_transmit_pkt_burst()
		{
			return false;
		}

		virtual void _drv_transmit_pkt_burst_finish()
		{
			class Unexpected_call { };
			throw Unexpected_call { };
		}

		virtual void _drv_transmit_pkt_burst_prepare()
		{
			class Unexpected_call { };
			throw Unexpected_call { };
		}

		virtual Burst_result
		_drv_transmit_pkt_burst_step(Packet_descriptor const &packet,
		                             char              const *packet_base,
		                             Packet_descriptor       &save)
		{
			(void)packet;
			(void)packet_base;
			(void)save;
			class Unexpected_call { };
			throw Unexpected_call { };
		}

		virtual Transmit_result
		_drv_transmit_pkt(const char *conn_rx_pkt_base,
		                  size_t      conn_rx_pkt_size) = 0;

		virtual void _custom_conn_rx_handle_packet_avail()
		{
			class Unexpected_call { };
			throw Unexpected_call { };
		}

		virtual bool _custom_conn_rx_packet_avail_handler() { return false; }

	public:

		Uplink_client_base(Env                    &env,
		                   Allocator              &alloc,
		                   Net::Mac_address const &drv_mac_addr)
		:
			_env          { env },
			_alloc        { alloc },
			_drv_mac_addr { drv_mac_addr }
		{
			log("MAC address ", _drv_mac_addr);
		}

		void mac_address(Net::Mac_address const &mac_address)
		{
			if (_drv_mac_addr_used) {

				class Already_in_use { };
				throw Already_in_use { };
			}
			_drv_mac_addr = mac_address;
		}

		virtual ~Uplink_client_base() { }
};

#endif /* _DRIVERS__NIC__UPLINK_CLIENT_BASE_H_ */
