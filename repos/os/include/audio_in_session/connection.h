/*
 * \brief  Connection to Audio_in service
 * \author Josef Soentgen
 * \date   2015-05-08
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
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
	 * \param progress_signal  install progress signal, the client may then
	 *                         call 'wait_for_progress', which is sent when the
	 *                         server processed one or more packets
	 */
	Connection(char const *channel, bool progress_signal = false)
	:
		Genode::Connection<Session>(
			session("ram_quota=%zd, channel=\"%s\"",
			        2*4096 + sizeof(Stream), channel)),
		Session_client(cap(), progress_signal)
	{ }
};

#endif /* _INCLUDE__AUDIO_IN_SESSION__CONNECTION_H_ */
