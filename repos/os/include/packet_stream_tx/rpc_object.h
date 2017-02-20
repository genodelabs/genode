/*
 * \brief  Server-side interface for packet-stream transmission
 * \author Norman Feske
 * \date   2010-03-01
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__PACKET_STREAM_TX__RPC_OBJECT_H_
#define _INCLUDE__PACKET_STREAM_TX__RPC_OBJECT_H_

#include <packet_stream_tx/packet_stream_tx.h>
#include <base/rpc_server.h>

namespace Packet_stream_tx { template <typename> class Rpc_object; }


template <typename CHANNEL>
class Packet_stream_tx::Rpc_object : public Genode::Rpc_object<CHANNEL, Rpc_object<CHANNEL> >
{
	private:

		Genode::Rpc_entrypoint     &_ep;
		Genode::Capability<CHANNEL> _cap;
		typename CHANNEL::Sink      _sink;

		Genode::Signal_context_capability _sigh_ready_to_ack;
		Genode::Signal_context_capability _sigh_packet_avail;

	public:

		/**
		 * Constructor
		 *
		 * \param ds  dataspace used as communication buffer
		 *            for the transmission packet stream
		 * \param ep  entry point used for serving the channel's RPC
		 *            interface
		 */
		Rpc_object(Genode::Dataspace_capability ds,
		           Genode::Region_map     &rm,
		           Genode::Rpc_entrypoint &ep)
		:
			_ep(ep), _cap(_ep.manage(this)), _sink(ds, rm),

			/* init signal handlers with default handlers of sink */
			_sigh_ready_to_ack(_sink.sigh_ready_to_ack()),
			_sigh_packet_avail(_sink.sigh_packet_avail())
		{ }

		/**
		 * Destructor
		 */
		~Rpc_object() { _ep.dissolve(this); }

		/*
		 * The 'sigh_packet_avail()' and 'sigh_ready_to_ack()' methods
		 * may be called at session-creation time to override the default
		 * data-flow signal handlers as provided by the packet-stream sink.
		 * The default handlers let the server block in the event of data
		 * congestion. By installing custom signal handlers, a server
		 * implementation is able to avoid blocking for a single event by
		 * facilitating the use of a select-like mode of operation.
		 *
		 * Note that calling these methods after the finished creation of
		 * the session has no effect because the client queries the signal
		 * handlers only once at session-creation time.
		 */

		/**
		 * Override default handler for server-side ready-to-ack signals
		 */
		void sigh_ready_to_ack(Genode::Signal_context_capability sigh) {
			_sigh_ready_to_ack = sigh; }

		/**
		 * Override default handler for server-side packet-avail signals
		 *
		 * Must be called at constuction time only.
		 */
		void sigh_packet_avail(Genode::Signal_context_capability sigh) {
			_sigh_packet_avail = sigh; }

		typename CHANNEL::Sink *sink() { return &_sink; }

		Genode::Capability<CHANNEL> cap() const { return _cap; }


		/*******************
		 ** RPC functions **
		 *******************/

		Genode::Dataspace_capability dataspace() { return _sink.dataspace(); }

		void sigh_ready_to_submit(Genode::Signal_context_capability sigh) {
			_sink.register_sigh_ready_to_submit(sigh); }

		void sigh_ack_avail(Genode::Signal_context_capability sigh) {
			_sink.register_sigh_ack_avail(sigh); }

		virtual Genode::Signal_context_capability sigh_ready_to_ack() {
			return _sigh_ready_to_ack; }

		virtual Genode::Signal_context_capability sigh_packet_avail() {
			return _sigh_packet_avail; }
};

#endif /* _INCLUDE__PACKET_STREAM_TX__RPC_OBJECT_H_ */
