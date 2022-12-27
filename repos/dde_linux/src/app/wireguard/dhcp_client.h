/*
 * \brief  DHCP client state model
 * \author Martin Stein
 * \date   2016-08-24
 */

/*
 * Copyright (C) 2016-2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DHCP_CLIENT_H_
#define _DHCP_CLIENT_H_

/* base includes */
#include <timer_session/connection.h>

/* dde_linux wireguard includes */
#include <dhcp.h>

namespace Wireguard {

	class Nic_connection;
}

namespace Net {

	class Ethernet_frame;
	class Dhcp_client;
}

class Net::Dhcp_client
{
	private:

		enum class State
		{
			INIT = 0, SELECT = 1, REQUEST = 2, BOUND = 3, RENEW = 4, REBIND = 5
		};

		Wireguard::Nic_connection            &_nic_connection;
		State                                 _state { State::INIT };
		Timer::One_shot_timeout<Dhcp_client>  _timeout;
		Genode::uint64_t                      _lease_time_sec = 0;

		void _handle_timeout(Genode::Duration);

		void _rerequest(State next_state);

		Genode::Microseconds _rerequest_timeout(unsigned lease_time_div_log2);

		void _set_state(State state, Genode::Microseconds timeout);

		void _send(Dhcp_packet::Message_type msg_type,
		           Ipv4_address              client_ip,
		           Ipv4_address              server_ip,
		           Ipv4_address              requested_ip);

	public:

		Dhcp_client(Timer::Connection         &timer,
		            Wireguard::Nic_connection &nic_connection);

		void handle_eth(Ethernet_frame &eth,
		                Size_guard     &size_guard);

		void discover();
};

#endif /* _DHCP_CLIENT_H_ */
