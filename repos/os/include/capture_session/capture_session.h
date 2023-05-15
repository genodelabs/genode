/*
 * \brief  Capture session interface
 * \author Norman Feske
 * \date   2020-06-26
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__CAPTURE_SESSION__CAPTURE_SESSION_H_
#define _INCLUDE__CAPTURE_SESSION__CAPTURE_SESSION_H_

#include <base/signal.h>
#include <session/session.h>
#include <os/surface.h>
#include <os/pixel_rgb888.h>
#include <dataspace/capability.h>

namespace Capture {

	using namespace Genode;

	struct Session_client;
	struct Session;

	using Rect  = Surface_base::Rect;
	using Point = Surface_base::Point;
	using Area  = Surface_base::Area;
	using Pixel = Pixel_rgb888;

}


struct Capture::Session : Genode::Session
{
	/**
	 * \noapi
	 */
	static const char *service_name() { return "Capture"; }

	/*
	 * A capture session consumes a dataspace capability for the server's
	 * session-object allocation, a session capability, and a dataspace
	 * capability for the pixel buffer.
	 */
	static constexpr unsigned CAP_QUOTA = 3;

	/**
	 * Return number of bytes needed for pixel buffer of specified size
	 */
	static size_t buffer_bytes(Area size)
	{
		size_t const bytes_per_pixel = 4;
		return bytes_per_pixel*size.count();
	}

	/**
	 * Request current screen size
	 */
	virtual Area screen_size() const = 0;

	/**
	 * Register signal handler to be notified whenever the screen size changes
	 */
	virtual void screen_size_sigh(Signal_context_capability) = 0;

	/**
	 * Define dimensions of the shared pixel buffer
	 *
	 * The 'size' controls the server-side allocation of the shared pixel
	 * buffer and may affect the screen size of the GUI server. The screen
	 * size is bounding box of the pixel buffers of all capture clients.
	 *
	 * \throw Out_of_ram  session quota does not suffice for specified
	 *                    buffer dimensions
	 * \throw Out_of_caps
	 */
	virtual void buffer(Area size) = 0;

	/**
	 * Request dataspace of the shared pixel buffer defined via 'buffer'
	 */
	virtual Dataspace_capability dataspace() = 0;

	/**
	 * Result type of 'capture_at'
	 *
	 * The geometry information are relative to the viewport specified for the
	 * 'capture_at' call.
	 */
	struct Affected_rects
	{
		enum { NUM_RECTS = 3U };

		Rect rects[NUM_RECTS];

		template <typename FN>
		void for_each_rect(FN const &fn) const
		{
			for (unsigned i = 0; i < NUM_RECTS; i++)
				if (rects[i].valid())
					fn(rects[i]);
		}
	};

	/**
	 * Update the pixel-buffer with content at the specified screen position
	 *
	 * \return  geometry information about the content that changed since the
	 *          previous call of 'capture_at'
	 */
	virtual Affected_rects capture_at(Point) = 0;


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC(Rpc_screen_size, Area, screen_size);
	GENODE_RPC(Rpc_screen_size_sigh, void, screen_size_sigh, Signal_context_capability);
	GENODE_RPC_THROW(Rpc_buffer, void, buffer,
	                 GENODE_TYPE_LIST(Out_of_ram, Out_of_caps), Area);
	GENODE_RPC(Rpc_dataspace, Dataspace_capability, dataspace);
	GENODE_RPC(Rpc_capture_at, Affected_rects, capture_at, Point);

	GENODE_RPC_INTERFACE(Rpc_screen_size, Rpc_screen_size_sigh, Rpc_buffer,
	                     Rpc_dataspace, Rpc_capture_at);
};

#endif /* _INCLUDE__CAPTURE_SESSION__CAPTURE_SESSION_H_ */
