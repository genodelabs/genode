/*
 * \brief  Proxy-ARP session and root component
 * \author Stefan Kalkowski
 * \date   2010-08-18
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _COMPONENT_H_
#define _COMPONENT_H_

/* Genode */
#include <base/lock.h>
#include <root/component.h>
#include <util/arg_string.h>
#include <nic/packet_allocator.h>
#include <nic_session/rpc_object.h>
#include <nic_session/connection.h>
#include <net/ipv4.h>
#include <base/allocator_guard.h>

#include "address_node.h"
#include "mac.h"
#include "packet_handler.h"

namespace Net {

	/**
	 * Helper class.
	 *
	 */
	class Guarded_range_allocator
	{
		private:

			Genode::Allocator_guard _guarded_alloc;
			Nic::Packet_allocator   _range_alloc;

		public:

			Guarded_range_allocator(Genode::Allocator *backing_store,
			                        Genode::size_t     amount)
			: _guarded_alloc(backing_store, amount),
			  _range_alloc(&_guarded_alloc) {}

			Genode::Allocator_guard *guarded_allocator() {
				return &_guarded_alloc; }

			Genode::Range_allocator *range_allocator() {
				return static_cast<Genode::Range_allocator *>(&_range_alloc); }
	};


	class Communication_buffer : Genode::Ram_dataspace_capability
	{
		public:

			Communication_buffer(Genode::size_t size)
			: Genode::Ram_dataspace_capability(Genode::env()->ram_session()->alloc(size))
			{ }

			~Communication_buffer() { Genode::env()->ram_session()->free(*this); }

			Genode::Dataspace_capability dataspace() { return *this; }
	};


	class Tx_rx_communication_buffers
	{
		private:

			Communication_buffer _tx_buf, _rx_buf;

		public:

			Tx_rx_communication_buffers(Genode::size_t tx_size,
			                            Genode::size_t rx_size)
			: _tx_buf(tx_size), _rx_buf(rx_size) { }

			Genode::Dataspace_capability tx_ds() { return _tx_buf.dataspace(); }
			Genode::Dataspace_capability rx_ds() { return _rx_buf.dataspace(); }
	};


	/**
	 * Nic-session component class
	 *
	 * We must inherit here from Guarded_range_allocator, although aggregation
	 * would be more convinient, because the range-allocator needs to be initialized
	 * before base-class Session_rpc_object.
	 */
	class Session_component : public  Guarded_range_allocator,
	                          private Tx_rx_communication_buffers,
	                          public  Nic::Session_rpc_object
	{
		private:

			class Tx_handler : public Packet_handler
			{
				private:

					Packet_descriptor  _tx_packet;
					Session_component *_component;

					void acknowledge_last_one();
					void next_packet(void** src, Genode::size_t *size);
					bool handle_arp(Ethernet_frame *eth, Genode::size_t size);
					bool handle_ip(Ethernet_frame *eth, Genode::size_t size);
					void finalize_packet(Ethernet_frame *eth, Genode::size_t size);

				public:

					Tx_handler(Nic::Connection   *session,
					           Session_component *component)
					: Packet_handler(session), _component(component) {}
			};


			Tx_handler         _tx_handler;
			Mac_address_node   _mac_node;
			Ipv4_address_node *_ipv4_node;
			Genode::Lock       _rx_lock;

			void _free_ipv4_node();

		public:

			/**
			 * Constructor
			 *
			 * \param allocator    backing store for guarded allocator
			 * \param amount       amount of memory managed by guarded allocator
			 * \param tx_buf_size  buffer size for tx channel
			 * \param rx_buf_size  buffer size for rx channel
			 * \param vmac         virtual mac address
			 * \param ep           entry point used for packet stream
			 */
			Session_component(Genode::Allocator          *allocator,
			                  Genode::size_t              amount,
			                  Genode::size_t              tx_buf_size,
			                  Genode::size_t              rx_buf_size,
			                  Ethernet_frame::Mac_address vmac,
			                  Nic::Connection            *session,
			                  Genode::Rpc_entrypoint     &ep);

			~Session_component();

			Nic::Session::Tx::Sink*   tx_sink()   { return _tx.sink();   }
			Nic::Session::Rx::Source* rx_source() { return _rx.source(); }
			Genode::Lock*             rx_lock()   { return &_rx_lock;    }

			Nic::Mac_address mac_address()
			{
				Nic::Mac_address m;
				Mac_address_node::Address mac = _mac_node.addr();
				Genode::memcpy(&m, mac.addr, sizeof(m.addr));
				return m;
			}

			void set_ipv4_address(Ipv4_packet::Ipv4_address ip_addr);
	};


	/*
	 * Root component, handling new session requests.
	 */
	class Root : public Genode::Root_component<Session_component>
	{
		private:

			Mac_allocator           _mac_alloc;
			Nic::Connection        *_session;
			Genode::Rpc_entrypoint &_ep;

		protected:

			Session_component *_create_session(const char *args)
			{
				using namespace Genode;

				size_t ram_quota =
					Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);
				size_t tx_buf_size =
					Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);
				size_t rx_buf_size =
					Arg_string::find_arg(args, "rx_buf_size").ulong_value(0);

				/* delete ram quota by the memory needed for the session */
				size_t session_size = max((size_t)4096, sizeof(Session_component));
				if (ram_quota < session_size)
					throw Root::Quota_exceeded();

				/*
				 * Check if donated ram quota suffices for both
				 * communication buffers. Also check both sizes separately
				 * to handle a possible overflow of the sum of both sizes.
				 */
				if (tx_buf_size                  > ram_quota - session_size
					|| rx_buf_size               > ram_quota - session_size
					|| tx_buf_size + rx_buf_size > ram_quota - session_size) {
					PERR("insufficient 'ram_quota', got %zd, need %zd",
					     ram_quota, tx_buf_size + rx_buf_size + session_size);
					throw Root::Quota_exceeded();
				}

				try {
					return new (md_alloc()) Session_component(env()->heap(),
					                                          ram_quota - session_size,
					                                          tx_buf_size,
					                                          rx_buf_size,
					                                          _mac_alloc.alloc(),
					                                          _session,
					                                          _ep);
				} catch(Mac_allocator::Alloc_failed) {
					PWRN("Mac address allocation failed!");
					return (Session_component*) 0;
				}
			}

		public:

			Root(Genode::Rpc_entrypoint *session_ep,
			     Genode::Allocator      *md_alloc,
			     Nic::Connection        *session)
			: Genode::Root_component<Session_component>(session_ep, md_alloc),
			  _session(session), _ep(*session_ep) { }
	};

} /* namespace Net */

#endif /* _COMPONENT_H_ */
