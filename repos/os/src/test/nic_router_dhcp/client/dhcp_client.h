/*
 * \brief  DHCP client state model
 * \author Martin Stein
 * \date   2016-08-24
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DHCP_CLIENT_H_
#define _DHCP_CLIENT_H_

/* Genode includes */
#include <net/dhcp.h>
#include <timer_session/connection.h>

namespace Net {

	/* external definition */
	class Nic_peer;
	class Ipv4_config;
	class Ethernet_frame;

	/* local definition */
	class Dhcp_client;
	class Dhcp_client_handler;
	class Drop_packet_inform;
}


struct Net::Drop_packet_inform : Genode::Exception
{
	char const *msg;

	Drop_packet_inform(char const *msg) : msg(msg) { }
};


class Net::Dhcp_client_handler
{
	public:

		virtual void ip_config(Ipv4_config const &ip_config) = 0;

		virtual Ipv4_config const &ip_config() const = 0;

		virtual ~Dhcp_client_handler() { }
};


class Net::Dhcp_client
{
	private:

		enum class State
		{
			INIT = 0, SELECT = 1, REQUEST = 2, BOUND = 3, RENEW = 4, REBIND = 5
		};

		enum { DISCOVER_TIMEOUT_SEC = 2 };
		enum { REQUEST_TIMEOUT_SEC  = 2 };

		Genode::Allocator                    &_alloc;
		State                                 _state { State::INIT };
		Timer::One_shot_timeout<Dhcp_client>  _timeout;
		unsigned long                         _lease_time_sec = 0;
		Genode::Microseconds const            _discover_timeout { (Genode::uint64_t)DISCOVER_TIMEOUT_SEC * 1000 * 1000 };
		Genode::Microseconds const            _request_timeout  { (Genode::uint64_t)REQUEST_TIMEOUT_SEC * 1000 * 1000  };
		Nic                                  &_nic;
		Dhcp_client_handler                  &_handler;

		void _handle_dhcp_reply(Dhcp_packet &dhcp);

		void _handle_timeout(Genode::Duration);

		void _rerequest(State next_state);

		Genode::Microseconds _rerequest_timeout(unsigned lease_time_div_log2);

		void _set_state(State state, Genode::Microseconds timeout);

		void _send(Dhcp_packet::Message_type msg_type,
		           Ipv4_address              client_ip,
		           Ipv4_address              server_ip,
		           Ipv4_address              requested_ip);

		void _discover();

	public:

		Dhcp_client(Genode::Allocator   &alloc,
		            Timer::Connection   &timer,
		            Nic                 &nic,
		            Dhcp_client_handler &handler);

		void handle_eth(Ethernet_frame &eth,
		                Size_guard     &size_guard);

};

#endif /* _DHCP_CLIENT_H_ */
