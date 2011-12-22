/*
 * \brief  Server-side interface for packet-stream transmission
 * \author Norman Feske
 * \date   2010-03-01
 */

/*
 * Copyright (C) 2010-2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__PACKET_STREAM_TX__RPC_OBJECT_H_
#define _INCLUDE__PACKET_STREAM_TX__RPC_OBJECT_H_

#include <packet_stream_tx/packet_stream_tx.h>
#include <base/rpc_server.h>

namespace Packet_stream_tx {

	template <typename CHANNEL>
	class Rpc_object : public Genode::Rpc_object<CHANNEL, Rpc_object<CHANNEL> >
	{
		private:

			Genode::Rpc_entrypoint     &_ep;
			Genode::Capability<CHANNEL> _cap;
			typename CHANNEL::Sink      _sink;

		public:

			/*
			 * Accessors for server-side signal handlers
			 *
			 * The functions are virtual to enable derived classes to supply
			 * custom handlers for data-flow signals.
			 */

			virtual Genode::Signal_context_capability sigh_ready_to_ack() {
				return _sink.sigh_ready_to_ack(); }

			virtual Genode::Signal_context_capability sigh_packet_avail() {
				return _sink.sigh_packet_avail(); }

			/**
			 * Constructor
			 *
			 * \param ds  dataspace used as communication buffer
			 *            for the transmission packet stream
			 * \param ep  entry point used for serving the channel's RPC
			 *            interface
			 */
			Rpc_object(Genode::Dataspace_capability ds,
			           Genode::Rpc_entrypoint &ep)
			: _ep(ep), _cap(_ep.manage(this)), _sink(ds) { }

			/**
			 * Destructor
			 */
			~Rpc_object() { _ep.dissolve(this); }

			Genode::Dataspace_capability dataspace() { return _sink.dataspace(); }

			void sigh_ready_to_submit(Genode::Signal_context_capability sigh) {
				_sink.register_sigh_ready_to_submit(sigh); }

			void sigh_ack_avail(Genode::Signal_context_capability sigh) {
				_sink.register_sigh_ack_avail(sigh); }

			typename CHANNEL::Sink *sink() { return &_sink; }

			Genode::Capability<CHANNEL> cap() const { return _cap; }
	};
}

#endif /* _INCLUDE__PACKET_STREAM_TX__RPC_OBJECT_H_ */
