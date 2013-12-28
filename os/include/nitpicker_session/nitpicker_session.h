/*
 * \brief  Nitpicker session interface
 * \author Norman Feske
 * \date   2006-08-10
 *
 * A Nitpicker session handles exactly one buffer.
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__NITPICKER_SESSION__NITPICKER_SESSION_H_
#define _INCLUDE__NITPICKER_SESSION__NITPICKER_SESSION_H_

#include <session/session.h>
#include <framebuffer_session/capability.h>
#include <input_session/capability.h>
#include <nitpicker_view/capability.h>

namespace Nitpicker {
	using Genode::size_t;
	struct Session;
}


struct Nitpicker::Session : Genode::Session
{
	static const char *service_name() { return "Nitpicker"; }

	/**
	 * Exception type
	 */
	struct Out_of_metadata : Genode::Exception { };

	virtual ~Session() { }

	/**
	 * Request framebuffer sub-session
	 */
	virtual Framebuffer::Session_capability framebuffer_session() = 0;

	/**
	 * Request input sub-session
	 */
	virtual Input::Session_capability input_session() = 0;

	/**
	 * Create a new view at the buffer
	 *
	 * \return  capability to a new view
	 */
	virtual View_capability create_view() = 0;

	/**
	 * Destroy view
	 */
	virtual void destroy_view(View_capability view) = 0;

	/**
	 * Define view that is used as desktop background
	 */
	virtual int background(View_capability view) = 0;

	/**
	 * Return physical screen mode
	 */
	virtual Framebuffer::Mode mode() = 0;

	/**
	 * Define dimensions of virtual framebuffer
	 *
	 * \throw Out_of_metadata  session quota does not suffice for specified
	 *                         buffer dimensions
	 */
	virtual void buffer(Framebuffer::Mode mode, bool use_alpha) = 0;

	/**
	 * Return number of bytes needed for virtual framebuffer of specified size
	 */
	static size_t ram_quota(Framebuffer::Mode mode, bool use_alpha)
	{
		/*
		 * If alpha blending is used, each pixel requires an additional byte
		 * for the alpha value and a byte holding the input mask.
		 */
		return (mode.bytes_per_pixel() + 2*use_alpha)*mode.width()*mode.height();
	}


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC(Rpc_framebuffer_session, Framebuffer::Session_capability, framebuffer_session);
	GENODE_RPC(Rpc_input_session, Input::Session_capability, input_session);
	GENODE_RPC(Rpc_create_view, View_capability, create_view);
	GENODE_RPC(Rpc_destroy_view, void, destroy_view, View_capability);
	GENODE_RPC(Rpc_background, int, background, View_capability);
	GENODE_RPC(Rpc_mode, Framebuffer::Mode, mode);
	GENODE_RPC_THROW(Rpc_buffer, void, buffer, GENODE_TYPE_LIST(Out_of_metadata),
	                 Framebuffer::Mode, bool);

	GENODE_RPC_INTERFACE(Rpc_framebuffer_session, Rpc_input_session,
	                     Rpc_create_view, Rpc_destroy_view, Rpc_background,
	                     Rpc_mode, Rpc_buffer);
};

#endif /* _INCLUDE__NITPICKER_SESSION__NITPICKER_SESSION_H_ */
