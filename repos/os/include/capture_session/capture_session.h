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
	 * Register signal handler informed of new data to capture
	 *
	 * A wakeup signal is delivered only after a call of 'capture_stopped'.
	 */
	virtual void wakeup_sigh(Signal_context_capability) = 0;

	enum class Buffer_result { OK, OUT_OF_RAM, OUT_OF_CAPS };

	struct Buffer_attr
	{
		Area px;  /* buffer area in pixels */
		Area mm;  /* physical size in millimeters */
	};

	/**
	 * Define dimensions of the shared pixel buffer
	 *
	 * The 'size' controls the server-side allocation of the shared pixel
	 * buffer and may affect the screen size of the GUI server.
	 */
	virtual Buffer_result buffer(Buffer_attr) = 0;

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

		void for_each_rect(auto const &fn) const
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
	 *
	 * A client should call 'capture_at' at intervals between 10 to 40 ms
	 * (25-100 FPS). Should no change happen for more than 50 ms, the client
	 * may stop the periodic capturing and call 'capture_stopped' once. As soon
	 * as new changes becomes available for capturing, a wakeup signal tells
	 * the client to resume the periodic capturing.
	 *
	 * The nitpicker GUI server reflects 'capture_at' calls as 'sync' signals
	 * to its GUI clients, which thereby enables applications to synchronize
	 * their output to the display's refresh rate.
	 */
	virtual Affected_rects capture_at(Point) = 0;

	/**
	 * Schedule wakeup signal
	 */
	virtual void capture_stopped() = 0;


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC(Rpc_screen_size, Area, screen_size);
	GENODE_RPC(Rpc_screen_size_sigh, void, screen_size_sigh, Signal_context_capability);
	GENODE_RPC(Rpc_wakeup_sigh, void, wakeup_sigh, Signal_context_capability);
	GENODE_RPC(Rpc_buffer, Buffer_result, buffer, Buffer_attr);
	GENODE_RPC(Rpc_dataspace, Dataspace_capability, dataspace);
	GENODE_RPC(Rpc_capture_at, Affected_rects, capture_at, Point);
	GENODE_RPC(Rpc_capture_stopped, void, capture_stopped);

	GENODE_RPC_INTERFACE(Rpc_screen_size, Rpc_screen_size_sigh, Rpc_wakeup_sigh,
	                     Rpc_buffer, Rpc_dataspace, Rpc_capture_at, Rpc_capture_stopped);
};

#endif /* _INCLUDE__CAPTURE_SESSION__CAPTURE_SESSION_H_ */
