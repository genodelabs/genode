/*
 * \brief  Client-side framebuffer interface
 * \author Norman Feske
 * \date   2006-07-10
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__FRAMEBUFFER_SESSION__CLIENT_H_
#define _INCLUDE__FRAMEBUFFER_SESSION__CLIENT_H_

#include <framebuffer_session/capability.h>
#include <base/rpc_client.h>

namespace Framebuffer {

	struct Session_client : Genode::Rpc_client<Session>
	{
		explicit Session_client(Session_capability session)
		: Genode::Rpc_client<Session>(session) { }

		Genode::Dataspace_capability dataspace() {
			return call<Rpc_dataspace>(); }

		void info(int *out_w, int *out_h, Mode *out_mode) {
			call<Rpc_info>(out_w, out_h, out_mode); }

		void refresh(int x, int y, int w, int h) {
			call<Rpc_refresh>(x, y, w, h); }
	};
}

#endif /* _INCLUDE__FRAMEBUFFER_SESSION__CLIENT_H_ */
