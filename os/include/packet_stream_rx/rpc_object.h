/*
 * \brief  Server-side interface for packet-stream reception
 * \author Norman Feske
 * \date   2010-03-01
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__PACKET_STREAM_RX__RPC_OBJECT_H_
#define _INCLUDE__PACKET_STREAM_RX__RPC_OBJECT_H_

#include <packet_stream_rx/packet_stream_rx.h>
#include <base/rpc_server.h>

namespace Packet_stream_rx {

	template <typename CHANNEL>
	class Rpc_object : public Genode::Rpc_object<CHANNEL, Rpc_object<CHANNEL> >
	{
		private:

			Genode::Rpc_entrypoint     &_ep;
			Genode::Capability<CHANNEL> _cap;
			typename CHANNEL::Source    _source;

		public:

			/*
			 * Accessors for server-side signal handlers
			 *
			 * The functions are virtual to enable derived classes to supply
			 * custom handlers for data-flow signals.
			 */

			virtual Genode::Signal_context_capability sigh_ready_to_submit() {
				return _source.sigh_ready_to_submit(); }

			virtual Genode::Signal_context_capability sigh_ack_avail() {
				return _source.sigh_ack_avail(); }

			/**
			 * Constructor
			 *
			 * \param ds            dataspace used as communication buffer
			 *                      for the receive packet stream
			 * \param buffer_alloc  allocator used for managing the communication
			 *                      buffer of the receive packet stream
			 * \param ep            entry point used for serving the channel's RPC
			 *                      interface
			 */
			Rpc_object(Genode::Dataspace_capability  ds,
			           Genode::Range_allocator      *buffer_alloc,
			           Genode::Rpc_entrypoint       &ep)
			: _ep(ep), _cap(_ep.manage(this)), _source(buffer_alloc, ds) { }

			/**
			 * Destructor
			 */
			~Rpc_object() { _ep.dissolve(this); }

			Genode::Dataspace_capability dataspace() { return _source.dataspace(); }

			void sigh_ready_to_ack(Genode::Signal_context_capability sigh) {
				_source.register_sigh_ready_to_ack(sigh); }

			void sigh_packet_avail(Genode::Signal_context_capability sigh) {
				_source.register_sigh_packet_avail(sigh); }

			typename CHANNEL::Source *source() { return &_source; }

			Genode::Capability<CHANNEL> cap() const { return _cap; }
	};
}

#endif /* _INCLUDE__PACKET_STREAM_RX__RPC_OBJECT_H_ */
