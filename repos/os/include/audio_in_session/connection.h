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
	 * Constructor
	 *
	 * \param channel          channel identifier (e.g., "left")
	 * \param label            optional session label
	 * \param progress_signal  install progress signal, the client may then
	 *                         call 'wait_for_progress', which is sent when the
	 *                         server processed one or more packets
	 */
	Connection(Genode::Env &env, char const *channel,
	           Label const &label = Label(), bool progress_signal = false)
	:
		Genode::Connection<Session>(env, label,
		                            Ram_quota { 10*1024 + sizeof(Stream) },
		                            Args("channel=\"", channel, "\"")),
		Session_client(env.rm(), cap(), progress_signal)
	{ }
};

#endif /* _INCLUDE__AUDIO_IN_SESSION__CONNECTION_H_ */
