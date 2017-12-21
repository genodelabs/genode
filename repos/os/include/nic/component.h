/*
 * \brief  Server::Entrypoint based NIC session component
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \date   2015-06-22
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__NIC__COMPONENT_H_
#define _INCLUDE__NIC__COMPONENT_H_

#include <base/attached_ram_dataspace.h>
#include <base/env.h>
#include <nic/packet_allocator.h>
#include <nic_session/rpc_object.h>

namespace Nic {

	class Communication_buffers;
	class Session_component;
}


class Nic::Communication_buffers
{
	protected:

		Nic::Packet_allocator          _rx_packet_alloc;
		Genode::Attached_ram_dataspace _tx_ds, _rx_ds;

		Communication_buffers(Genode::Allocator   &rx_block_md_alloc,
		                      Genode::Ram_session &ram_session,
		                      Genode::Region_map  &region_map,
		                      Genode::size_t       tx_size,
		                      Genode::size_t       rx_size)
		:
			_rx_packet_alloc(&rx_block_md_alloc),
			_tx_ds(ram_session, region_map, tx_size),
			_rx_ds(ram_session, region_map, rx_size)
		{ }
};


class Nic::Session_component : Communication_buffers, public Session_rpc_object
{
	protected:

		Genode::Entrypoint               &_ep;
		Genode::Signal_context_capability _link_state_sigh { };


		/**
		 * Signal link-state change to client
		 */
		void _link_state_changed()
		{
			if (_link_state_sigh.valid())
				Genode::Signal_transmitter(_link_state_sigh).submit();
		}

		/**
		 * Sub-classes must implement this function, it is called upon all
		 * packet-stream signals.
		 */
		virtual void _handle_packet_stream() = 0;

		void _dispatch() { _handle_packet_stream(); }

		Genode::Signal_handler<Session_component> _packet_stream_dispatcher {
			_ep, *this, &Session_component::_dispatch };

	public:

		/**
		 * Constructor
		 *
		 * \param tx_buf_size        buffer size for tx channel
		 * \param rx_buf_size        buffer size for rx channel
		 * \param rx_block_md_alloc  backing store of the meta data of the
		 *                           rx block allocator
		 * \param env                Genode environment needed to access
		 *                           resources and open connections from
		 *                           within the Session_component
		 */
		Session_component(Genode::size_t const tx_buf_size,
		                  Genode::size_t const rx_buf_size,
		                  Genode::Allocator   &rx_block_md_alloc,
		                  Genode::Env         &env)
		:
			Communication_buffers(rx_block_md_alloc, env.ram(), env.rm(),
			                      tx_buf_size, rx_buf_size),
			Session_rpc_object(env.rm(),
			                   _tx_ds.cap(),
			                   _rx_ds.cap(),
			                  &_rx_packet_alloc, env.ep().rpc_ep()),
			_ep(env.ep())
		{
			/* install data-flow signal handlers for both packet streams */
			_tx.sigh_ready_to_ack(_packet_stream_dispatcher);
			_tx.sigh_packet_avail(_packet_stream_dispatcher);
			_rx.sigh_ready_to_submit(_packet_stream_dispatcher);
			_rx.sigh_ack_avail(_packet_stream_dispatcher);
		}

		/**
		 * Constructor
		 *
		 * \param tx_buf_size        buffer size for tx channel
		 * \param rx_buf_size        buffer size for rx channel
		 * \param rx_block_md_alloc  backing store of the meta data of the
		 *                           rx block allocator
		 * \param env                Genode environment needed to access
		 *                           resources and open connections from
		 *                           within the Session_component
		 * \param ep                 entrypoint for RPC
		 */
		Session_component(Genode::size_t const tx_buf_size,
		                  Genode::size_t const rx_buf_size,
		                  Genode::Allocator   &rx_block_md_alloc,
		                  Genode::Env         &env,
		                  Genode::Entrypoint  &ep)
		:
			Communication_buffers(rx_block_md_alloc, env.ram(), env.rm(),
			                      tx_buf_size, rx_buf_size),
			Session_rpc_object(env.rm(),
			                   _tx_ds.cap(),
			                   _rx_ds.cap(),
			                  &_rx_packet_alloc, ep.rpc_ep()),
			                  _ep(ep)
		{
			/* install data-flow signal handlers for both packet streams */
			_tx.sigh_ready_to_ack(_packet_stream_dispatcher);
			_tx.sigh_packet_avail(_packet_stream_dispatcher);
			_rx.sigh_ready_to_submit(_packet_stream_dispatcher);
			_rx.sigh_ack_avail(_packet_stream_dispatcher);
		}

		void link_state_sigh(Genode::Signal_context_capability sigh)
		{
			_link_state_sigh = sigh;
		}

		/**
		 * Return the current link state
		 */
		virtual bool link_state() = 0;

		/**
		 * Return the MAC address of the device
		 */
		virtual Mac_address mac_address() = 0;
};


#endif /* _INCLUDE__NIC__COMPONENT_H_ */
