/*
 * \brief  UDP/TCP proxy session
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PROXY_H_
#define _PROXY_H_

/* Genode includes */
#include <timer_session/connection.h>
#include <net/ipv4.h>
#include <util/list.h>

namespace Net {

	class Tcp_packet;
	class Udp_packet;
	class Interface;
	class Tcp_proxy;
	class Udp_proxy;

	using Tcp_proxy_list = Genode::List<Tcp_proxy>;
	using Udp_proxy_list = Genode::List<Udp_proxy>;
}

class Net::Tcp_proxy : public Genode::List<Tcp_proxy>::Element
{
	private:

		using Signal_handler = Genode::Signal_handler<Tcp_proxy>;

		Genode::uint16_t const _client_port;
		Genode::uint16_t const _proxy_port;
		Ipv4_address const     _client_ip;
		Ipv4_address const     _proxy_ip;
		Interface             &_client;
		Timer::Connection      _timer;
		bool                   _client_fin = false;
		bool                   _other_fin = false;
		bool                   _client_fin_acked = false;
		bool                   _other_fin_acked = false;
		bool                   _del = false;
		Signal_handler         _del_timeout;
		unsigned const         _del_timeout_us;

		void _del_timeout_handle() { _del = true; }

	public:

		Tcp_proxy(Genode::uint16_t client_port, Genode::uint16_t proxy_port,
		          Ipv4_address client_ip, Ipv4_address proxy_ip,
		          Interface &client, Genode::Entrypoint &ep,
		          unsigned const rtt_sec);

		void print(Genode::Output &out) const;

		bool matches_client(Ipv4_address client_ip,
		                    Genode::uint16_t client_port);

		bool matches_proxy(Ipv4_address proxy_ip,
		                   Genode::uint16_t proxy_port);

		void tcp_packet(Ipv4_packet *const ip, Tcp_packet *const tcp);


		/***************
		 ** Accessors **
		 ***************/

		Genode::uint16_t client_port() const { return _client_port; }
		Genode::uint16_t proxy_port()  const { return _proxy_port; }
		Ipv4_address     client_ip()   const { return _client_ip; }
		Ipv4_address     proxy_ip()    const { return _proxy_ip; }
		Interface       &client()      const { return _client; }
		bool             del()         const { return _del; }
};

class Net::Udp_proxy : public Genode::List<Udp_proxy>::Element
{
	private:

		using Signal_handler = Genode::Signal_handler<Udp_proxy>;

		Genode::uint16_t const _client_port;
		Genode::uint16_t const _proxy_port;
		Ipv4_address const     _client_ip;
		Ipv4_address const     _proxy_ip;
		Interface             &_client;
		Timer::Connection      _timer;
		bool                   _del = false;
		Signal_handler         _del_timeout;
		unsigned const         _del_timeout_us;

		void _del_timeout_handle() { _del = true; }

	public:

		Udp_proxy(Genode::uint16_t client_port, Genode::uint16_t proxy_port,
		          Ipv4_address client_ip, Ipv4_address proxy_ip,
		          Interface &client, Genode::Entrypoint &ep,
		          unsigned const rtt_sec);

		void print(Genode::Output &out) const;

		bool matches_client(Ipv4_address client_ip,
		                    Genode::uint16_t client_port);

		bool matches_proxy(Ipv4_address proxy_ip,
		                   Genode::uint16_t proxy_port);

		void udp_packet(Ipv4_packet *const ip, Udp_packet *const udp);


		/***************
		 ** Accessors **
		 ***************/

		Genode::uint16_t client_port() const { return _client_port; }
		Genode::uint16_t proxy_port()  const { return _proxy_port; }
		Ipv4_address     client_ip()   const { return _client_ip; }
		Ipv4_address     proxy_ip()    const { return _proxy_ip; }
		Interface       &client()      const { return _client; }
		bool             del()         const { return _del; }
};

#endif /* _PROXY_H_ */
