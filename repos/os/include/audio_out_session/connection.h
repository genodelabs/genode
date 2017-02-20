/*
 * \brief  Connection to audio service
 * \author Sebastian Sumpf
 * \date   2012-12-20
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__AUDIO_OUT_SESSION__CONNECTION_H_
#define _INCLUDE__AUDIO_OUT_SESSION__CONNECTION_H_

#include <audio_out_session/client.h>
#include <base/connection.h>
#include <base/allocator.h>

namespace Audio_out { struct Connection; }


struct Audio_out::Connection : Genode::Connection<Session>, Audio_out::Session_client
{
	/**
	 * Issue session request
	 *
	 * \noapi
	 */
	Capability<Audio_out::Session> _session(Genode::Parent &parent, char const *channel)
	{
		return session(parent, "ram_quota=%ld, channel=\"%s\"",
		               2*4096 + 2048 + sizeof(Stream), channel);
	}

	/**
	 * Constructor
	 *
	 * \param channel          channel identifier (e.g., "front left")
	 * \param alloc_signal     install 'alloc_signal', the client may then use
	 *                         'wait_for_alloc' when the stream is full
	 * \param progress_signal  install progress signal, the client may then
	 *                         call 'wait_for_progress', which is sent when the
	 *                         server processed one or more packets
	 */
	Connection(Genode::Env &env,
	           char const  *channel,
	           bool         alloc_signal = true,
	           bool         progress_signal = false)
	:
		Genode::Connection<Session>(env, _session(env.parent(), channel)),
		Session_client(env.rm(), cap(), alloc_signal, progress_signal)
	{ }

	/**
	 * Constructor
	 *
	 * \noapi
	 * \deprecated  Use the constructor with 'Env &' as first
	 *              argument instead
	 */
	Connection(const char *channel,
	           bool        alloc_signal = true,
	           bool        progress_signal = false) __attribute__((deprecated))
	:
		Genode::Connection<Session>(_session(*Genode::env_deprecated()->parent(), channel)),
		Session_client(*Genode::env_deprecated()->rm_session(), cap(), alloc_signal, progress_signal)
	{ }
};

#endif /* _INCLUDE__AUDIO_OUT_SESSION__CONNECTION_H_ */
