/*
 * \brief  Modified base class for the Uplink client role of NIC drivers
 * \author Martin Stein
 * \author Johannes Schlatow
 * \date   2020-12-07
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
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
	public:

		using Label = String<64>;

	protected:

		enum class Transmit_result { ACCEPTED, REJECTED, RETRY };

		enum class Write_result { WRITE_SUCCEEDED, WRITE_FAILED };

		static constexpr size_t PKT_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE;
		static constexpr size_t BUF_SIZE = Uplink::Session::QUEUE_SIZE * PKT_SIZE;

		Env                                  &_env;
		Vfs::Env::User                       &_vfs_user;
		Allocator                            &_alloc;
		Label                     const      &_label;
		Net::Mac_address                      _drv_mac_addr;
		bool                                  _drv_mac_addr_used               { false };
		bool                                  _drv_link_state                  { false };
		Constructible<Uplink::Connection>     _conn                            { };
		Nic::Packet_allocator                 _conn_pkt_alloc                  { &_alloc };
		Io_signal_handler<Uplink_client_base> _conn_rx_ready_to_ack_handler    { _env.ep(), *this, &Uplink_client_base::_conn_rx_handle_ready_to_ack };
		Io_signal_handler<Uplink_client_base> _conn_rx_packet_avail_handler    { _env.ep(), *this, &Uplink_client_base::_conn_rx_handle_packet_avail };
		Io_signal_handler<Uplink_client_base> _conn_tx_ack_avail_handler       { _env.ep(), *this, &Uplink_client_base::_conn_tx_handle_ack_avail };

		/*****************************************
		 ** Interface towards Uplink connection **
		 *****************************************/

		void _conn_rx_handle_ready_to_ack()
		{
			if (!_conn.constructed())
				return;

			if (_custom_conn_rx_ready_to_ack_handler())
				_custom_conn_rx_handle_ready_to_ack();

			_vfs_user.wakeup_vfs_user();
		}

		void _conn_tx_handle_ack_avail()
		{
			if (!_conn.constructed())
				return;

			while (_conn->tx()->ack_avail()) {
				_conn->tx()->release_packet(_conn->tx()->get_acked_packet()); }

			_vfs_user.wakeup_vfs_user();
		}

		void _conn_rx_handle_packet_avail()
		{
			if (!_conn.constructed())
				return;

			if (_custom_conn_rx_packet_avail_handler())
				_custom_conn_rx_handle_packet_avail();

			_vfs_user.wakeup_vfs_user();
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
					_drv_mac_addr, _label.string());

				/* install signal handlers at connection */
				_conn->rx_channel()->sigh_ready_to_ack(
					_conn_rx_ready_to_ack_handler);

				_conn->rx_channel()->sigh_packet_avail(
					_conn_rx_packet_avail_handler);

				_conn->tx_channel()->sigh_ack_avail(
					_conn_tx_ack_avail_handler);

			} else {

				_conn.destruct();
				_drv_mac_addr_used = false;
			}
		}

		virtual Transmit_result
		_drv_transmit_pkt(const char *conn_rx_pkt_base,
		                  size_t      conn_rx_pkt_size) = 0;

		virtual void _custom_conn_rx_handle_packet_avail()
		{
			class Unexpected_call { };
			throw Unexpected_call { };
		}

		virtual void _custom_conn_rx_handle_ready_to_ack()
		{
			class Unexpected_call { };
			throw Unexpected_call { };
		}

		virtual bool _custom_conn_rx_packet_avail_handler() { return false; }
		virtual bool _custom_conn_rx_ready_to_ack_handler() { return false; }

	public:

		Uplink_client_base(Env                    &env,
		                   Vfs::Env::User         &vfs_user,
		                   Allocator              &alloc,
		                   Net::Mac_address const &drv_mac_addr,
		                   Label            const &label)
		:
			_env          { env },
			_vfs_user     { vfs_user },
			_alloc        { alloc },
			_label        { label },
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
			log("MAC address ", _drv_mac_addr);
		}

		virtual ~Uplink_client_base() { }
};

#endif /* _DRIVERS__NIC__UPLINK_CLIENT_BASE_H_ */
