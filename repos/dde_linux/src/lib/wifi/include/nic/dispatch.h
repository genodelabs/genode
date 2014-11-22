/**
 * \brief  Packet-stream-session components
 * \author Sebastian Sumpf
 * \author Norman Feske
 * \date   2012-07-06
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SIGNAL__DISPATCHER_H_
#define _SIGNAL__DISPATCHER_H_

/* Genode includes */
#include <base/signal.h>

/* local includes */
#include <lx.h>


/**
 * Session components that overrides signal handlers
 */
template <typename SESSION_RPC_OBJECT>
class Packet_session_component : public SESSION_RPC_OBJECT
{
	private:

		Genode::Signal_rpc_member<Packet_session_component> _tx_ready_to_ack_dispatcher;
		Genode::Signal_rpc_member<Packet_session_component> _tx_packet_avail_dispatcher;
		Genode::Signal_rpc_member<Packet_session_component> _rx_ack_avail_dispatcher;
		Genode::Signal_rpc_member<Packet_session_component> _rx_ready_to_submit_dispatcher;

		void _tx_ready_to_ack(unsigned)
		{
			_process_packets(0);
		}

		void _tx_packet_avail(unsigned)
		{
			_process_packets(0);
		}

		void _rx_ack_avail(unsigned)
		{
			_process_packets(0);
		}

		void _rx_ready_to_submit(unsigned)
		{
			_process_packets(0);
		}

	protected:

		virtual void _process_packets(unsigned) = 0;

	public:

		Packet_session_component(Genode::Dataspace_capability  tx_ds,
		                         Genode::Dataspace_capability  rx_ds,
		                         Genode::Range_allocator      *rx_buffer_alloc,
		                         Server::Entrypoint           &ep)
		:
			SESSION_RPC_OBJECT(tx_ds, rx_ds, rx_buffer_alloc, ep.rpc_ep()),
			_tx_ready_to_ack_dispatcher(ep, *this, &Packet_session_component::_tx_ready_to_ack),
			_tx_packet_avail_dispatcher(ep, *this, &Packet_session_component::_tx_packet_avail),
			_rx_ack_avail_dispatcher(ep, *this, &Packet_session_component::_rx_ack_avail),
			_rx_ready_to_submit_dispatcher(ep, *this, &Packet_session_component::_rx_ready_to_submit)
		{
			SESSION_RPC_OBJECT::_tx.sigh_ready_to_ack(_tx_ready_to_ack_dispatcher);
			SESSION_RPC_OBJECT::_tx.sigh_packet_avail(_tx_packet_avail_dispatcher);
			SESSION_RPC_OBJECT::_rx.sigh_ack_avail(_rx_ack_avail_dispatcher);
			SESSION_RPC_OBJECT::_rx.sigh_ready_to_submit(_rx_ready_to_submit_dispatcher);
		}
};


/**
 * Root component, handling new session requests
 */
template <typename                ROOT_COMPONENT,
          typename                SESSION_COMPONENT,
          typename                DEVICE,
          Genode::Cache_attribute CACHEABILITY>
class Packet_root : public ROOT_COMPONENT
{
	private:

		Server::Entrypoint  &_ep;
		DEVICE              &_device;

	protected:

		/**
		 * Always returns the singleton block-session component
		 */
		SESSION_COMPONENT *_create_session(const char *args)
		{
			using namespace Genode;
			size_t ram_quota =
				Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);
			size_t tx_buf_size =
				Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);
			size_t rx_buf_size =
				Arg_string::find_arg(args, "rx_buf_size").ulong_value(0);

			/* delete ram quota by the memory needed for the session */
			size_t session_size = max((size_t)4096,
			                          sizeof(SESSION_COMPONENT)
			                          + sizeof(Allocator_avl));
			if (ram_quota < session_size)
				throw Root::Quota_exceeded();

			/*
			 * Check if donated ram quota suffices for both communication
			 * buffers. Also check both sizes separately to handle a
			 * possible overflow of the sum of both sizes.
			 */
			if (tx_buf_size > ram_quota - session_size) {
				PERR("insufficient 'ram_quota', got %zd, need %zd",
				     ram_quota, tx_buf_size + session_size);
				throw Root::Quota_exceeded();
			}

			return new (ROOT_COMPONENT::md_alloc())
				SESSION_COMPONENT(Lx::backend_alloc(tx_buf_size, CACHEABILITY),
				                  Lx::backend_alloc(rx_buf_size, CACHEABILITY),
				                  _ep, _device);
		}

	public:

		Packet_root(Server::Entrypoint &ep, Genode::Allocator *md_alloc, DEVICE &device)
		: ROOT_COMPONENT(&ep.rpc_ep(), md_alloc),
		  _ep(ep), _device(device) { }
};

#endif /* _SIGNAL__DISPATCHER_H_ */
