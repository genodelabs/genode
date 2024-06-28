/*
 * \brief  Client-side interface for packet-stream transmission
 * \author Norman Feske
 * \date   2010-03-01
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__PACKET_STREAM_RX__CLIENT_H_
#define _INCLUDE__PACKET_STREAM_RX__CLIENT_H_

#include <packet_stream_rx/packet_stream_rx.h>
#include <base/rpc_client.h>

namespace Packet_stream_rx { template <typename> class Client; }


template <typename CHANNEL>
class Packet_stream_rx::Client : public Genode::Rpc_client<CHANNEL>
{
	private:

		typename CHANNEL::Sink _sink;

		/*
		 * Type shortcuts
		 */
		using Base                = Genode::Rpc_client<CHANNEL>;
		using Rpc_dataspace       = typename Base::Rpc_dataspace;
		using Rpc_packet_avail    = typename Base::Rpc_packet_avail;
		using Rpc_ready_to_ack    = typename Base::Rpc_ready_to_ack;
		using Rpc_ready_to_submit = typename Base::Rpc_ready_to_submit;
		using Rpc_ack_avail       = typename Base::Rpc_ack_avail;

	public:

		/**
		 * Constructor
		 */
		Client(Genode::Capability<CHANNEL> channel_cap,
		       Genode::Region_map &rm)
		:
			Genode::Rpc_client<CHANNEL>(channel_cap),
			_sink(Base::template call<Rpc_dataspace>(), rm)
		{
			/* wire data-flow signals for the packet receiver */
			_sink.register_sigh_ack_avail(Base::template call<Rpc_ack_avail>());
			_sink.register_sigh_ready_to_submit(Base::template call<Rpc_ready_to_submit>());
		}

		void sigh_ready_to_ack(Genode::Signal_context_capability sigh) override {
			Base::template call<Rpc_ready_to_ack>(sigh); }

		void sigh_packet_avail(Genode::Signal_context_capability sigh) override {
			Base::template call<Rpc_packet_avail>(sigh); }

		typename CHANNEL::Sink *sink() override { return &_sink; }
};

#endif /* _INCLUDE__PACKET_STREAM_RX__CLIENT_H_ */
