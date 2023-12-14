/*
 * \brief  Uplink client that drops and acks received packets
 * \author Sebastian Sumpf
 * \date   2023-12-14
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _UPLINK_CLIENT_H_
#define _UPLINK_CLIENT_H_

/* Genode includes */
#include <net/mac_address.h>
#include <nic/packet_allocator.h>
#include <uplink_session/connection.h>

namespace Genode {
	class Uplink_client;
}

class Genode::Uplink_client : Noncopyable
{
	private:

		Env       &_env;
		Allocator &_alloc;

		Nic::Packet_allocator _pkt_alloc { &_alloc };

		static Net::Mac_address _default_mac_address()
		{
			Net::Mac_address mac_addr { (uint8_t)0 };

			mac_addr.addr[0] = 0x02; /* unicast, locally managed MAC address */
			mac_addr.addr[5] = 0x01; /* arbitrary index */

			return mac_addr;
		}

		Net::Mac_address   _mac_addr { _default_mac_address() };
		Uplink::Connection _uplink { _env, &_pkt_alloc, 64*1024, 64*1024, _mac_addr };

		Signal_handler<Uplink_client> _packet_stream_handler { _env.ep(), *this,
			&Uplink_client::_handle_packet_stream };

		void _handle_packet_stream()
		{
			while (_uplink.rx()->packet_avail() &&
			       _uplink.rx()->ready_to_ack()) {
				Packet_descriptor const pkt { _uplink.rx()->get_packet() };
				if (!pkt.size() || !_uplink.rx()->packet_valid(pkt)) {
					continue;
				}
				_uplink.rx()->acknowledge_packet(pkt);
			}
		}

	public:

		Uplink_client(Env &env, Allocator &alloc)
		: _env(env), _alloc(alloc)
		{
			_uplink.rx_channel()->sigh_ready_to_ack(_packet_stream_handler);
			_uplink.rx_channel()->sigh_packet_avail(_packet_stream_handler);
		}
};

#endif /* _UPLINK_CLIENT_H_ */
