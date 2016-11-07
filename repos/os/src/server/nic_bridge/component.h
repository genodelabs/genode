/*
 * \brief  Proxy-ARP session and root component
 * \author Stefan Kalkowski
 * \date   2010-08-18
 */

/*
 * Copyright (C) 2010-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _COMPONENT_H_
#define _COMPONENT_H_

/* Genode */
#include <base/log.h>
#include <base/heap.h>
#include <nic/packet_allocator.h>
#include <nic_session/rpc_object.h>
#include <nic_session/connection.h>
#include <os/session_policy.h>
#include <root/component.h>
#include <util/arg_string.h>

#include <address_node.h>
#include <mac.h>
#include <nic.h>
#include <packet_handler.h>
#include <ram_session_guard.h>

namespace Net {
	class Stream_allocator;
	class Stream_dataspace;
	class Stream_dataspaces;
	class Session_component;
	class Root;
}


/********************
 ** Helper classes **
 ********************/

class Net::Stream_allocator
{
	protected:

		Genode::Ram_session_guard _ram;
		Genode::Heap              _heap;
		::Nic::Packet_allocator   _range_alloc;

	public:

		Stream_allocator(Genode::Ram_session &ram,
		                 Genode::Region_map  &rm,
		                 Genode::size_t     amount)
		: _ram(ram, amount),
		  _heap(ram, rm),
		  _range_alloc(&_heap) {}

		Genode::Range_allocator *range_allocator() {
			return static_cast<Genode::Range_allocator *>(&_range_alloc); }
};


struct Net::Stream_dataspace : Genode::Ram_dataspace_capability
{
	Genode::Ram_session &ram;

	Stream_dataspace(Genode::Ram_session &ram, Genode::size_t size)
	: Genode::Ram_dataspace_capability(ram.alloc(size)), ram(ram) { }

	~Stream_dataspace() { ram.free(*this); }
};


struct Net::Stream_dataspaces
{
	Stream_dataspace tx_ds, rx_ds;

	Stream_dataspaces(Genode::Ram_session &ram, Genode::size_t tx_size,
	                  Genode::size_t rx_size)
	: tx_ds(ram, tx_size), rx_ds(ram, rx_size) { }
};


/**
 * Nic-session component class
 *
 * We must inherit here from Stream_allocator, although aggregation
 * would be more convinient, because the allocator needs to be initialized
 * before base-class Session_rpc_object.
 */
class Net::Session_component : public  Net::Stream_allocator,
                               private Net::Stream_dataspaces,
                               public  ::Nic::Session_rpc_object,
                               public  Net::Packet_handler
{
	private:

		Mac_address_node                  _mac_node;
		Ipv4_address_node                 _ipv4_node;
		Net::Nic                         &_nic;
		Genode::Signal_context_capability _link_state_sigh;

		void _unset_ipv4_node();

	public:

		/**
		 * Constructor
		 *
		 * \param ram          ram session
		 * \param rm           region map of this component
		 * \param ep           entry point this session component is handled by
		 * \param amount       amount of memory managed by guarded allocator
		 * \param tx_buf_size  buffer size for tx channel
		 * \param rx_buf_size  buffer size for rx channel
		 * \param vmac         virtual mac address
		 */
		Session_component(Genode::Ram_session &ram,
		                  Genode::Region_map  &rm,
		                  Genode::Entrypoint  &ep,
		                  Genode::size_t       amount,
		                  Genode::size_t       tx_buf_size,
		                  Genode::size_t       rx_buf_size,
		                  Mac_address          vmac,
		                  Net::Nic            &nic,
		                  char                *ip_addr = 0);

		~Session_component();

		::Nic::Mac_address mac_address()
		{
			::Nic::Mac_address m;
			Mac_address_node::Address mac = _mac_node.addr();
			Genode::memcpy(&m, mac.addr, sizeof(m.addr));
			return m;
		}

		void link_state_changed()
		{
			if (_link_state_sigh.valid())
				Genode::Signal_transmitter(_link_state_sigh).submit();
		}

		void set_ipv4_address(Ipv4_address ip_addr);


		/****************************************
		 ** Nic::Driver notification interface **
		 ****************************************/

		bool link_state();

		void link_state_sigh(Genode::Signal_context_capability sigh) {
			_link_state_sigh = sigh; }


		/******************************
		 ** Packet_handler interface **
		 ******************************/

		Packet_stream_sink< ::Nic::Session::Policy>   * sink()   {
			return _tx.sink(); }

		Packet_stream_source< ::Nic::Session::Policy> * source() {
			return _rx.source(); }

		bool handle_arp(Ethernet_frame *eth,      Genode::size_t size);
		bool handle_ip(Ethernet_frame *eth,       Genode::size_t size);
		void finalize_packet(Ethernet_frame *eth, Genode::size_t size);
};


/*
 * Root component, handling new session requests.
 */
class Net::Root : public Genode::Root_component<Net::Session_component>
{
	private:

		Mac_allocator     _mac_alloc;
		Genode::Env      &_env;
		Net::Nic         &_nic;
		Genode::Xml_node  _config;

	protected:

		Session_component *_create_session(const char *args)
		{
			using namespace Genode;

			enum { MAX_IP_ADDR_LENGTH  = 16, };
			char ip_addr[MAX_IP_ADDR_LENGTH];
			memset(ip_addr, 0, MAX_IP_ADDR_LENGTH);

			 try {
				Session_label const label = label_from_args(args);
				Session_policy policy(label, _config);
				policy.attribute("ip_addr").value(ip_addr, sizeof(ip_addr));
			} catch (Xml_node::Nonexistent_attribute) {
				Genode::log("Missing \"ip_addr\" attribute in policy definition");
			} catch (Session_policy::No_policy_defined) { }

			size_t ram_quota =
				Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);
			size_t tx_buf_size =
				Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);
			size_t rx_buf_size =
				Arg_string::find_arg(args, "rx_buf_size").ulong_value(0);

			try {
				return new (md_alloc())
					Session_component(_env.ram(), _env.rm(), _env.ep(),
					                  ram_quota, tx_buf_size, rx_buf_size,
					                  _mac_alloc.alloc(), _nic, ip_addr);
			} catch(Mac_allocator::Alloc_failed) {
				Genode::warning("Mac address allocation failed!");
				throw Root::Unavailable();
			} catch(Ram_session::Quota_exceeded) {
				Genode::warning("insufficient 'ram_quota'");
				throw Root::Quota_exceeded();
			}
		}

	public:

		Root(Genode::Env &env, Net::Nic &nic, Genode::Allocator &md_alloc,
		     Genode::Xml_node config)
		: Genode::Root_component<Session_component>(env.ep(), md_alloc),
		  _env(env), _nic(nic), _config(config) { }
};

#endif /* _COMPONENT_H_ */
