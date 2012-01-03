/*
 * \brief  Connection to audio-out service
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Christian Helmuth
 * \date   2009-12-01
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__AUDIO_OUT_SESSION__CONNECTION_H_
#define _INCLUDE__AUDIO_OUT_SESSION__CONNECTION_H_

#include <audio_out_session/client.h>
#include <base/connection.h>
#include <base/allocator.h>

namespace Audio_out {

	struct Connection : Genode::Connection<Session>, Session_client
	{
		/**
		 * Constructor
		 *
		 * \param channel       channel identifier (e.g., "front left")
		 * \param buffer_alloc  allocator used for managing the
		 *                      transmission buffer
		 * \param buffer_size   size of transmission buffer in bytes
		 *                      (defaults to all packets of the queue plus
		 *                      some space for metadata)
		 */
		Connection(const char              *channel,
		           Genode::Range_allocator *buffer_alloc,
		           Genode::size_t           buffer_size = QUEUE_SIZE * FRAME_SIZE * PERIOD + 0x400)
		:
			Genode::Connection<Session>(
				session("ram_quota=%zd, channel=\"%s\", buffer_size=%zd",
				        3*4096 + buffer_size, channel, buffer_size)),
			Session_client(cap(), buffer_alloc)
		{ }
	};
}

#endif /* _INCLUDE__AUDIO_OUT_SESSION__CONNECTION_H_ */
