/*
 * \brief  Client-side interface for packet-stream reception
 * \author Norman Feske
 * \date   2010-03-01
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__PACKET_STREAM_TX__CLIENT_H_
#define _INCLUDE__PACKET_STREAM_TX__CLIENT_H_

#include <packet_stream_tx/packet_stream_tx.h>
#include <base/rpc_client.h>

namespace Packet_stream_tx { template <typename> class Client; }


template <typename CHANNEL>
class Packet_stream_tx::Client : public Genode::Rpc_client<CHANNEL>
{
	private:

		/*
		 * Type shortcuts
		 */
		typedef Genode::Rpc_client<CHANNEL>        Base;
		typedef typename Base::Rpc_dataspace       Rpc_dataspace;
		typedef typename Base::Rpc_packet_avail    Rpc_packet_avail;
		typedef typename Base::Rpc_ready_to_ack    Rpc_ready_to_ack;
		typedef typename Base::Rpc_ready_to_submit Rpc_ready_to_submit;
		typedef typename Base::Rpc_ack_avail       Rpc_ack_avail;

		/**
		 * Packet-stream source
		 */
		typename CHANNEL::Source _source;

	public:

		/**
		 * Constructor
		 *
		 * \param buffer_alloc  allocator used for managing the
		 *                      transmission buffer
		 */
		Client(Genode::Capability<CHANNEL> channel_cap,
		       Genode::Region_map &rm,
		       Genode::Range_allocator &buffer_alloc)
		:
			Genode::Rpc_client<CHANNEL>(channel_cap),
			_source(Base::template call<Rpc_dataspace>(), rm, buffer_alloc)
		{
			/* wire data-flow signals for the packet transmitter */
			_source.register_sigh_packet_avail(Base::template call<Rpc_packet_avail>());
			_source.register_sigh_ready_to_ack(Base::template call<Rpc_ready_to_ack>());
			sigh_ready_to_submit(_source.sigh_ready_to_submit());
			sigh_ack_avail(_source.sigh_ack_avail());
		}

		void sigh_ready_to_submit(Genode::Signal_context_capability sigh) {
			Base::template call<Rpc_ready_to_submit>(sigh); }

		void sigh_ack_avail(Genode::Signal_context_capability sigh) {
			Base::template call<Rpc_ack_avail>(sigh); }

		typename CHANNEL::Source *source() { return &_source; }
};

#endif /* _INCLUDE__PACKET_STREAM_TX__CLIENT_H_ */
