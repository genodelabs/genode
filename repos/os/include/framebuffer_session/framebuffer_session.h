/*
 * \brief  Framebuffer session interface
 * \author Norman Feske
 * \date   2006-07-10
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__FRAMEBUFFER_SESSION__FRAMEBUFFER_SESSION_H_
#define _INCLUDE__FRAMEBUFFER_SESSION__FRAMEBUFFER_SESSION_H_

#include <base/output.h>
#include <base/signal.h>
#include <dataspace/capability.h>
#include <session/session.h>

namespace Framebuffer {

	struct Mode;
	struct Session;
	struct Session_client;
}


/**
 * Framebuffer mode info as returned by 'Framebuffer::Session::mode()'
 */
struct Framebuffer::Mode
{
	public:

		/**
		 * Pixel formats
		 */
		enum Format { INVALID, RGB565 };

		static Genode::size_t bytes_per_pixel(Format format)
		{
			if (format == RGB565) return 2;
			return 0;
		}

	private:

		int _width = 0, _height = 0;

		Format _format = INVALID;

		/*
		 * Helpers for sanitized access. The sanitizing is needed whenever
		 * a 'Mode' object is transferred via RPC from an untrusted client.
		 */
		static Format _sanitized(Format f) { return f == RGB565 ? RGB565 : INVALID; }
		static int    _sanitized(int v)    { return v >= 0 ? v : 0; }

	public:

		Mode() { }

		Mode(int width, int height, Format format)
		: _width(width), _height(height), _format(format) { }

		int    width()  const { return _sanitized(_width);  }
		int    height() const { return _sanitized(_height); }
		Format format() const { return _sanitized(_format); }

		/**
		 * Return number of bytes per pixel
		 */
		Genode::size_t bytes_per_pixel() const {
			return bytes_per_pixel(format()); }

		void print(Genode::Output &out) const
		{
			Genode::print(out, width(), "x", height(), "@");
			switch (format()) {
			case RGB565: Genode::print(out, "RGB565");  break;
			default:     Genode::print(out, "INVALID"); break;
			}
		}
};


struct Framebuffer::Session : Genode::Session
{
	/**
	 * \noapi
	 */
	static const char *service_name() { return "Framebuffer"; }

	/*
	 * A framebuffer session consumes a dataspace capability for the server's
	 * session-object allocation, a dataspace capability for the framebuffer
	 * dataspace, and its session capability.
	 */
	enum { CAP_QUOTA = 3 };

	typedef Session_client Client;

	virtual ~Session() { }

	/**
	 * Request dataspace representing the logical frame buffer
	 *
	 * By calling this method, the framebuffer client enables the server
	 * to reallocate the framebuffer dataspace (e.g., on mode changes).
	 * Hence, prior calling this method, the client should make sure to
	 * have detached the previously requested dataspace from its local
	 * address space.
	 */
	virtual Genode::Dataspace_capability dataspace() = 0;

	/**
	 * Request display-mode properties of the framebuffer ready to be
	 * obtained via the 'dataspace()' method
	 */
	virtual Mode mode() const = 0;

	/**
	 * Register signal handler to be notified on mode changes
	 *
	 * The framebuffer server may support changing the display mode on the
	 * fly. For example, a virtual framebuffer presented in a window may
	 * get resized according to the window dimensions. By installing a
	 * signal handler for mode changes, the framebuffer client can respond
	 * to such changes. The new mode can be obtained using the 'mode()'
	 * method. However, from the client's perspective, the original mode
	 * stays in effect until the it calls 'dataspace()' again.
	 */
	virtual void mode_sigh(Genode::Signal_context_capability sigh) = 0;

	/**
	 * Flush specified pixel region
	 *
	 * \param x,y,w,h  region to be updated on physical frame buffer
	 */
	virtual void refresh(int x, int y, int w, int h) = 0;

	/**
	 * Register signal handler for refresh synchronization
	 */
	virtual void sync_sigh(Genode::Signal_context_capability) = 0;


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC(Rpc_dataspace, Genode::Dataspace_capability, dataspace);
	GENODE_RPC(Rpc_mode, Mode, mode);
	GENODE_RPC(Rpc_refresh, void, refresh, int, int, int, int);
	GENODE_RPC(Rpc_mode_sigh, void, mode_sigh, Genode::Signal_context_capability);
	GENODE_RPC(Rpc_sync_sigh, void, sync_sigh, Genode::Signal_context_capability);

	GENODE_RPC_INTERFACE(Rpc_dataspace, Rpc_mode, Rpc_mode_sigh, Rpc_refresh,
	                     Rpc_sync_sigh);
};

#endif /* _INCLUDE__FRAMEBUFFER_SESSION__FRAMEBUFFER_SESSION_H_ */
