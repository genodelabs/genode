/*
 * \brief  Client-side capture session interface
 * \author Norman Feske
 * \date   2020-06-26
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__CAPTURE_SESSION__CLIENT_H_
#define _INCLUDE__CAPTURE_SESSION__CLIENT_H_

#include <capture_session/capture_session.h>
#include <base/rpc_client.h>

namespace Capture { struct Session_client; }


struct Capture::Session_client : public Genode::Rpc_client<Session>
{
	/**
	 * Constructor
	 */
	Session_client(Capability<Session> session) : Rpc_client<Session>(session) { }

	Area screen_size() const override { return call<Rpc_screen_size>(); }

	void screen_size_sigh(Signal_context_capability sigh) override
	{
		call<Rpc_screen_size_sigh>(sigh);
	}

	void buffer(Area size) override { call<Rpc_buffer>(size); }

	Dataspace_capability dataspace() override { return call<Rpc_dataspace>(); }

	Affected_rects capture_at(Point pos) override
	{
		return call<Rpc_capture_at>(pos);
	}
};

#endif /* _INCLUDE__CAPTURE_SESSION__CLIENT_H_ */
