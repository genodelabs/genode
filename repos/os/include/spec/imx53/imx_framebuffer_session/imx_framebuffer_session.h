/*
 * \brief  i.MX53 specific framebuffer session extension
 * \author Stefan Kalkowski
 * \date   2013-02-26
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__IMX_FRAMEBUFFER_SESSION__IMX_FRAMEBUFFER_SESSION_H_
#define _INCLUDE__IMX_FRAMEBUFFER_SESSION__IMX_FRAMEBUFFER_SESSION_H_

#include <base/capability.h>
#include <base/rpc.h>
#include <framebuffer_session/framebuffer_session.h>

namespace Framebuffer { struct Imx_session; }


struct Framebuffer::Imx_session : Session
{
	virtual ~Imx_session() { }

	/**
	 * Set overlay properties
	 *
	 * \param phys_base  physical base address of overlay framebuffer
	 * \param x          horizontal position in pixel
	 * \param y          vertical position in pixel
	 * \param alpha      alpha transparency value of overlay (0-255)
	 */
	virtual void overlay(Genode::addr_t phys_base, int x, int y, int alpha) = 0;


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC(Rpc_overlay, void, overlay, Genode::addr_t, int, int, int);

	GENODE_RPC_INTERFACE_INHERIT(Session, Rpc_overlay);
};

#endif /* _INCLUDE__IMX_FRAMEBUFFER_SESSION__IMX_FRAMEBUFFER_SESSION_H_ */
