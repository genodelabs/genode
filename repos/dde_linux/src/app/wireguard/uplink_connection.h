/*
 * \brief  Network back-end towards private network (unencrypted user packets)
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2022-01-07
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _UPLINK_CONNECTION_H_
#define _UPLINK_CONNECTION_H_

/* os includes */
#include <uplink_session/connection.h>
#include <net/ethernet.h>
#include <nic/packet_allocator.h>

namespace Wireguard {

	class Uplink_connection;
}


class Wireguard::Uplink_connection
{
	private:

		using Handle_packet_func = void (*)(void           *buf_base,
		                                    Genode::size_t  buf_size);

		using Packet_descriptor = Uplink::Packet_descriptor;
		using Packet_source     =
			::Uplink::Packet_stream_source<::Uplink::Session::Policy>;

		static constexpr Genode::size_t PACKET_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE;
		static constexpr Genode::size_t BUF_SIZE    = Uplink::Connection::Session::QUEUE_SIZE * PACKET_SIZE;

		enum class Send_pkt_result { SUCCEEDED, FAILED };

		Nic::Packet_allocator     _packet_alloc;
		Net::Mac_address    const _mac_address      { 2 };
		Uplink::Connection        _connection;
		bool                      _notify_peers     { true };
		bool                const _verbose          { true };
		bool                const _verbose_pkt_drop { true };

		void _connection_tx_flush_acks();

		template <typename WRITE_TO_PACKET_FUNC>
		Send_pkt_result _send(Genode::size_t          pkt_size,
		                      WRITE_TO_PACKET_FUNC && write_to_pkt)
		{
			try {
				Uplink::Packet_descriptor pkt        { _connection.tx()->alloc_packet(pkt_size) };
				void *const               pkt_base   { _connection.tx()->packet_content(pkt) };
				Net::Size_guard           size_guard { pkt_size };

				write_to_pkt(pkt_base, size_guard);
				_connection_tx_flush_acks();
				_connection.tx()->submit_packet(pkt);
			}
			catch (Packet_source::Packet_alloc_failed) {
				if (_verbose) {
					Genode::log(
						"Failed sending uplink packet - Failed allocating packet");
				}
				return Send_pkt_result::FAILED;
			}
			return Send_pkt_result::SUCCEEDED;
		}

	public:

		Uplink_connection(Genode::Env                       &env,
		                  Genode::Allocator                 &alloc,
		                  Genode::Signal_context_capability  sigh);

		void for_each_rx_packet(Handle_packet_func handle_packet);

		void notify_peer();

		void send_ip(void const     *ip_base,
		             Genode::size_t  ip_size);
};

#endif /* _UPLINK_CONNECTION_H_ */
