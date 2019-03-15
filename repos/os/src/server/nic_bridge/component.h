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

#ifndef _COMPONENT_H_
#define _COMPONENT_H_

/* Genode includes */
#include <base/log.h>
#include <base/heap.h>
#include <base/ram_allocator.h>
#include <nic/packet_allocator.h>
#include <nic_session/rpc_object.h>
#include <nic_session/connection.h>
#include <os/session_policy.h>
#include <root/component.h>
#include <util/arg_string.h>

/* NIC router includes */
#include <mac_allocator.h>

/* local includes */
#include <address_node.h>
#include <nic.h>
#include <packet_handler.h>

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

		Genode::Ram_quota_guard           _ram_guard;
		Genode::Cap_quota_guard           _cap_guard;
		Genode::Constrained_ram_allocator _ram;
		Genode::Heap                      _heap;
		::Nic::Packet_allocator           _range_alloc;

	public:

		Stream_allocator(Genode::Ram_allocator &ram,
		                 Genode::Region_map    &rm,
		                 Genode::Ram_quota      ram_quota,
		                 Genode::Cap_quota      cap_quota)
		:
			_ram_guard(ram_quota), _cap_guard(cap_quota),
			_ram(ram, _ram_guard, _cap_guard),
			_heap(ram, rm),
			_range_alloc(&_heap)
		{ }

		Genode::Range_allocator *range_allocator() {
			return static_cast<Genode::Range_allocator *>(&_range_alloc); }
};


struct Net::Stream_dataspace : Genode::Ram_dataspace_capability
{
	Genode::Ram_allocator &ram;

	Stream_dataspace(Genode::Ram_allocator &ram, Genode::size_t size)
	: Genode::Ram_dataspace_capability(ram.alloc(size)), ram(ram) { }

	~Stream_dataspace() { ram.free(*this); }
};


struct Net::Stream_dataspaces
{
	Stream_dataspace tx_ds, rx_ds;

	Stream_dataspaces(Genode::Ram_allocator &ram, Genode::size_t tx_size,
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
class Net::Session_component : private Net::Stream_allocator,
                               private Net::Stream_dataspaces,
                               public  ::Nic::Session_rpc_object,
                               public  Net::Packet_handler
{
	private:

		Mac_address_node                  _mac_node;
		Ipv4_address_node                 _ipv4_node;
		Net::Nic                         &_nic;
		Genode::Signal_context_capability _link_state_sigh { };

		void _unset_ipv4_node();

	public:

		typedef Genode::String<32> Ip_addr;

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
		Session_component(Genode::Ram_allocator       &ram,
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
		                  Ip_addr               const &ip_addr);

		~Session_component();

		::Nic::Mac_address mac_address() override
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

		bool link_state() override;

		void link_state_sigh(Genode::Signal_context_capability sigh) override {
			_link_state_sigh = sigh; }


		/******************************
		 ** Packet_handler interface **
		 ******************************/

		Packet_stream_sink< ::Nic::Session::Policy> * sink() override {
			return _tx.sink(); }

		Packet_stream_source< ::Nic::Session::Policy> * source() override {
			return _rx.source(); }


		bool handle_arp(Ethernet_frame &eth,
		                Size_guard     &size_guard) override;

		bool handle_ip(Ethernet_frame &eth,
		               Size_guard     &size_guard) override;

		void finalize_packet(Ethernet_frame *, Genode::size_t) override;

		Mac_address vmac() const { return _mac_node.addr(); }
};


/*
 * Root component, handling new session requests.
 */
class Net::Root : public Genode::Root_component<Net::Session_component>
{
	private:

		enum { DEFAULT_MAC = 0x02 };

		Mac_allocator     _mac_alloc;
		Genode::Env      &_env;
		Net::Nic         &_nic;
		Genode::Xml_node  _config;
		bool       const &_verbose;

	protected:

		Session_component *_create_session(const char *args) override
		{
			using namespace Genode;

			Session_label  const label  { label_from_args(args) };
			Session_policy const policy { label, _config };
			Mac_address          mac    { policy.attribute_value("mac", Mac_address()) };

			if (mac == Mac_address()) {
				try { mac = _mac_alloc.alloc(); }
				catch (Mac_allocator::Alloc_failed) {
					Genode::warning("MAC address allocation failed!");
					throw Service_denied();
				}
			} else if (_mac_alloc.mac_managed_by_allocator(mac)) {
				Genode::warning("MAC address already in use");
				throw Service_denied();
			}

			return new (md_alloc())
				Session_component(_env.ram(), _env.rm(), _env.ep(),
				                  ram_quota_from_args(args),
				                  cap_quota_from_args(args),
				                  Arg_string::find_arg(args, "tx_buf_size").ulong_value(0),
				                  Arg_string::find_arg(args, "rx_buf_size").ulong_value(0),
				                  mac, _nic, _verbose, label,
				                  policy.attribute_value("ip_addr", Session_component::Ip_addr()));
		}

		
		void _destroy_session(Session_component *session) override
		{
			_mac_alloc.free(session->vmac());
			Genode::destroy(md_alloc(), session);
		}

	public:

		Root(Genode::Env             &env,
		     Net::Nic                &nic,
		     Genode::Allocator       &md_alloc,
		     bool              const &verbose,
		     Genode::Xml_node         config);
};

#endif /* _COMPONENT_H_ */
