/*
 * \brief  Client-side audio-out session interface
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

#ifndef _INCLUDE__AUDIO_OUT_SESSION__CLIENT_H_
#define _INCLUDE__AUDIO_OUT_SESSION__CLIENT_H_

#include <base/rpc_client.h>
#include <audio_out_session/audio_out_session.h>
#include <packet_stream_tx/client.h>

namespace Audio_out {

	class Session_client : public Genode::Rpc_client<Session>
	{
		private:

			Packet_stream_tx::Client<Session::Channel> _channel;

			Session_capability _cap;

		public:

			/**
			 * Constructor
			 *
			 * \param session       Audio-out session capability
			 * \param buffer_alloc  allocator used for managing the
			 *                      transmission buffer
			 */
			Session_client(Genode::Capability<Session> session,
			               Genode::Range_allocator *buffer_alloc)
			:
				Genode::Rpc_client<Session>(session),
				_channel(call<Rpc_channel_cap>(), buffer_alloc),
				_cap(session)
			{ }

			/**
			 * Return session capability of this session
			 *
			 * This function is meant to be used to obtain the argument for the
			 * 'sync_session' function called for another 'Audio_out' session.
			 */
			Session_capability session_capability() { return _cap; }

			Session::Channel *channel() { return &_channel; }

			/*********************************
			 ** Audio-out session interface **
			 *********************************/

			Session::Channel::Source *stream() { return _channel.source(); }

			void flush(void) { call<Rpc_flush>(); }

			void sync_session(Session_capability audio_out_session) {
				call<Rpc_sync_session>(audio_out_session); }
	};
}

#endif /* _INCLUDE__AUDIO_OUT_SESSION__CLIENT_H_ */
