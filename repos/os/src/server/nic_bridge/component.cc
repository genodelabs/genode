/*
 * \brief  Proxy-ARP session and root component
 * \author Stefan Kalkowski
 * \date   2010-08-18
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode */
#include <net/arp.h>
#include <net/dhcp.h>
#include <net/udp.h>

#include <component.h>

using namespace Net;
using namespace Genode;


bool Session_component::handle_arp(Ethernet_frame &eth,
                                   Size_guard     &size_guard)
{
	Arp_packet &arp = eth.data<Arp_packet>(size_guard);
	if (arp.ethernet_ipv4() &&
		arp.opcode() == Arp_packet::REQUEST) {

		/*
		 * 'Gratuitous ARP' broadcast messages are used to announce newly created
		 * IP<->MAC address mappings to other hosts. nic_bridge-internal hosts
		 * would expect a nic_bridge-internal MAC address in this message, whereas
		 * external hosts would expect the NIC's MAC address in this message.
		 * The simplest solution to this problem is to just drop those messages,
		 * since they are not really necessary.
		 */
		 if (arp.src_ip() == arp.dst_ip())
			return false;

		Ipv4_address_node *node = vlan().ip_tree.first();
		if (node)
			node = node->find_by_address(arp.dst_ip());
		if (!node) {
			arp.src_mac(_nic.mac());
		}
	}
	return true;
}


bool Session_component::handle_ip(Ethernet_frame &eth,
                                  Size_guard     &size_guard)
{
	Ipv4_packet &ip = eth.data<Ipv4_packet>(size_guard);
	if (ip.protocol() == Ipv4_packet::Protocol::UDP) {

		Udp_packet &udp = ip.data<Udp_packet>(size_guard);
		if (Dhcp_packet::is_dhcp(&udp)) {

			Dhcp_packet &dhcp = udp.data<Dhcp_packet>(size_guard);
			if (dhcp.op() == Dhcp_packet::REQUEST) {

				dhcp.broadcast(true);
				udp.update_checksum(ip.src(), ip.dst());
			}
		}
	}
	return true;
}


void Session_component::finalize_packet(Ethernet_frame *eth,
                                        Genode::size_t  size)
{
	Mac_address_node *node = vlan().mac_tree.first();
	if (node)
		node = node->find_by_address(eth->dst());
	if (node)
		node->component().send(eth, size);
	else {
		/* set our MAC as sender */
		eth->src(_nic.mac());
		_nic.send(eth, size);
	}
}


void Session_component::_unset_ipv4_node()
{
	Ipv4_address_node * first = vlan().ip_tree.first();
	if (!first) return;
	if (first->find_by_address(_ipv4_node.addr()))
		vlan().ip_tree.remove(&_ipv4_node);
}


bool Session_component::link_state() { return _nic.link_state(); }


void Session_component::set_ipv4_address(Ipv4_address ip_addr)
{
	_unset_ipv4_node();
	_ipv4_node.addr(ip_addr);
	vlan().ip_tree.insert(&_ipv4_node);
}


Session_component::Session_component(Genode::Ram_allocator       &ram,
                                     Genode::Region_map          &rm,
                                     Genode::Entrypoint          &ep,
                                     Genode::Ram_quota            ram_quota,
                                     Genode::Cap_quota            cap_quota,
                                     Genode::size_t               tx_buf_size,
                                     Genode::size_t               rx_buf_size,
                                     Mac_address                  vmac,
                                     Net::Nic                    &nic,
                                     bool                  const &verbose,
                                     Genode::Session_label const &label,
                                     Ip_addr               const &ip_addr)
: Stream_allocator(ram, rm, ram_quota, cap_quota),
  Stream_dataspaces(ram, tx_buf_size, rx_buf_size),
  Session_rpc_object(rm,
                     Stream_dataspaces::tx_ds,
                     Stream_dataspaces::rx_ds,
                     Stream_allocator::range_allocator(), ep.rpc_ep()),
  Packet_handler(ep, nic.vlan(), label, verbose),
  _mac_node(*this, vmac),
  _ipv4_node(*this),
  _nic(nic)
{
	vlan().mac_tree.insert(&_mac_node);
	vlan().mac_list.insert(&_mac_node);

	/* static IP parsing */
	if (ip_addr.valid()) {
		Ipv4_address ip = Ipv4_packet::ip_from_string(ip_addr.string());

		if (ip == Ipv4_address()) {
			Genode::warning("Empty or error IP address. Skipped.");
		} else {
			set_ipv4_address(ip);
			Genode::log("vmac = ", vmac, " ip = ", ip);
		}
	}

	_tx.sigh_ready_to_ack(_sink_ack);
	_tx.sigh_packet_avail(_sink_submit);
	_rx.sigh_ack_avail(_source_ack);
	_rx.sigh_ready_to_submit(_source_submit);
}


Session_component::~Session_component() {
	vlan().mac_tree.remove(&_mac_node);
	vlan().mac_list.remove(&_mac_node);
	_unset_ipv4_node();
}


Net::Root::Root(Genode::Env       &env,
                Net::Nic          &nic,
                Genode::Allocator &md_alloc,
                bool        const &verbose,
                Genode::Xml_node   config)
:
	Genode::Root_component<Session_component>(env.ep(), md_alloc),
	_mac_alloc(Mac_address(config.attribute_value("mac", Mac_address(DEFAULT_MAC)))),
	_env(env),
	_nic(nic),
	_config(config),
	_verbose(verbose)
{ }
