/*
 * \brief  Network back-end towards public network (encrypted UDP tunnel)
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

#ifndef _NIC_CONNECTION_H_
#define _NIC_CONNECTION_H_

/* base includes */
#include <base/duration.h>

/* os includes */
#include <nic_session/connection.h>
#include <nic/packet_allocator.h>
#include <net/ethernet.h>
#include <net/arp.h>

/* dde_linux wireguard includes */
#include <ipv4_address_prefix.h>
#include <arp_cache.h>
#include <arp_waiter.h>
#include <ipv4_config.h>
#include <dhcp_client.h>

namespace Timer
{
	class Connection;
}

namespace Net {

	class Dhcp_packet;
}

namespace Wireguard {

	class Nic_connection_notifier;
	class Nic_connection;
}


class Wireguard::Nic_connection_notifier : Genode::Interface
{
	public:

		virtual void notify_about_ip_config_update() = 0;
};


class Wireguard::Nic_connection
{
	private:

		using Handle_packet_func = void (*)(void           *buf_base,
		                                    Genode::size_t  buf_size);

		using Packet_source = Nic::Packet_stream_source<::Nic::Session::Policy>;

		enum { PACKET_SIZE         = Nic::Packet_allocator::DEFAULT_PACKET_SIZE    };
		enum { WG_PROT_PACKET_SIZE = Nic::Packet_allocator::OFFSET_PACKET_SIZE };
		enum { BUF_SIZE            = Nic::Connection::Session::QUEUE_SIZE * PACKET_SIZE };

		enum {

			ETHERNIC_CONNECTION_HEADER_SIZE = sizeof(Net::Ethernet_frame),

			ETHERNIC_CONNECTION_DATA_SIZE_WITH_ARP =
				sizeof(Net::Arp_packet) + ETHERNIC_CONNECTION_HEADER_SIZE < Net::Ethernet_frame::MIN_SIZE ?
					Net::Ethernet_frame::MIN_SIZE - ETHERNIC_CONNECTION_HEADER_SIZE :
					sizeof(Net::Arp_packet),

			ETHERNIC_CONNECTION_CRC_SIZE = sizeof(Genode::uint32_t),

			ARP_PACKET_SIZE =
				ETHERNIC_CONNECTION_HEADER_SIZE +
				ETHERNIC_CONNECTION_DATA_SIZE_WITH_ARP +
				ETHERNIC_CONNECTION_CRC_SIZE,
		};

		enum Handle_pkt_result { DROP_PACKET, ACK_PACKET };

		enum Send_pkt_result { SUCCEEDED, FAILED, PACKET_WAITS_FOR_ARP };

		Genode::Allocator                      &_alloc;
		Nic_connection_notifier                &_notifier;
		Net::Dhcp_client                        _dhcp_client;
		Genode::Reconstructible<Ipv4_config>    _ip_config;
		Nic::Packet_allocator                   _packet_alloc       { &_alloc };
		bool                                    _notify_peers       { true };
		Net::Arp_cache                          _arp_cache          { };
		Net::Arp_waiter_list                    _arp_waiters        { };
		Nic::Connection                         _connection;
		Net::Mac_address const                  _mac_address        { _connection.mac_address() };
		bool const                              _verbose            { true };
		bool const                              _verbose_pkt_drop   { true };
		Genode::Signal_handler<Nic_connection>  _link_state_handler;

		Send_pkt_result
		_finish_send_eth_ipv4_with_eth_dst_set_via_arp(Nic::Packet_descriptor  pkt,
		                                               Net::Mac_address const &eth_dst);

		void _connection_tx_flush_acks();

