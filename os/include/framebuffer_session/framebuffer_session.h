/*
 * \brief  Framebuffer session interface
 * \author Norman Feske
 * \date   2006-07-10
 */

/*
 * Copyright (C) 2006-2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__FRAMEBUFFER_SESSION__FRAMEBUFFER_SESSION_H_
#define _INCLUDE__FRAMEBUFFER_SESSION__FRAMEBUFFER_SESSION_H_

#include <dataspace/capability.h>
#include <session/session.h>

namespace Framebuffer {

	struct Session : Genode::Session
	{
		static const char *service_name() { return "Framebuffer"; }

		/**
		 * Pixel formats
		 */
		enum Mode { INVALID, RGB565 };

		virtual ~Session() { }

		/**
		 * Request dataspace representing the logical frame buffer
		 */
		virtual Genode::Dataspace_capability dataspace() = 0;

		/**
		 * Request current screen mode properties
		 *
		 * \param out_w     width of frame buffer
		 * \param out_h     height of frame buffer
		 * \param out_mode  pixel format
		 */
		virtual void info(int *out_w, int *out_h, Mode *out_mode) = 0;

		/**
		 * Flush specified pixel region
		 *
		 * \param x,y,w,h  region to be updated on physical frame buffer
		 */
		virtual void refresh(int x, int y, int w, int h) = 0;

		/**
		 * Return number of bytes per pixel for a given pixel format
		 */
		static Genode::size_t bytes_per_pixel(Mode mode) { return 2; }


		/*********************
		 ** RPC declaration **
		 *********************/

		GENODE_RPC(Rpc_dataspace, Genode::Dataspace_capability, dataspace);
		GENODE_RPC(Rpc_info, void, info, int *, int *, Mode *);
		GENODE_RPC(Rpc_refresh, void, refresh, int, int, int, int);

		GENODE_RPC_INTERFACE(Rpc_dataspace, Rpc_info, Rpc_refresh);
	};
}

#endif /* _INCLUDE__FRAMEBUFFER_SESSION__FRAMEBUFFER_SESSION_H_ */
