/*
 * \brief  Downlink interface in form of a NIC session component
 * \author Martin Stein
 * \date   2016-08-23
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _COMPONENT_H_
#define _COMPONENT_H_

/* Genode includes */
#include <root/component.h>
#include <nic/packet_allocator.h>
#include <nic_session/rpc_object.h>
#include <net/ipv4.h>
#include <base/allocator_guard.h>

/* local includes */
#include <mac_allocator.h>
#include <interface.h>

namespace Net {

	class Port_allocator;
	class Guarded_range_allocator;
	class Communication_buffer;
	class Tx_rx_communication_buffers;
	class Session_component;
	class Root;
}


class Net::Guarded_range_allocator
{
	private:

		Genode::Allocator_guard _guarded_alloc;
		Nic::Packet_allocator   _range_alloc;

	public:

		Guarded_range_allocator(Genode::Allocator   &backing_store,
		                        Genode::size_t const amount);


		/***************
		 ** Accessors **
		 ***************/

		Genode::Allocator_guard &guarded_allocator() { return _guarded_alloc; }
		Genode::Range_allocator &range_allocator();
};


struct Net::Communication_buffer : Genode::Ram_dataspace_capability
{
	Communication_buffer(Genode::size_t const size);

	~Communication_buffer() { Genode::env()->ram_session()->free(*this); }

	Genode::Dataspace_capability dataspace() { return *this; }
};


class Net::Tx_rx_communication_buffers
{
	private:

		Communication_buffer _tx_buf, _rx_buf;

	public:

		Tx_rx_communication_buffers(Genode::size_t const tx_size,
		                            Genode::size_t const rx_size);

		Genode::Dataspace_capability tx_ds() { return _tx_buf.dataspace(); }
		Genode::Dataspace_capability rx_ds() { return _rx_buf.dataspace(); }
};


class Net::Session_component : public  Guarded_range_allocator,
                               private Tx_rx_communication_buffers,
                               public  ::Nic::Session_rpc_object,
                               public  Interface
{
	private:

		void _arp_broadcast(Interface &interface, Ipv4_address ip_addr);

	public:

		Session_component(Genode::Allocator  &allocator,
		                  Genode::size_t      amount,
		                  Genode::size_t      tx_buf_size,
		                  Genode::size_t      rx_buf_size,
		                  Mac_address         vmac,
		                  Server::Entrypoint &ep,
		                  Mac_address         router_mac,
		                  Ipv4_address        router_ip,
		                  char const         *args,
		                  Port_allocator     &tcp_port_alloc,
		                  Port_allocator     &udp_port_alloc,
		                  Tcp_proxy_list     &tcp_proxys,
		                  Udp_proxy_list     &udp_proxys,
		                  unsigned            rtt_sec,
		                  Interface_tree     &interface_tree,
		                  Arp_cache          &arp_cache,
		                  Arp_waiter_list    &arp_waiters,
		                  bool                verbose);

		~Session_component();


		/******************
		 ** Nic::Session **
		 ******************/

		Mac_address mac_address() { return Interface::mac(); }
		bool link_state();
		void link_state_sigh(Genode::Signal_context_capability sigh);


		/********************
		 ** Net::Interface **
		 ********************/

		Packet_stream_sink<Nic::Session::Policy>   *sink()   { return _tx.sink(); }
		Packet_stream_source<Nic::Session::Policy> *source() { return _rx.source(); }
};


class Net::Root : public Genode::Root_component<Session_component>
{
	private:

		Mac_allocator       _mac_alloc;
		Server::Entrypoint &_ep;
		Mac_address         _router_mac;
		Port_allocator     &_tcp_port_alloc;
		Port_allocator     &_udp_port_alloc;
		Tcp_proxy_list     &_tcp_proxys;
		Udp_proxy_list     &_udp_proxys;
		unsigned            _rtt_sec;
		Interface_tree     &_interface_tree;
		Arp_cache          &_arp_cache;
		Arp_waiter_list    &_arp_waiters;
		bool                _verbose;


		/********************
		 ** Root_component **
		 ********************/

		Session_component *_create_session(const char *args);

	public:

		Root(Server::Entrypoint &ep,
		     Genode::Allocator  &md_alloc,
		     Mac_address         router_mac,
		     Port_allocator     &tcp_port_alloc,
		     Port_allocator     &udp_port_alloc,
		     Tcp_proxy_list     &tcp_proxys,
		     Udp_proxy_list     &udp_proxys,
		     unsigned            rtt_sec,
		     Interface_tree     &interface_tree,
		     Arp_cache          &arp_cache,
		     Arp_waiter_list    &arp_waiters,
		     bool                verbose);
};

#endif /* _COMPONENT_H_ */
