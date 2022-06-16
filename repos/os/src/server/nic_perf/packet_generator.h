/*
 * \brief  Packet generator
 * \author Johannes Schlatow
 * \date   2022-06-14
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PACKET_GENERATOR_H_
#define _PACKET_GENERATOR_H_

/* Genode includes */
#include <util/xml_node.h>
#include <net/mac_address.h>
#include <net/ipv4.h>
#include <net/arp.h>
#include <net/udp.h>
#include <timer_session/connection.h>

namespace Nic_perf {
	using namespace Genode;
	using namespace Net;

	class Interface;
	class Packet_generator;
}

class Nic_perf::Packet_generator
{
	public:
		struct Not_ready          : Exception { };
		struct Ip_address_not_set : Exception { };
		struct Udp_port_not_set   : Exception { };

	private:

		enum State { MUTED, NEED_ARP_REQUEST, WAIT_ARP_REPLY, READY };

		size_t       _mtu      { 1024 };
		bool         _enable   { false };
		Ipv4_address _dst_ip   { };
		Port         _dst_port { 0 };
		Mac_address  _dst_mac  { };
		State        _state    { MUTED };

		Timer::One_shot_timeout<Packet_generator>  _timeout;
		Nic_perf::Interface                       &_interface;

		void _generate_arp_request(void *, Size_guard &, Mac_address const &, Ipv4_address const &);

		void _generate_test_packet(void *, Size_guard &, Mac_address const &, Ipv4_address const &);

		void _handle_timeout(Genode::Duration);

	public:

		Packet_generator(Timer::Connection &timer, Nic_perf::Interface &interface)
		: _timeout(timer, *this, &Packet_generator::_handle_timeout),
		  _interface(interface)
		{ }

		void apply_config(Xml_node const &config)
		{
			Ipv4_address old_ip = _dst_ip;

			/* restore defaults */
			_dst_ip   = Ipv4_address();
			_dst_port = Port(0);
			_enable   = false;
			_state    = MUTED;

			config.with_sub_node("tx", [&] (Xml_node node) {
				_mtu      = node.attribute_value("mtu",      _mtu);
				_dst_ip   = node.attribute_value("to",       _dst_ip);
				_dst_port = node.attribute_value("udp_port", _dst_port);
				_enable   = true;
				_state    = READY;
			});

			/* redo ARP resolution if dst ip changed */
			if (old_ip != _dst_ip) {
				_dst_mac = Mac_address();

				if (_enable)
					_state = NEED_ARP_REQUEST;
			}
		}

		bool enabled() const  { return _enable; }

		size_t size() const
		{
			switch (_state) {
			case READY:
				return _mtu;
			case NEED_ARP_REQUEST:
				return Ethernet_frame::MIN_SIZE + sizeof(uint32_t);
			case WAIT_ARP_REPLY:
			case MUTED:
				return 0;
			}

			return 0;
		}

		void handle_arp_reply(Arp_packet const & arp);

		void generate(void *, Size_guard &, Mac_address const &, Ipv4_address const &);
};

#endif /* _PACKET_GENERATOR_H_ */
