/*
 * \brief  Client-side i.MX53 specific framebuffer interface
 * \author Stefan Kalkowski
 * \date   2013-02-26
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__IMX_FRAMEBUFFER_SESSION__CLIENT_H_
#define _INCLUDE__IMX_FRAMEBUFFER_SESSION__CLIENT_H_

#include <framebuffer_session/capability.h>
#include <imx_framebuffer_session/imx_framebuffer_session.h>
#include <base/rpc_client.h>

namespace Framebuffer {

	struct Imx_client : Genode::Rpc_client<Imx_session>
	{
		explicit Imx_client(Capability<Imx_session> session)
		: Genode::Rpc_client<Imx_session>(session) { }

		Genode::Dataspace_capability dataspace() {
			return call<Rpc_dataspace>(); }

		Mode mode() const { return call<Rpc_mode>(); }

		void mode_sigh(Genode::Signal_context_capability sigh) {
			call<Rpc_mode_sigh>(sigh); }

		void refresh(int x, int y, int w, int h) {
			call<Rpc_refresh>(x, y, w, h); }

		void overlay(Genode::addr_t phys_addr, int x, int y, int alpha) {
			call<Rpc_overlay>(phys_addr, x, y, alpha); }
	};
}

#endif /* _INCLUDE__IMX_FRAMEBUFFER_SESSION__CLIENT_H_ */
