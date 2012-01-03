/*
 * \brief  Server-side audio-out session interface
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Christian Helmuth
 * \author Stefan Kalkowski
 * \date   2009-12-02
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__AUDIO_OUT_SESSION__RPC_OBJECT_H_
#define _INCLUDE__AUDIO_OUT_SESSION__RPC_OBJECT_H_

#include <base/rpc_server.h>
#include <audio_out_session/audio_out_session.h>
#include <packet_stream_tx/rpc_object.h>

namespace Audio_out {

	class Session_rpc_object : public Genode::Rpc_object<Session, Session_rpc_object>
	{
		private:

			Packet_stream_tx::Rpc_object<Channel> _channel;

		public:

			/**
			 * Constructor
			 *
			 * \param ds  dataspace used as communication buffer
			 *            for the packet stream
			 * \param ep  entry point used for packet-stream channel
			 */
			Session_rpc_object(Genode::Dataspace_capability ds,
			                   Genode::Rpc_entrypoint &ep)
			: _channel(ds, ep) { }

			/**
			 * Return capability to packet-stream channel
			 *
			 * This function is called by the client via an RPC call at session
			 * construction time.
			 */
			Genode::Capability<Channel> channel_cap() { return _channel.cap(); }

			Channel::Sink *channel() { return _channel.sink(); }
	};
}

#endif /* _INCLUDE__AUDIO_OUT_SESSION__RPC_OBJECT_H_ */
