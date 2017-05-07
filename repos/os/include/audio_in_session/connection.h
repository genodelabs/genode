/*
 * \brief  Connection to Audio_in service
 * \author Josef Soentgen
 * \date   2015-05-08
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__AUDIO_IN_SESSION__CONNECTION_H_
#define _INCLUDE__AUDIO_IN_SESSION__CONNECTION_H_

#include <audio_in_session/client.h>
#include <base/connection.h>
#include <base/allocator.h>

namespace Audio_in { struct Connection; }


struct Audio_in::Connection : Genode::Connection<Session>, Audio_in::Session_client
{
	/**
	 * Issue session request
	 *
	 * \noapi
	 */
	Capability<Audio_in::Session> _session(Genode::Parent &parent, char const *channel)
	{
		return session(parent, "ram_quota=%ld, cap_quota=%ld, channel=\"%s\"",
		               10*1024 + sizeof(Stream), CAP_QUOTA, channel);
	}

	/**
	 * Constructor
	 *
	 * \param progress_signal  install progress signal, the client may then
	 *                         call 'wait_for_progress', which is sent when the
	 *                         server processed one or more packets
	 */
	Connection(Genode::Env &env, char const *channel, bool progress_signal = false)
	:
		Genode::Connection<Session>(env, _session(env.parent(), channel)),
		Session_client(env.rm(), cap(), progress_signal)
	{ }
};

#endif /* _INCLUDE__AUDIO_IN_SESSION__CONNECTION_H_ */