		template <typename FUNC>
		Send_pkt_result
		_send_eth_ipv4_with_eth_dst_set_via_arp(Genode::size_t           pkt_size,
		                                        Net::Ipv4_address const &dst_ip,
		                                        FUNC                 &&  write_to_pkt)
		{
			Send_pkt_result result { Send_pkt_result::FAILED };
			try {
				Nic::Packet_descriptor  pkt        { _connection.tx()->alloc_packet(pkt_size) };
				void                   *pkt_base   { _connection.tx()->packet_content(pkt) };
				Net::Size_guard         size_guard { pkt_size };
				Net::Ethernet_frame    &eth        { Net::Ethernet_frame::construct_at(pkt_base, size_guard) };

				write_to_pkt(eth, size_guard);
				_arp_cache.find_by_ip(dst_ip).with_result(
					[&] (Net::Const_pointer<Net::Arp_cache_entry> entry_ref) {
						result = _finish_send_eth_ipv4_with_eth_dst_set_via_arp(
							pkt, entry_ref.deref().mac());
					},
					[&] (Net::Arp_cache_error) {
						_broadcast_arp_request(ip_config().interface().address, dst_ip);
						new (_alloc) Net::Arp_waiter { _arp_waiters, dst_ip, pkt };
						result = Send_pkt_result::PACKET_WAITS_FOR_ARP;
					}
				);
			}
			catch (Packet_source::Packet_alloc_failed) {
				if (_verbose) {
					Genode::log("Failed sending NIC packet - Failed allocating packet");
				}
			}
			return result;
		}

		Handle_pkt_result _drop_pkt(char const *packet_type,
		                            char const *reason);

		Send_pkt_result _send_arp_reply(Net::Ethernet_frame &request_eth,
		                                Net::Arp_packet     &request_arp);

		Handle_pkt_result _handle_arp_request(Net::Ethernet_frame &eth,
		                                      Net::Arp_packet     &arp);

		Handle_pkt_result _handle_arp(Net::Ethernet_frame &eth,
		                              Net::Size_guard     &size_guard);

		void _broadcast_arp_request(Net::Ipv4_address const &src_ip,
		                            Net::Ipv4_address const &dst_ip);

		Handle_pkt_result _handle_arp_reply(Net::Arp_packet &arp);

		void _handle_link_state();

	public:

		Nic_connection(Genode::Env                       &env,
		               Genode::Allocator                 &heap,
		               Genode::Signal_context_capability  pkt_stream_sigh,
		               Genode::Xml_node const            &config_node,
		               Timer::Connection                 &timer,
		               Nic_connection_notifier           &notifier);

		void for_each_rx_packet(Handle_packet_func handle_packet);

		void notify_peer();

		void send_wg_prot(Genode::uint8_t const *wg_prot_base,
		                  Genode::size_t         wg_prot_size,
		                  Genode::uint16_t       udp_src_port_big_endian,
		                  Genode::uint16_t       udp_dst_port_big_endian,
		                  Genode::uint32_t       ipv4_src_addr_big_endian,
		                  Genode::uint32_t       ipv4_dst_addr_big_endian,
		                  Genode::uint8_t        ipv4_dscp_ecn,
		                  Genode::uint8_t        ipv4_ttl);

		Genode::Microseconds dhcp_discover_timeout() const { return Genode::Microseconds { 3 * 1000 * 1000 }; }

		Genode::Microseconds dhcp_request_timeout() const { return Genode::Microseconds { 10 * 1000 * 1000 }; }

		Ipv4_config const &ip_config() const { return *_ip_config; }

		bool verbose() const { return _verbose; }

		void discard_ip_config();

		void ip_config_from_dhcp_ack(Net::Dhcp_packet &dhcp_ack);

		Net::Mac_address mac_address() const { return _mac_address; }

		template <typename FUNC>
		Send_pkt_result send(Genode::size_t pkt_size, FUNC && write_to_pkt)
		{
			try {
				Nic::Packet_descriptor  pkt        { _connection.tx()->alloc_packet(pkt_size) };
				void                   *pkt_base   { _connection.tx()->packet_content(pkt) };
				Net::Size_guard         size_guard { pkt_size };

				write_to_pkt(pkt_base, size_guard);
				_connection_tx_flush_acks();
				_connection.tx()->submit_packet(pkt);
			}
			catch (Packet_source::Packet_alloc_failed) {
				if (_verbose) {
					Genode::log("Failed sending NIC packet - Failed allocating packet");
				}
				return Send_pkt_result::FAILED;
			}
			return Send_pkt_result::SUCCEEDED;
		}
};

#endif /* _NIC_CONNECTION_H_ */
