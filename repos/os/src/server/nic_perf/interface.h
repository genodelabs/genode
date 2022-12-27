/*
 * \brief  Base class for Nic/Uplink session components
 * \author Johannes Schlatow
 * \date   2022-06-15
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INTERFACE_H_
#define _INTERFACE_H_

/* local includes */
#include <packet_generator.h>
#include <packet_stats.h>
#include <dhcp_client.h>

/* Genode includes */
#include <base/registry.h>
#include <os/packet_stream.h>
#include <net/dhcp.h>
#include <nic_session/nic_session.h>
#include <timer_session/connection.h>

namespace Nic_perf {
	using namespace Genode;
	using namespace Net;

	using Dhcp_options = Dhcp_packet::Options_aggregator<Size_guard>;

	class Interface;

	using Interface_registry = Registry<Interface>;
}

class Nic_perf::Interface
{
	protected:

		using Sink   = Nic::Packet_stream_sink<Nic::Session::Policy>;
		using Source = Nic::Packet_stream_source<Nic::Session::Policy>;

		Interface_registry::Element _element;
		Session_label               _label;

		Packet_stats                _stats;
		Packet_generator            _generator;

		bool                        _mac_from_policy;

		Mac_address                 _mac { };
		Mac_address const           _default_mac;
		Ipv4_address                _ip  { };
		Ipv4_address                _dhcp_client_ip { };

		Source                     &_source;
		Sink                       &_sink;

		Constructible<Dhcp_client>  _dhcp_client { };
		Timer::Connection          &_timer;

		static Ipv4_address _subnet_mask()
		{
			uint8_t buf[] = { 0xff, 0xff, 0xff, 0 };
			return Ipv4_address((void*)buf);
		}

		void _handle_eth(void *, size_t);
		void _handle_ip(Ethernet_frame &, Size_guard &);
		void _handle_arp(Ethernet_frame &, Size_guard &);
		void _handle_dhcp_request(Ethernet_frame &, Dhcp_packet &);
		void _send_dhcp_reply(Ethernet_frame const &, Dhcp_packet const &, Dhcp_packet::Message_type);

	public:

		Interface(Interface_registry  &registry,
		          Session_label const &label,
		          Xml_node      const &policy,
		          bool                 mac_from_policy,
		          Mac_address          mac,
		          Source              &source,
		          Sink                &sink,
		          Timer::Connection   &timer)
		: _element(registry, *this),
		  _label(label),
		  _stats(_label),
		  _generator(timer, *this),
		  _mac_from_policy(mac_from_policy),
		  _default_mac(mac),
		  _source(source),
		  _sink(sink),
		  _timer(timer)
		{ apply_config(policy); }

		void apply_config(Xml_node const &config)
		{
			_generator.apply_config(config);

			/* restore defaults when applied to empty/incomplete config */
			_mac            = _default_mac;
			_ip             = Ipv4_address();
			_dhcp_client_ip = Ipv4_address();

			_dhcp_client.destruct();

			config.with_sub_node("interface",
				[&] (Xml_node node) {
					_ip             = node.attribute_value("ip", _ip);
					_dhcp_client_ip = node.attribute_value("dhcp_client_ip", _dhcp_client_ip);

					if (_mac_from_policy)
						_mac         = node.attribute_value("mac", _mac);
				},

				/* node does not exist */
				[&] () { _dhcp_client.construct(_timer, *this); }
			);
		}

		Session_label const &label()        const { return _label; }
		Packet_stats        &packet_stats()       { return _stats; }

		Mac_address   const &mac()          const { return _mac; }
		Ipv4_address  const &ip()           const { return _ip; }
		void ip(Ipv4_address const &ip)           { _ip = ip; }

		void handle_packet_stream();

		template <typename FUNC>
		bool send(size_t pkt_size, FUNC && write_to_pkt)
		{
			if (!pkt_size)
				return false;

			try {
				Packet_descriptor  pkt      = _source.alloc_packet(pkt_size);
				void              *pkt_base = _source.packet_content(pkt);

				Size_guard size_guard { pkt_size };
				write_to_pkt(pkt_base, size_guard);

				_source.try_submit_packet(pkt);
			} catch (...) { return false; }

			_stats.tx_packet(pkt_size);

			return true;
		}
};

#endif /* _INTERFACE_H_ */
