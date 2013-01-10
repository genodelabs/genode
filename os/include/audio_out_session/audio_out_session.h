/*
 * \brief  Audio-out session interface
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Christian Helmuth
 * \author Stefan Kalkowski
 * \date   2009-12-02
 *
 * A audio-out session corresponds to one output channel, which can be used to
 * transmit audio frames. Payload is communicated over the packet-stream
 * interface set up between 'Session_client' and 'Session_server'. The term
 * _channel_ means literally one audio channel, e.g. front left or rear center.
 * Therefore, a standard two-channel stereo track needs two audio-out sessions
 * - one for "front left" and one for "front right". The channel format is
 * FLOAT_LE currently.
 *
 * Audio channel identifiers (loosly related to WAV channels) are:
 *
 * * Front left, right, center
 * * LFE (low frequency effets, subwoofer)
 * * Rear left, right, center
 *
 * For example, consumer-oriented 6-channel (5.1) audio uses front
 * left/right/center, rear left/right and LFE.
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__AUDIO_OUT_SESSION__AUDIO_OUT_SESSION_H_
#define _INCLUDE__AUDIO_OUT_SESSION__AUDIO_OUT_SESSION_H_

#include <base/signal.h>
#include <base/rpc.h>
#include <dataspace/capability.h>
#include <audio_out_session/capability.h>
#include <packet_stream_tx/packet_stream_tx.h>
#include <session/session.h>

namespace Audio_out {

	enum {
		QUEUE_SIZE = 16,   /* buffer queue size */
		FRAME_SIZE = 4,    /* frame size in bytes */
		PERIOD     = 1024  /* frames per period */
	};

	struct Session : Genode::Session
	{
		typedef Packet_stream_policy<Packet_descriptor,
		                             QUEUE_SIZE, QUEUE_SIZE,
		                             float> Policy;

		typedef Packet_stream_tx::Channel<Policy> Channel;

		static const char *service_name() { return "Audio_out"; }

		virtual ~Session() { }

		/**
		 * Request client-side packet-stream interface of channel
		 */
		virtual Channel::Source *stream() { return 0; }

		/**
		 * Flush the audio buffer
		 */
		virtual void flush(void) = 0;

		/**
		 * Set synchronization session
		 *
		 * \param   audio_out_session   synchronization session
		 *
		 * Session can be kept in sync (or bundled) using this function, for
		 * example, the left and right stero channels. A session has exactly
		 * one synchronization session.
		 */
		virtual void sync_session(Session_capability audio_out_session) = 0;


		/*******************
		 ** RPC interface **
		 *******************/

		GENODE_RPC(Rpc_flush, void, flush);
		GENODE_RPC(Rpc_sync_session, void, sync_session, Session_capability);
		GENODE_RPC(Rpc_channel_cap, Genode::Capability<Channel>, channel_cap);

		GENODE_RPC_INTERFACE(Rpc_flush, Rpc_sync_session, Rpc_channel_cap);
	};
}

#endif /* _INCLUDE__AUDIO_OUT_SESSION__AUDIO_OUT_SESSION_H_ */
